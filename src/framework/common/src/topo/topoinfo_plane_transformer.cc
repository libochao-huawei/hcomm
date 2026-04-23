/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "topoinfo_plane_transformer.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common.h"
#include "log.h"
#include "plane_transformer/plane_transformer_factory.h"
#include "sal_pub.h"

namespace hccl {
namespace {
constexpr u32 DEFAULT_NET_PLANE_ID = 0;
constexpr u32 DEFAULT_NET_PLANE_NUM = 1;
constexpr double STATIC_DELAY = 10;
constexpr double TRANSMIT_DELAY = 5;
using RankIdentity = std::pair<std::string, s32>;

/**
 * @brief 为 rank 构造稳定身份键，用于校验调用方传入的 `rankIds/subRankList` 是否同序。
 * @param rankInfo 输入 rank 信息。
 * @return RankIdentity
 */
RankIdentity BuildRankIdentity(const RankInfo_t &rankInfo)
{
    return std::make_pair(rankInfo.serverId, rankInfo.deviceInfo.devicePhyId);
}

/**
 * @brief 把子通信域 rank 的平面信息恢复为默认单平面状态。
 * @param subRankList 子通信域 rank 列表。
 */
void ResetPlaneInfo(std::vector<RankInfo_t> &subRankList)
{
    for (auto &rankInfo : subRankList) {
        rankInfo.netPlaneId = DEFAULT_NET_PLANE_ID;
        rankInfo.netPlaneNum = DEFAULT_NET_PLANE_NUM;
        rankInfo.groupId = 0;
    }
}

/**
 * @brief 尝试提取 rank 的主平面标识。
 * @param rankInfo 输入 rank 信息。
 * @param planeToken 输出平面标识。
 * @return true 表示成功提取到 plane token。
 * @return false 表示当前 rank 未携带 OXC plane token。
 *
 * @note 这里不把“缺失 plane token”视为错误，而是把它当成“非 OXC/未启用平面拆分”的
 *       合法退化场景，由外层逻辑继续保持默认单平面结果。
 */
bool TryGetPrimaryPlaneToken(const RankInfo_t &rankInfo, std::string &planeToken)
{
    /**
     * @brief 当前 OXC 并行平面语义优先绑定到 L2 / OXC_Mesh。
     *
     * @note 文档语义中真正代表 OXC 超平面的，是 `net_layer == 2 && net_type == "OXC_Mesh"`。
     *       因此这里先只在 L2 上找 plane token，避免误把 L0/L1/L3 的 plane_id 当成
     *       netPlane 分组依据。如果输入并非 OXC 2.0 完整结构，则回退到旧逻辑，维持兼容。
     */
    for (const auto &levelInfo : rankInfo.levelList) {
        if (levelInfo.netLayer != 2 || levelInfo.netType != "OXC_Mesh") {
            continue;
        }
        for (const auto &addrInfo : levelInfo.rankAddrList) {
            if (!addrInfo.planeId.empty()) {
                planeToken = addrInfo.planeId;
                return true;
            }
        }
    }

    for (const auto &levelInfo : rankInfo.levelList) {
        for (const auto &addrInfo : levelInfo.rankAddrList) {
            if (!addrInfo.planeId.empty()) {
                planeToken = addrInfo.planeId;
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 为全局 rank 列表构造稳定的 plane-group 映射。
 * @param globalRankList 全局通信域 rank 列表。
 * @param globalGroupIds 输出：按全局 rank 下标存放的 groupId。
 * @param hasPlaneToken 输出：是否至少发现一个 OXC plane token。
 */
void BuildGlobalPlaneGroupIds(const std::vector<RankInfo_t> &globalRankList, std::vector<u32> &globalGroupIds,
    bool &hasPlaneToken)
{
    globalGroupIds.assign(globalRankList.size(), 0);
    hasPlaneToken = false;

    std::unordered_map<std::string, u32> tokenToGroupId;
    for (u32 globalRankId = 0; globalRankId < globalRankList.size(); ++globalRankId) {
        std::string planeToken;
        if (!TryGetPrimaryPlaneToken(globalRankList[globalRankId], planeToken)) {
            continue;
        }

        hasPlaneToken = true;
        auto emplaceResult = tokenToGroupId.emplace(planeToken, static_cast<u32>(tokenToGroupId.size()));
        globalGroupIds[globalRankId] = emplaceResult.first->second;
    }
}

/**
 * @brief 把全局 plane-group 映射写回到子通信域 rank 列表。
 * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
 * @param globalGroupIds 全局通信域按 rank 下标展开的 groupId 映射。
 * @param subRankList 子通信域 rank 列表。
 */
void ApplyGroupIdsToSubRankList(const std::vector<u32> &rankIds, const std::vector<u32> &globalGroupIds,
    std::vector<RankInfo_t> &subRankList)
{
    for (u32 i = 0; i < rankIds.size(); ++i) {
        subRankList[i].groupId = globalGroupIds[rankIds[i]];
    }
}

/**
 * @brief 校验 `rankIds` 与 `subRankList` 是否同序引用同一批全局 rank。
 * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
 * @param globalRankList 全局通信域 rank 列表。
 * @param subRankList 子通信域 rank 列表。
 * @return HcclResult
 */
HcclResult ValidatePlaneInput(const std::vector<u32> &rankIds, const std::vector<RankInfo_t> &globalRankList,
    const std::vector<RankInfo_t> &subRankList)
{
    CHK_PRT_RET(rankIds.size() != subRankList.size(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ValidatePlaneInput] rankIdsSize[%zu] != subRankSize[%zu].",
            rankIds.size(), subRankList.size()), HCCL_E_PARA);
    CHK_PRT_RET(!rankIds.empty() && globalRankList.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ValidatePlaneInput] globalRankList is empty."),
        HCCL_E_PARA);

    for (u32 i = 0; i < rankIds.size(); ++i) {
        const u32 globalRankId = rankIds[i];
        CHK_PRT_RET(globalRankId >= globalRankList.size(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ValidatePlaneInput] rankIds[%u]=%u out of range[%zu].",
                i, globalRankId, globalRankList.size()), HCCL_E_PARA);
        CHK_PRT_RET(BuildRankIdentity(subRankList[i]) != BuildRankIdentity(globalRankList[globalRankId]),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ValidatePlaneInput] rankIds[%u]=%u mismatch with sub rank[%u].",
                i, globalRankId, subRankList[i].rankId), HCCL_E_PARA);
    }

    return HCCL_SUCCESS;
}

/**
 * @brief 判断子通信域 rankId 是否呈严格递增的等差数列。
 * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
 * @return true 表示均匀平面。
 * @return false 表示非均匀平面，需要退化为默认单平面。
 */
bool IsUniformPlane(const std::vector<u32> &rankIds)
{
    if (rankIds.size() <= 1) {
        return true;
    }

    if (rankIds[1] <= rankIds[0]) {
        return false;
    }
    const u32 offset = rankIds[1] - rankIds[0];
    for (u32 i = 2; i < rankIds.size(); ++i) {
        if (rankIds[i] <= rankIds[i - 1] || rankIds[i] - rankIds[i - 1] != offset) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 校验把 rank 沿某一方向平移 offset 后，是否仍停留在同一 plane-group。
 * @param offset 平移偏移量。
 * @param addDirection `true` 表示加方向平移，`false` 表示减方向平移。
 * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
 * @param globalGroupIds 全局通信域按 rank 下标展开的 groupId 映射。
 * @return true 表示平移后仍保持同组。
 * @return false 表示会越界或跨组。
 */
bool VerifySameGroup(const u32 offset, const bool addDirection, const std::vector<u32> &rankIds,
    const std::vector<u32> &globalGroupIds)
{
    const u32 maxGlobalRankId = static_cast<u32>(globalGroupIds.size() - 1);
    for (const auto globalRankId : rankIds) {
        if (addDirection) {
            if (globalRankId > maxGlobalRankId - offset) {
                return false;
            }
        } else if (globalRankId < offset) {
            return false;
        }

        const u32 shiftedRankId = addDirection ? (globalRankId + offset) : (globalRankId - offset);
        if (globalGroupIds[shiftedRankId] != globalGroupIds[globalRankId]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 计算在某一方向上可连续扩展的最大同组偏移量。
 * @param maxDelta 当前方向上的理论最大偏移量。
 * @param addDirection `true` 表示加方向平移，`false` 表示减方向平移。
 * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
 * @param globalGroupIds 全局通信域按 rank 下标展开的 groupId 映射。
 * @return u32
 */
u32 CalcValidOffset(const u32 maxDelta, const bool addDirection, const std::vector<u32> &rankIds,
    const std::vector<u32> &globalGroupIds)
{
    u32 offset = 0;
    while (offset < maxDelta) {
        if (!VerifySameGroup(offset + 1, addDirection, rankIds, globalGroupIds)) {
            break;
        }
        ++offset;
    }
    return offset;
}

/**
 * @brief 统计当前子通信域中，首个 server 上参与通信的设备数量。
 * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
 * @param globalRankList 全局通信域 rank 列表。
 * @return u32
 *
 * @note 该统计沿用 task_0 reference 的“首 server 内设备并行度”语义，
 *       同时忽略 host rank，避免 host-only 场景误参与机内并行计算。
 */
u32 CountNpusInSameServer(const std::vector<u32> &rankIds, const std::vector<RankInfo_t> &globalRankList)
{
    if (rankIds.empty()) {
        return 0;
    }

    const std::string &refServerId = globalRankList[rankIds[0]].serverId;
    return static_cast<u32>(std::count_if(rankIds.begin(), rankIds.end(), [&](const u32 globalRankId) {
        const auto &rankInfo = globalRankList[globalRankId];
        return rankInfo.serverId == refServerId && rankInfo.deviceInfo.devicePhyId != HOST_DEVICE_ID;
    }));
}

/**
 * @brief 按 task_0 reference 语义计算机内同号卡并行，并把结果写回到 `subRankList`。
 * @param parallelCommId 当前子通信域对应的并行通信域 ID。
 * @param parallelCommNum 当前子通信域所属并行通信域总数。
 * @param npuPerServer 每个 server 参与平行扩展的设备数量。
 * @param subRankList 子通信域 rank 列表。
 * @param netPlaneNum 输出：总并行平面数量。
 */
void CalcIntraServerParallel(const u32 parallelCommId, const u32 parallelCommNum, const u32 npuPerServer,
    std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    std::unordered_map<s32, u32> npuMapping;
    u32 localRankCounter = 0;
    netPlaneNum = parallelCommNum * npuPerServer;

    for (auto &rankInfo : subRankList) {
        const s32 phyId = rankInfo.deviceInfo.devicePhyId;
        auto emplaceResult = npuMapping.emplace(phyId, localRankCounter);
        if (emplaceResult.second) {
            ++localRankCounter;
        }
        rankInfo.netPlaneId = parallelCommId * npuPerServer + emplaceResult.first->second;
        rankInfo.netPlaneNum = netPlaneNum;
    }
}

/**
 * @brief 基于预计算的全局 plane-group 映射执行均匀平面解析。
 * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
 * @param globalRankList 全局通信域 rank 列表。
 * @param globalGroupIds 全局通信域按 rank 下标展开的 groupId 映射。
 * @param subRankList 子通信域 rank 列表。
 * @param netPlaneNum 输出：总并行平面数量。
 * @return HcclResult
 */
HcclResult ParseUniformPlaneByGroups(const std::vector<u32> &rankIds, const std::vector<RankInfo_t> &globalRankList,
    const std::vector<u32> &globalGroupIds, std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    CHK_PRT_RET(globalGroupIds.size() != globalRankList.size(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlaneByGroups] globalGroupIdsSize[%zu] != globalRankSize[%zu].",
            globalGroupIds.size(), globalRankList.size()), HCCL_E_PARA);

    const u32 rankNum = static_cast<u32>(rankIds.size());
    const u32 maxGlobalRankId = static_cast<u32>(globalRankList.size() - 1);
    u32 maxDeltaLeft = rankIds.front();
    u32 maxDeltaRight = maxGlobalRankId - rankIds.back();
    for (u32 i = 1; i < rankNum; ++i) {
        CHK_PRT_RET(rankIds[i] <= rankIds[i - 1],
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlaneByGroups] rankIds is not strictly increasing at index[%u].",
                i), HCCL_E_PARA);
        const u32 delta = rankIds[i] - rankIds[i - 1] - 1;
        maxDeltaLeft = std::min(maxDeltaLeft, delta);
        maxDeltaRight = std::min(maxDeltaRight, delta);
    }

    const u32 leftOffset = CalcValidOffset(maxDeltaLeft, false, rankIds, globalGroupIds);
    const u32 rightOffset = CalcValidOffset(maxDeltaRight, true, rankIds, globalGroupIds);
    const u32 parallelCommId = leftOffset;
    const u32 parallelCommNum = leftOffset + rightOffset + 1;
    const u32 npuPerServer = CountNpusInSameServer(rankIds, globalRankList);
    CHK_PRT_RET(npuPerServer == 0,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlaneByGroups] npuPerServer is zero."),
        HCCL_E_PARA);

    CalcIntraServerParallel(parallelCommId, parallelCommNum, npuPerServer, subRankList, netPlaneNum);
    HCCL_INFO("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlaneByGroups] rankNum[%u] parallelCommId[%u] parallelCommNum[%u] npuPerServer[%u] netPlaneNum[%u].",
        rankNum, parallelCommId, parallelCommNum, npuPerServer, netPlaneNum);
    return HCCL_SUCCESS;
}

/**
 * @brief 按对称组大小重新切分输入分组。
 * @param symmSize 目标对称组大小。
 * @param groups 输入输出：待重构分组。
 */
void ApplyGroupsRefactor(u32 symmSize, std::vector<std::vector<u32>> &groups)
{
    std::vector<std::vector<u32>> newGroups;
    for (const auto &group : groups) {
        std::vector<u32> tmpGroup;
        for (const auto &elem : group) {
            tmpGroup.push_back(elem);
            if (tmpGroup.size() == symmSize) {
                newGroups.push_back(tmpGroup);
                tmpGroup.clear();
            }
        }
        if (!tmpGroup.empty()) {
            newGroups.push_back(tmpGroup);
        }
    }
    groups = newGroups;
}

/**
 * @brief 根据环境变量解析 AHC 算法类型。
 * @param envName 环境变量名。
 * @param defaultType 默认算法。
 * @return HcclAlgoType
 */
HcclAlgoType ParseEnvAlgoType(const char *envName, HcclAlgoType defaultType)
{
    const std::string selectedAlg = SalGetEnv(envName);
    if (selectedAlg == "ring") {
        return HcclAlgoType::HCCL_ALGO_TYPE_RING;
    }
    if (selectedAlg == "HD") {
        return HcclAlgoType::HCCL_ALGO_TYPE_HDR;
    }
    if (selectedAlg == "NB") {
        return HcclAlgoType::HCCL_ALGO_TYPE_NB;
    }
    if (selectedAlg == "NHR") {
        return HcclAlgoType::HCCL_ALGO_TYPE_NHR;
    }
    if (selectedAlg == "MTR") {
        return HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    }
    return defaultType;
}

namespace ApplyTransformUtils {
std::pair<bool, u32> FindTargetRow(const std::vector<std::vector<u32>> &groups, u32 localIndex)
{
    for (const auto &group : groups) {
        auto it = std::find(group.begin(), group.end(), localIndex);
        if (it != group.end()) {
            return {true, static_cast<u32>(std::distance(group.begin(), it))};
        }
    }
    return {false, 0};
}

std::vector<u32> ComputeMetaPlaneIds(u32 targetRowId, u32 netPlaneId, u32 metaPlaneNum, u32 symGroupSize)
{
    std::vector<u32> metaPlaneIds(symGroupSize);
    if (symGroupSize == 0 || metaPlaneNum == 0) {
        return metaPlaneIds;
    }

    metaPlaneIds[targetRowId] = netPlaneId % metaPlaneNum;
    for (s32 row = static_cast<s32>(targetRowId) - 1; row >= 0; --row) {
        metaPlaneIds[static_cast<u32>(row)] =
            (metaPlaneIds[static_cast<u32>(row) + 1] + metaPlaneNum - 1) % metaPlaneNum;
    }
    for (u32 row = targetRowId + 1; row < symGroupSize; ++row) {
        metaPlaneIds[row] = (metaPlaneIds[row - 1] + 1) % metaPlaneNum;
    }
    return metaPlaneIds;
}

void ProcessSingleRow(u32 row, const std::vector<u32> &metaPlaneIds, const std::vector<std::vector<u32>> &groups,
    const std::vector<std::vector<u32>> &rankMapping, std::vector<u32> &indexList)
{
    const u32 metaPlaneId = metaPlaneIds[row];
    const auto &mapping = rankMapping[metaPlaneId];
    const u32 numNodes = groups.size();

    std::vector<u32> rowIndexList;
    rowIndexList.reserve(numNodes);
    for (const auto &group : groups) {
        rowIndexList.push_back(group[row]);
    }

    for (u32 i = 0; i < numNodes; ++i) {
        indexList[rowIndexList[i]] = rowIndexList[mapping[i]];
    }
}
}  // namespace ApplyTransformUtils
}  // namespace

HcclResult TopoinfoPlaneTransformer::ParsePlane(const RankTable_t &globalRankTable, const RankTable_t &subRankTable,
    const u32 subCommRankId, u32 &netPlaneId, u32 &netPlaneNum)
{
    netPlaneId = DEFAULT_NET_PLANE_ID;
    netPlaneNum = DEFAULT_NET_PLANE_NUM;

    CHK_PRT_RET(globalRankTable.rankList.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlaneCompat] global rankList is empty."),
        HCCL_E_PARA);
    CHK_PRT_RET(subRankTable.rankList.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlaneCompat] sub rankList is empty."),
        HCCL_E_PARA);
    CHK_PRT_RET(subCommRankId >= subRankTable.rankList.size(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlaneCompat] subCommRankId[%u] out of range[%zu].",
            subCommRankId, subRankTable.rankList.size()), HCCL_E_PARA);

    if (subRankTable.rankList.size() <= 1) {
        return HCCL_SUCCESS;
    }

    std::map<RankIdentity, u32> rankIdMap;
    for (const auto &globalRankInfo : globalRankTable.rankList) {
        rankIdMap[BuildRankIdentity(globalRankInfo)] = globalRankInfo.rankId;
    }

    std::vector<u32> rankIds;
    rankIds.reserve(subRankTable.rankList.size());
    std::vector<RankInfo_t> subRankList = subRankTable.rankList;
    for (const auto &subRankInfo : subRankTable.rankList) {
        auto iter = rankIdMap.find(BuildRankIdentity(subRankInfo));
        CHK_PRT_RET(iter == rankIdMap.end(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlaneCompat] failed to map sub rank[%u] back to global rank.",
                subRankInfo.rankId), HCCL_E_PARA);
        rankIds.push_back(iter->second);
    }

    CHK_RET(ParsePlane(rankIds, globalRankTable.rankList, subRankList, netPlaneNum));
    netPlaneId = subRankList[subCommRankId].netPlaneId;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::ParsePlane(const u32 rankNum, const u32 *rankIds,
    const std::vector<RankInfo_t> &globalRankList, std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    netPlaneNum = DEFAULT_NET_PLANE_NUM;

    CHK_PRT_RET(rankNum != subRankList.size(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlane] rankNum[%u] != subRankSize[%zu].",
            rankNum, subRankList.size()), HCCL_E_PARA);
    CHK_PRT_RET(rankNum > 0 && rankIds == nullptr,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlane] rankIds is null when rankNum[%u] > 0.",
            rankNum), HCCL_E_PARA);

    std::vector<u32> newRankIds;
    newRankIds.reserve(rankNum);
    for (u32 i = 0; i < rankNum; ++i) {
        newRankIds.push_back(rankIds[i]);
    }
    return ParsePlane(newRankIds, globalRankList, subRankList, netPlaneNum);
}

HcclResult TopoinfoPlaneTransformer::ParsePlane(const std::vector<u32> &rankIds,
    const std::vector<RankInfo_t> &globalRankList, std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    netPlaneNum = DEFAULT_NET_PLANE_NUM;
    ResetPlaneInfo(subRankList);
    CHK_RET(ValidatePlaneInput(rankIds, globalRankList, subRankList));

    if (rankIds.size() <= 1) {
        return HCCL_SUCCESS;
    }

    std::vector<u32> globalGroupIds;
    bool hasPlaneToken = false;
    BuildGlobalPlaneGroupIds(globalRankList, globalGroupIds, hasPlaneToken);
    ApplyGroupIdsToSubRankList(rankIds, globalGroupIds, subRankList);

    if (!hasPlaneToken) {
        HCCL_INFO("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlane] no OXC plane token detected, keep default single plane.");
        return HCCL_SUCCESS;
    }

    if (!IsUniformPlane(rankIds)) {
        HCCL_INFO("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlane] non-uniform rankIds detected, keep default single plane.");
        return HCCL_SUCCESS;
    }

    return ParseUniformPlaneByGroups(rankIds, globalRankList, globalGroupIds, subRankList, netPlaneNum);
}

HcclResult TopoinfoPlaneTransformer::ParseUniformPlane(const std::vector<u32> &rankIds,
    const std::vector<RankInfo_t> &globalRankList, std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    std::vector<u32> globalGroupIds;
    bool hasPlaneToken = false;
    BuildGlobalPlaneGroupIds(globalRankList, globalGroupIds, hasPlaneToken);
    CHK_PRT_RET(!hasPlaneToken,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlane] no OXC plane token found in global rank list."),
        HCCL_E_PARA);
    return ParseUniformPlaneByGroups(rankIds, globalRankList, globalGroupIds, subRankList, netPlaneNum);
}

u32 TopoinfoPlaneTransformer::CalcSymGroupSize(const std::vector<std::vector<u32>> &groups)
{
    if (groups.empty()) {
        return 0;
    }

    u32 symGroupSize = static_cast<u32>(groups.front().size());
    for (const auto &group : groups) {
        symGroupSize = std::min(symGroupSize, static_cast<u32>(group.size()));
    }
    return symGroupSize;
}

HcclResult TopoinfoPlaneTransformer::RegroupAndSelectAlgo(const u32 netPlaneNum, const bool enabledBroke,
    std::vector<std::vector<u32>> &groups, HcclAlgoType &intraAlgType, HcclAlgoType &interAlgType)
{
    CHK_PRT_RET(groups.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][RegroupAndSelectAlgo] groups is empty."),
        HCCL_E_PARA);

    std::vector<u32> groupSizes;
    groupSizes.reserve(groups.size());
    for (const auto &group : groups) {
        CHK_PRT_RET(group.empty(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][RegroupAndSelectAlgo] group is empty."),
            HCCL_E_PARA);
        groupSizes.push_back(static_cast<u32>(group.size()));
    }

    u32 bestSymmSize = 1;
    if (enabledBroke) {
        u32 sumGroupSize = groupSizes.front();
        u32 minGroupSize = groupSizes.front();
        for (u32 i = 1; i < groupSizes.size(); ++i) {
            minGroupSize = std::min(minGroupSize, groupSizes[i]);
            sumGroupSize += groupSizes[i];
        }

        double minCost = sumGroupSize * STATIC_DELAY;
        for (u32 symmSize = 1; symmSize <= minGroupSize; ++symmSize) {
            double maxBrokeRate = 0;
            u32 newGroupNum = 0;
            for (const u32 size : groupSizes) {
                const double brokeRate = static_cast<double>(symmSize + (size % symmSize) - 1) / symmSize;
                maxBrokeRate = std::max(maxBrokeRate, brokeRate);
                newGroupNum += size / symmSize;
            }
            const double cost = (symmSize + newGroupNum) * STATIC_DELAY + maxBrokeRate * TRANSMIT_DELAY;
            if (cost < minCost) {
                bestSymmSize = symmSize;
                minCost = cost;
            }
        }
    } else {
        bestSymmSize = groupSizes.front();
        for (u32 i = 1; i < groupSizes.size(); ++i) {
            bestSymmSize = std::__gcd(bestSymmSize, groupSizes[i]);
        }
    }

    ApplyGroupsRefactor(bestSymmSize, groups);
    intraAlgType = ParseEnvAlgoType("HCCL_AHC_ALGO_INTRA", HcclAlgoType::HCCL_ALGO_TYPE_RING);
    interAlgType = ParseEnvAlgoType("HCCL_AHC_ALGO_INTER", HcclAlgoType::HCCL_ALGO_TYPE_RING);

    HCCL_INFO("[OXC_HCOMM][TopoinfoPlaneTransformer][RegroupAndSelectAlgo] netPlaneNum[%u] enabledBroke[%u] bestSymmSize[%u] intraAlgType[%u] interAlgType[%u].",
        netPlaneNum, static_cast<u32>(enabledBroke), bestSymmSize, static_cast<u32>(intraAlgType),
        static_cast<u32>(interAlgType));
    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::ReparseGroupedPlane(const u32 localIndex,
    const std::vector<std::vector<u32>> &groups, u32 &netPlaneId, u32 &netPlaneNum)
{
    CHK_PRT_RET(groups.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ReparseGroupedPlane] groups is empty."),
        HCCL_E_PARA);

    const u32 symGroupSize = CalcSymGroupSize(groups);
    CHK_PRT_RET(symGroupSize == 0,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ReparseGroupedPlane] symGroupSize is zero."),
        HCCL_E_PARA);

    u32 rankOfGroup = 0;
    bool isFound = false;
    for (const auto &group : groups) {
        for (u32 i = 0; i < group.size(); ++i) {
            if (group[i] == localIndex) {
                rankOfGroup = std::min(i, symGroupSize - 1);
                isFound = true;
                break;
            }
        }
        if (isFound) {
            break;
        }
    }

    CHK_PRT_RET(!isFound,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ReparseGroupedPlane] localIndex[%u] not found in groups.",
            localIndex), HCCL_E_PARA);

    netPlaneId = netPlaneId * symGroupSize + rankOfGroup;
    netPlaneNum = symGroupSize * netPlaneNum;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::ApplyTransform(const u32 netPlaneId, const u32 localIndex,
    const std::vector<std::vector<u32>> &groups, const std::vector<std::vector<u32>> &transformMatrix,
    std::vector<u32> &indexList)
{
    bool isFound = false;
    u32 targetRowId = 0;
    std::tie(isFound, targetRowId) = ApplyTransformUtils::FindTargetRow(groups, localIndex);
    CHK_PRT_RET(!isFound,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] localIndex[%u] not found in groups.",
            localIndex), HCCL_E_PARA);

    const u32 symGroupSize = CalcSymGroupSize(groups);
    if (targetRowId >= symGroupSize) {
        return HCCL_SUCCESS;
    }

    const u32 metaPlaneNum = static_cast<u32>(transformMatrix.size());
    if (metaPlaneNum == 0) {
        return HCCL_SUCCESS;
    }

    const auto metaPlaneIds =
        ApplyTransformUtils::ComputeMetaPlaneIds(targetRowId, netPlaneId, metaPlaneNum, symGroupSize);
    for (u32 row = 0; row < symGroupSize; ++row) {
        ApplyTransformUtils::ProcessSingleRow(row, metaPlaneIds, groups, transformMatrix, indexList);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::TransformPlaneByAlgo(const HcclAlgoType algType, const u32 netPlaneId,
    const u32 localIndex, const std::vector<std::vector<u32>> &groups, std::vector<u32> &indexList)
{
    CHK_PRT_RET(groups.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][TransformPlaneByAlgo] groups is empty."),
        HCCL_E_PARA);

    u32 maxIndex = 0;
    for (const auto &group : groups) {
        CHK_PRT_RET(group.empty(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][TransformPlaneByAlgo] group is empty."),
            HCCL_E_PARA);
        maxIndex = std::max(maxIndex, *std::max_element(group.begin(), group.end()));
    }
    indexList.assign(maxIndex + 1, INVALID_UINT);

    const std::unique_ptr<IPlaneTransformer> transformerPtr = PlaneTransformerFactory::Instance().Get(algType);
    if (transformerPtr == nullptr) {
        for (u32 i = 0; i < indexList.size(); ++i) {
            indexList[i] = i;
        }
        HCCL_WARNING("[OXC_HCOMM][TopoinfoPlaneTransformer][TransformPlaneByAlgo] algType[%u] is not available in factory, fallback to identity mapping.",
            static_cast<u32>(algType));
        return HCCL_SUCCESS;
    }

    const u32 planeSize = static_cast<u32>(groups.size());
    const u32 planeNum = CalcSymGroupSize(groups);
    const std::vector<std::vector<u32>> transformMatrix = transformerPtr->CalcPlaneTransform(planeSize, planeNum);

    HCCL_INFO("[OXC_HCOMM][TopoinfoPlaneTransformer][TransformPlaneByAlgo] use factory transform for algType[%u], planeSize[%u], planeNum[%u], netPlaneId[%u].",
        static_cast<u32>(algType), planeSize, planeNum, netPlaneId);
    CHK_RET(ApplyTransform(netPlaneId, localIndex, groups, transformMatrix, indexList));
    for (u32 i = 0; i < indexList.size(); ++i) {
        if (indexList[i] == INVALID_UINT) {
            indexList[i] = i;
        }
    }
    return HCCL_SUCCESS;
}
}  // namespace hccl
