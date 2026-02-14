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
#include "log.h"
#include "sal_pub.h"

namespace hccl {

namespace {
// C++14 兼容的 GCD 实现
u32 Gcd(u32 a, u32 b)
{
    while (b != 0) {
        u32 temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}
} // namespace

namespace ParseUniformPlaneUtils {

bool VerifySameGroup(const u32 offset, const u32 type, const std::vector<u32> &rankIds,
    const std::vector<RankInfo_t> &globalRankList)
{
    const u32 rankNum = static_cast<u32>(rankIds.size());
    for (u32 i = 0; i < rankNum; ++i) {
        const u32 rankBase = rankIds[i];
        u32 rankShifted = rankBase;
        switch (type) {
            case HCCL_ADD_FLAG:
                rankShifted += offset;
                break;
            case HCCL_SUBTRACT_FLAG:
                rankShifted -= offset;
                break;
            default:
                break;
        }
        if (globalRankList[rankShifted].groupId != globalRankList[rankBase].groupId) {
            return false;
        }
    }
    return true;
}

u32 CalcValidOffset(const u32 maxDelta, const u32 directFlag, const std::vector<u32> &rankIds,
    const std::vector<RankInfo_t> &globalRankList)
{
    u32 offset = 0;
    while (offset + 1 <= maxDelta) {
        if (!VerifySameGroup(offset + 1, directFlag, rankIds, globalRankList)) {
            break;
        }
        offset++;
    }
    return offset;
}

u32 CountNpusInSameServer(const std::vector<u32> &rankIds, const std::vector<RankInfo_t> &globalRankList)
{
    if (rankIds.empty()) {
        return 0;
    }

    const std::string &refServerId = globalRankList[rankIds[0]].serverId;
    return static_cast<u32>(std::count_if(rankIds.begin(), rankIds.end(),
        [&](const u32 id) { return globalRankList[id].serverId == refServerId; }));
}

void CalcIntraSeverParallel(const u32 parCommId, const u32 parCommNum, const u32 npuPerServer,
    std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    std::unordered_map<u32, u32> npuMapping;
    u32 localRankCounter = 0;
    netPlaneNum = parCommNum * npuPerServer;

    for (auto &rank : subRankList) {
        const u32 phyId = static_cast<u32>(rank.deviceInfo.devicePhyId);

        const auto emplaceResult = npuMapping.emplace(phyId, localRankCounter);
        if (emplaceResult.second) {
            ++localRankCounter;
        }

        rank.netPlaneId = parCommId * npuPerServer + emplaceResult.first->second;
    }
}

} // namespace ParseUniformPlaneUtils

HcclResult TopoinfoPlaneTransformer::ParseUniformPlane(const std::vector<u32> &rankIds,
    const std::vector<RankInfo_t> &globalRankList, std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    // 1. 计算最大扩展范围
    const u32 rankNum = static_cast<u32>(rankIds.size());
    const u32 maxGlobalRank = static_cast<u32>(globalRankList.size()) - 1;
    u32 maxDeltaLef = rankIds[0] - 0;
    u32 maxDeltaRig = maxGlobalRank - rankIds[rankNum - 1];
    for (u32 i = 1; i < rankNum; ++i) {
        maxDeltaLef = std::min(maxDeltaLef, rankIds[i] - rankIds[i - 1] - 1);
        maxDeltaRig = std::min(maxDeltaRig, rankIds[i] - rankIds[i - 1] - 1);
    }

    // 2. 计算实际可扩展偏移量
    const u32 lefOffset =
        ParseUniformPlaneUtils::CalcValidOffset(maxDeltaLef, HCCL_SUBTRACT_FLAG, rankIds, globalRankList);
    const u32 rigOffset =
        ParseUniformPlaneUtils::CalcValidOffset(maxDeltaRig, HCCL_ADD_FLAG, rankIds, globalRankList);

    // 3. 通信域平行
    const u32 parCommId = lefOffset;
    const u32 parCommNum = lefOffset + rigOffset + 1;

    // 4. 统计同服务器内 NPU 数量
    const u32 npuPerServer = ParseUniformPlaneUtils::CountNpusInSameServer(rankIds, globalRankList);

    // 5. 机内同号卡平行
    ParseUniformPlaneUtils::CalcIntraSeverParallel(parCommId, parCommNum, npuPerServer, subRankList, netPlaneNum);

    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::ParsePlane(const u32 rankNum, const u32 *rankIds,
    const std::vector<RankInfo_t> &globalRankList, std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum)
{
    if (rankIds == nullptr) {
        HCCL_ERROR("[OXC][ParsePlane] rankIds is nullptr.");
        return HCCL_E_PARA;
    }

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
    const u32 rankNum = static_cast<u32>(rankIds.size());
    if (rankNum == 0) {
        HCCL_DEBUG("[OXC][ParsePlane] rankIds is empty, skip plane parsing.");
        netPlaneNum = 1;
        return HCCL_SUCCESS;
    }

    if (rankNum == 1) {
        // 单 rank 情况，netPlaneNum = 1
        HCCL_DEBUG("[OXC][ParsePlane] Single rank, netPlaneNum = 1.");
        netPlaneNum = 1;
        return HCCL_SUCCESS;
    }

    // 判断是否为均匀平面（rankIds 呈等差数列）
    const u32 offset = rankIds[1] - rankIds[0];
    bool isUniformPlane = true;
    for (size_t i = 2; i < rankIds.size(); ++i) {
        if (rankIds[i] - rankIds[i - 1] != offset) {
            isUniformPlane = false;
            break;
        }
    }

    if (isUniformPlane) {
        return ParseUniformPlane(rankIds, globalRankList, subRankList, netPlaneNum);
    } else {
        // 非均匀平面，降级为默认值 netPlaneNum = 1
        HCCL_INFO("[OXC][ParsePlane] Non-uniform plane detected, fallback to netPlaneNum = 1.");
        netPlaneNum = 1;
        return HCCL_SUCCESS;
    }
}

u32 TopoinfoPlaneTransformer::CalcSymGroupSize(const std::vector<std::vector<u32>> &groups)
{
    if (groups.empty()) {
        return 0;
    }

    u32 symGroupSize = static_cast<u32>(groups[0].size());
    for (const auto &group : groups) {
        if (symGroupSize > static_cast<u32>(group.size())) {
            symGroupSize = static_cast<u32>(group.size());
        }
    }
    return symGroupSize;
}

static void ApplyGroupsRefactor(const u32 symmSize, std::vector<std::vector<u32>> &groups)
{
    std::vector<std::vector<u32>> newGroups;
    for (size_t i = 0; i < groups.size(); ++i) {
        size_t groupSize = groups[i].size();
        std::vector<u32> tmpGroup;
        for (size_t j = 0; j < groupSize; ++j) {
            tmpGroup.push_back(groups[i][j]);
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

HcclResult TopoinfoPlaneTransformer::RegroupAndSelectAlgo(const u32 netPlaneNum, const bool enabledBroke,
    std::vector<std::vector<u32>> &groups, HcclAlgoType &intraAlgType, HcclAlgoType &interAlgType)
{
    // 收集分组大小
    std::vector<u32> groupSize;
    for (const auto &group : groups) {
        groupSize.push_back(static_cast<u32>(group.size()));
    }

    u32 bestSymmSize = 1;
    if (enabledBroke) {
        // 启用 broke 模式时的计算
        u32 sumGroupSize = groupSize[0];
        u32 minGroupSize = groupSize[0];
        for (size_t i = 1; i < groupSize.size(); ++i) {
            if (minGroupSize > groupSize[i]) {
                minGroupSize = groupSize[i];
            }
            sumGroupSize += groupSize[i];
        }

        double minCost = sumGroupSize * STATIC_DELAY;
        bestSymmSize = 1;
        for (u32 symmSize = 1; symmSize <= minGroupSize; ++symmSize) {
            double maxBrokeRate = 0;
            u32 newGroupNum = 0;
            for (const u32 siz : groupSize) {
                const double brokeRate = static_cast<double>(symmSize + (siz % symmSize) - 1) / symmSize;
                if (maxBrokeRate < brokeRate) {
                    maxBrokeRate = brokeRate;
                }
                newGroupNum += siz / symmSize;
            }
            const double cost = (symmSize + newGroupNum) * STATIC_DELAY + maxBrokeRate * TRANSMIT_DELAY;
            if (cost < minCost) {
                bestSymmSize = symmSize;
                minCost = cost;
            }
        }
    } else {
        // 非 broke 模式：使用 GCD
        u32 symmSize = groupSize[0];
        for (size_t i = 1; i < groupSize.size(); ++i) {
            symmSize = Gcd(symmSize, groupSize[i]);
        }
        bestSymmSize = symmSize;
    }

    ApplyGroupsRefactor(bestSymmSize, groups);
    intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_RING;
    interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_RING;

    // 从环境变量读取算法配置
    std::string intraAlgoEnv = SalGetEnv("HCCL_AHC_ALGO_INTRA");
    if (intraAlgoEnv == "ring") {
        intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_RING;
    } else if (intraAlgoEnv == "HD") {
        intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_HDR;
    } else if (intraAlgoEnv == "NB") {
        intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_NB;
    } else if (intraAlgoEnv == "NHR") {
        intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_NHR;
    }

    std::string interAlgoEnv = SalGetEnv("HCCL_AHC_ALGO_INTER");
    if (interAlgoEnv == "ring") {
        interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_RING;
    } else if (interAlgoEnv == "HD") {
        interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_HDR;
    } else if (interAlgoEnv == "NB") {
        interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_NB;
    } else if (interAlgoEnv == "NHR") {
        interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_NHR;
    } else if (interAlgoEnv == "MTR") {
        interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    }

    HCCL_INFO("[OXC][RegroupAndSelectAlgo] netPlaneNum[%u] enabledBroke[%u] intraAlgType[%d] interAlgType[%d]",
        netPlaneNum, static_cast<u32>(enabledBroke), static_cast<s32>(intraAlgType), static_cast<s32>(interAlgType));

    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::ReparseGroupedPlane(const u32 localIndex,
    const std::vector<std::vector<u32>> &groups, u32 &netPlaneId, u32 &netPlaneNum)
{
    const u32 symGroupSize = CalcSymGroupSize(groups);
    if (symGroupSize == 0) {
        HCCL_WARNING("[OXC][ReparseGroupedPlane] symGroupSize is zero.");
        return HCCL_SUCCESS;
    }

    u32 rankOfGroup = 0;
    bool isFound = false;
    for (const auto &group : groups) {
        for (size_t i = 0; i < group.size(); ++i) {
            if (group[i] == localIndex) {
                rankOfGroup = std::min(static_cast<u32>(i), symGroupSize - 1);
                isFound = true;
                break;
            }
        }
        if (isFound) {
            break;
        }
    }

    netPlaneId = netPlaneId * symGroupSize + rankOfGroup;
    netPlaneNum = symGroupSize * netPlaneNum;

    return HCCL_SUCCESS;
}

namespace ApplyTransformUtils {

std::pair<bool, u32> FindTargetRow(const std::vector<std::vector<u32>> &groups, const u32 localIndex)
{
    for (const auto &group : groups) {
        auto it = std::find(group.begin(), group.end(), localIndex);
        if (it != group.end()) {
            return { true, static_cast<u32>(std::distance(group.begin(), it)) };
        }
    }
    return { false, 0 };
}

std::vector<u32> ComputeMetaPlaneIds(const u32 targetRowId, const u32 netPlaneId, const u32 metaPlaneNum,
    const u32 symGroupSize)
{
    std::vector<u32> metaPlaneIds(symGroupSize);
    if (symGroupSize == 0 || metaPlaneNum == 0) {
        return metaPlaneIds;
    }

    // 1. 设置基准点
    metaPlaneIds[targetRowId] = netPlaneId % metaPlaneNum;

    // 2. 向下扩散
    for (s32 row = static_cast<s32>(targetRowId) - 1; row >= 0; --row) {
        metaPlaneIds[row] = (metaPlaneIds[row + 1] + metaPlaneNum - 1) % metaPlaneNum;
    }

    // 3. 向上扩散
    for (size_t row = targetRowId + 1; row < symGroupSize; ++row) {
        metaPlaneIds[row] = (metaPlaneIds[row - 1] + 1) % metaPlaneNum;
    }

    return metaPlaneIds;
}

void ProcessSingleRow(const u32 row, const std::vector<u32> &metaPlaneIds,
    const std::vector<std::vector<u32>> &groups, const std::vector<std::vector<u32>> &rankMapping,
    std::vector<u32> &indexList)
{
    const u32 metaPlaneId = metaPlaneIds[row];
    const auto &mapping = rankMapping[metaPlaneId];
    const u32 numNodes = static_cast<u32>(groups.size());

    // 生成当前行所有节点的索引列表
    std::vector<u32> rowIndexList;
    rowIndexList.reserve(numNodes);
    for (const auto &group : groups) {
        if (row < group.size()) {
            rowIndexList.push_back(group[row]);
        }
    }

    // 应用映射规则生成元素列表
    std::vector<u32> elemList(rowIndexList.size());
    for (size_t i = 0; i < rowIndexList.size(); ++i) {
        if (mapping[i] < rowIndexList.size()) {
            elemList[i] = rowIndexList[mapping[i]];
        }
    }

    // 更新最终索引向量
    for (size_t i = 0; i < rowIndexList.size(); ++i) {
        if (rowIndexList[i] < indexList.size()) {
            indexList[rowIndexList[i]] = elemList[i];
        }
    }
}

} // namespace ApplyTransformUtils

HcclResult TopoinfoPlaneTransformer::ApplyTransform(const u32 netPlaneId, const u32 localIndex,
    const std::vector<std::vector<u32>> &groups, const std::vector<std::vector<u32>> &transformMatrix,
    std::vector<u32> &indexList)
{
    // 步骤1：定位目标行
    bool isFound;
    u32 targetRowId;
    std::tie(isFound, targetRowId) = ApplyTransformUtils::FindTargetRow(groups, localIndex);
    if (!isFound) {
        HCCL_ERROR("[OXC][ApplyTransform] Failed to find localIndex[%u] in groups.", localIndex);
        return HCCL_E_INTERNAL;
    }

    // 步骤2：验证对称组有效性
    const u32 symGroupSize = CalcSymGroupSize(groups);
    if (targetRowId >= symGroupSize) {
        // 无需处理 broke 情况
        return HCCL_SUCCESS;
    }

    HCCL_DEBUG("[OXC][ApplyTransform] netPlaneId: %u, localIndex: %u, symGroupSize: %u",
               netPlaneId, localIndex, symGroupSize);

    // 步骤3：计算元平面ID分布
    const u32 metaPlaneNum = static_cast<u32>(transformMatrix.size());
    if (metaPlaneNum == 0) {
        HCCL_DEBUG("[OXC][ApplyTransform] metaPlaneNum is zero.");
        return HCCL_SUCCESS;
    }

    const auto metaPlaneIds =
        ApplyTransformUtils::ComputeMetaPlaneIds(targetRowId, netPlaneId, metaPlaneNum, symGroupSize);

    // 步骤4：逐行处理数据转换
    for (u32 row = 0; row < symGroupSize; ++row) {
        ApplyTransformUtils::ProcessSingleRow(row, metaPlaneIds, groups, transformMatrix, indexList);
    }

    return HCCL_SUCCESS;
}

HcclResult TopoinfoPlaneTransformer::TransformPlaneByAlgo(const HcclAlgoType algType, const u32 netPlaneId,
    const u32 localIndex, const std::vector<std::vector<u32>> &groups, std::vector<u32> &indexList)
{
    // 1. 从工厂获取 PlaneTransformer 对象（线程安全）
    const std::unique_ptr<IPlaneTransformer> transformerPtr = PlaneTransformerFactory::Instance().Get(algType);

    // 2. 检查是否成功获取到对象
    if (transformerPtr == nullptr) {
        HCCL_WARNING("[OXC][TransformPlaneByAlgo] Failed to get PlaneTransformer for algoType[%d], skip transform.",
                     static_cast<s32>(algType));
        return HCCL_SUCCESS; // 取消重排操作
    }

    HCCL_DEBUG("[OXC][TransformPlaneByAlgo] netPlaneId[%u], localIndex[%u]", netPlaneId, localIndex);

    // 3. 计算平面变换
    const u32 planeSize = static_cast<u32>(groups.size());
    const u32 planeNum = CalcSymGroupSize(groups);
    const std::vector<std::vector<u32>> transformMatrix = transformerPtr->CalcPlaneTransform(planeSize, planeNum);

    // 4. 应用平面变换
    return ApplyTransform(netPlaneId, localIndex, groups, transformMatrix, indexList);
}

} // namespace hccl
