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
constexpr double STATIC_DELAY = 10.0;
constexpr double TRANSMIT_DELAY = 5.0;
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

u32 CeilDiv(const u32 lhs, const u32 rhs)
{
    return (rhs == 0) ? 0 : ((lhs + rhs - 1) / rhs);
}

void ApplyGroupsRefactor(const u32 symmSize, std::vector<std::vector<u32>> &groups)
{
    if (symmSize == 0) {
        return;
    }

    std::vector<std::vector<u32>> newGroups;
    for (const auto &group : groups) {
        std::vector<u32> tmpGroup;
        for (u32 value : group) {
            tmpGroup.push_back(value);
            if (tmpGroup.size() == symmSize) {
                newGroups.push_back(tmpGroup);
                tmpGroup.clear();
            }
        }
        if (!tmpGroup.empty()) {
            newGroups.push_back(tmpGroup);
        }
    }
    groups = std::move(newGroups);
}

HcclAlgoType ResolveAhcAlgoType(const std::string &envName, const bool allowDefault)
{
    const std::string algoName = SalGetEnv(envName.c_str());
    if (algoName == "ring") {
        return HcclAlgoType::HCCL_ALGO_TYPE_RING;
    }
    if (algoName == "HD") {
        return HcclAlgoType::HCCL_ALGO_TYPE_HDR;
    }
    if (algoName == "NB") {
        return HcclAlgoType::HCCL_ALGO_TYPE_NB;
    }
    if (algoName == "NHR") {
        return HcclAlgoType::HCCL_ALGO_TYPE_NHR;
    }
    if (algoName == "NHR_V1") {
        return HcclAlgoType::HCCL_ALGO_TYPE_NHR_V1;
    }
    if (allowDefault && algoName == "MTR") {
        return HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    }
    return HcclAlgoType::HCCL_ALGO_TYPE_RING;
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

namespace ApplyTransformUtils {
std::pair<bool, u32> FindTargetRow(const std::vector<std::vector<u32>> &groups, const u32 localIndex)
{
    for (const auto &group : groups) {
        auto it = std::find(group.begin(), group.end(), localIndex);
        if (it != group.end()) {
            return {true, static_cast<u32>(std::distance(group.begin(), it))};
        }
    }
    return {false, 0U};
}

std::vector<u32> ComputeMetaPlaneIds(const u32 targetRowId, const u32 netPlaneId, const u32 metaPlaneNum,
    const u32 symGroupSize)
{
    std::vector<u32> metaPlaneIds(symGroupSize, 0U);
    if (symGroupSize == 0U || metaPlaneNum == 0U) {
        return metaPlaneIds;
    }

    metaPlaneIds[targetRowId] = netPlaneId % metaPlaneNum;
    for (s32 row = static_cast<s32>(targetRowId) - 1; row >= 0; --row) {
        metaPlaneIds[static_cast<u32>(row)] = (metaPlaneIds[static_cast<u32>(row + 1)] + metaPlaneNum - 1U) % metaPlaneNum;
    }
    for (u32 row = targetRowId + 1U; row < symGroupSize; ++row) {
        metaPlaneIds[row] = (metaPlaneIds[row - 1U] + 1U) % metaPlaneNum;
    }
    return metaPlaneIds;
}

HcclResult ProcessSingleRow(const u32 row, const std::vector<u32> &metaPlaneIds,
    const std::vector<std::vector<u32>> &groups, const TransformMatrix &transformMatrix, std::vector<u32> &indexList)
{
    const u32 metaPlaneId = metaPlaneIds[row];
    CHK_PRT_RET(metaPlaneId >= transformMatrix.size(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] metaPlaneId[%u] out of range[%zu].",
            metaPlaneId, transformMatrix.size()), HCCL_E_PARA);
    const auto &mapping = transformMatrix[metaPlaneId];
    const u32 numNodes = static_cast<u32>(groups.size());
    CHK_PRT_RET(mapping.size() != numNodes,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] mappingSize[%zu] != numNodes[%u].",
            mapping.size(), numNodes), HCCL_E_PARA);

    std::vector<u32> rowIndexList;
    rowIndexList.reserve(numNodes);
    for (const auto &group : groups) {
        CHK_PRT_RET(row >= group.size(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] row[%u] out of range groupSize[%zu].",
                row, group.size()), HCCL_E_PARA);
        rowIndexList.push_back(group[row]);
    }

    std::vector<u32> elemList(numNodes, 0U);
    for (u32 i = 0; i < numNodes; ++i) {
        CHK_PRT_RET(mapping[i] >= rowIndexList.size(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] mapping[%u]=%u out of range[%zu].",
                i, mapping[i], rowIndexList.size()), HCCL_E_PARA);
        elemList[i] = rowIndexList[mapping[i]];
    }

    for (u32 i = 0; i < numNodes; ++i) {
        CHK_PRT_RET(rowIndexList[i] >= indexList.size(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] rowIndex[%u]=%u out of range[%zu].",
                i, rowIndexList[i], indexList.size()), HCCL_E_PARA);
        indexList[rowIndexList[i]] = elemList[i];
    }
    return HCCL_SUCCESS;
}
}  // namespace ApplyTransformUtils

u32 TopoinfoPlaneTransformer::CalcSymGroupSize(const std::vector<std::vector<u32>> &groups)
{
    CHK_PRT_RET(groups.empty(), HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][CalcSymGroupSize] groups is empty."),
        0);
    u32 symGroupSize = static_cast<u32>(groups.front().size());
    for (const auto &group : groups) {
        symGroupSize = std::min(symGroupSize, static_cast<u32>(group.size()));
    }
    return symGroupSize;
}

HcclResult TopoinfoPlaneTransformer::ApplyTransform(const u32 netPlaneId, const u32 localIndex,
    const std::vector<std::vector<u32>> &groups, const TransformMatrix &transformMatrix, std::vector<u32> &indexList)
{
    CHK_PRT_RET(groups.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] groups is empty."), HCCL_E_PARA);
    bool isFound = false;
    u32 targetRowId = 0U;
    std::tie(isFound, targetRowId) = ApplyTransformUtils::FindTargetRow(groups, localIndex);
    CHK_PRT_RET(!isFound,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] localIndex[%u] not found.", localIndex),
        HCCL_E_NOT_FOUND);

    const u32 symGroupSize = CalcSymGroupSize(groups);
    BuildIdentityIndexList(static_cast<u32>(indexList.size()), indexList);
    if (targetRowId >= symGroupSize) {
        return HCCL_SUCCESS;
    }

    const u32 metaPlaneNum = static_cast<u32>(transformMatrix.size());
    if (metaPlaneNum == 0U) {
        return HCCL_SUCCESS;
    }
    CHK_PRT_RET(metaPlaneNum > 0U && groups.size() != transformMatrix.front().size(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] groupNum[%zu] != matrixWidth[%zu].",
            groups.size(), transformMatrix.front().size()), HCCL_E_PARA);
    for (u32 metaPlaneId = 0; metaPlaneId < metaPlaneNum; ++metaPlaneId) {
        CHK_PRT_RET(transformMatrix[metaPlaneId].size() != groups.size(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] matrixRow[%u] width[%zu] != groupNum[%zu].",
                metaPlaneId, transformMatrix[metaPlaneId].size(), groups.size()), HCCL_E_PARA);
        std::vector<bool> seen(groups.size(), false);
        for (u32 column = 0; column < transformMatrix[metaPlaneId].size(); ++column) {
            const u32 mapped = transformMatrix[metaPlaneId][column];
            CHK_PRT_RET(mapped >= groups.size(),
                HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] matrixRow[%u][%u]=%u out of range[%zu].",
                    metaPlaneId, column, mapped, groups.size()), HCCL_E_PARA);
            CHK_PRT_RET(seen[mapped],
                HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] matrixRow[%u] duplicated mapped[%u].",
                    metaPlaneId, mapped), HCCL_E_PARA);
            seen[mapped] = true;
        }
    }
    for (const auto &group : groups) {
        CHK_PRT_RET(group.empty(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] found empty group."), HCCL_E_PARA);
        for (u32 index : group) {
            CHK_PRT_RET(index >= indexList.size(),
                HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ApplyTransform] group index[%u] out of range[%zu].",
                    index, indexList.size()), HCCL_E_PARA);
        }
    }
    const auto metaPlaneIds = ApplyTransformUtils::ComputeMetaPlaneIds(targetRowId, netPlaneId, metaPlaneNum, symGroupSize);
    for (u32 row = 0U; row < symGroupSize; ++row) {
        CHK_RET(ApplyTransformUtils::ProcessSingleRow(row, metaPlaneIds, groups, transformMatrix, indexList));
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::RegroupAndSelectAlgo(const u32 netPlaneNum, const bool enabledBroke,
    std::vector<std::vector<u32>> &groups, HcclAlgoType &intraAlgType, HcclAlgoType &interAlgType)
{
    CHK_PRT_RET(groups.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][RegroupAndSelectAlgo] groups is empty."), HCCL_E_PARA);

    std::vector<u32> groupSizes;
    groupSizes.reserve(groups.size());
    for (const auto &group : groups) {
        CHK_PRT_RET(group.empty(),
            HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][RegroupAndSelectAlgo] found empty group."), HCCL_E_PARA);
        groupSizes.push_back(static_cast<u32>(group.size()));
    }

    u32 bestSymmSize = 1;
    if (enabledBroke) {
        const u32 sumGroupSize = std::accumulate(groupSizes.begin(), groupSizes.end(), 0U);
        const u32 minGroupSize = *std::min_element(groupSizes.begin(), groupSizes.end());
        double minCost = static_cast<double>(sumGroupSize) * STATIC_DELAY;
        for (u32 symmSize = 1; symmSize <= minGroupSize; ++symmSize) {
            double maxBrokeRate = 0.0;
            u32 newGroupNum = 0;
            for (const u32 size : groupSizes) {
                const double brokeRate = static_cast<double>(symmSize + (size % symmSize) - 1U) /
                    static_cast<double>(symmSize);
                maxBrokeRate = std::max(maxBrokeRate, brokeRate);
                newGroupNum += CeilDiv(size, symmSize);
            }
            const double cost = (static_cast<double>(symmSize + newGroupNum) * STATIC_DELAY) +
                (maxBrokeRate * TRANSMIT_DELAY);
            if (cost < minCost) {
                minCost = cost;
                bestSymmSize = symmSize;
            }
        }
    } else {
        bestSymmSize = groupSizes.front();
        for (u32 i = 1; i < groupSizes.size(); ++i) {
            bestSymmSize = std::__gcd(bestSymmSize, groupSizes[i]);
        }
    }

    ApplyGroupsRefactor(bestSymmSize, groups);

    intraAlgType = ResolveAhcAlgoType("HCCL_AHC_ALGO_INTRA", false);
    interAlgType = ResolveAhcAlgoType("HCCL_AHC_ALGO_INTER", true);
    HCCL_INFO("[OXC_HCOMM][TopoinfoPlaneTransformer][RegroupAndSelectAlgo] netPlaneNum[%u] enabledBroke[%u] bestSymmSize[%u] intraAlgType[%u] interAlgType[%u].",
        netPlaneNum, static_cast<u32>(enabledBroke), bestSymmSize, intraAlgType, interAlgType);
    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::ReparseGroupedPlane(const u32 localIndex,
    const std::vector<std::vector<u32>> &groups, u32 &netPlaneId, u32 &netPlaneNum)
{
    CHK_PRT_RET(groups.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ReparseGroupedPlane] groups is empty."), HCCL_E_PARA);

    const u32 symGroupSize = CalcSymGroupSize(groups);
    u32 rankOfGroup = 0;
    bool found = false;
    for (const auto &group : groups) {
        for (u32 i = 0; i < group.size(); ++i) {
            if (group[i] == localIndex) {
                rankOfGroup = std::min(i, symGroupSize - 1U);
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
    }
    CHK_PRT_RET(!found,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ReparseGroupedPlane] localIndex[%u] not found.", localIndex),
        HCCL_E_NOT_FOUND);

    netPlaneId = netPlaneId * symGroupSize + rankOfGroup;
    netPlaneNum = symGroupSize * netPlaneNum;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::TransformPlaneByAlgo(HcclAlgoType algType, const u32 netPlaneId,
    const u32 localIndex, const std::vector<std::vector<u32>> &groups, std::vector<u32> &indexList)
{
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(algType);
    if (transformer == nullptr) {
        BuildIdentityIndexList(static_cast<u32>(indexList.size()), indexList);
        return HCCL_SUCCESS;
    }

    const u32 planeSize = static_cast<u32>(groups.size());
    const u32 planeNum = CalcSymGroupSize(groups);
    TransformMatrix transformMatrix;
    CHK_RET(transformer->CalcPlaneTransform(planeSize, planeNum, transformMatrix));
    return ApplyTransform(netPlaneId, localIndex, groups, transformMatrix, indexList);
}

}  // namespace hccl
