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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "log.h"

namespace hccl {
namespace {
using RankIdentity = std::pair<std::string, s32>;

/**
 * @brief 为 rank 构造稳定身份键。
 * @param rankInfo 输入 rank 信息。
 * @return RankIdentity
 *
 * @note 当前阶段使用 `serverId + devicePhyId` 作为稳定身份，
 *       用于把 `subRankTable` 成员映射回 `globalRankTable`。
 */
RankIdentity BuildRankIdentity(const RankInfo_t &rankInfo)
{
    return std::make_pair(rankInfo.serverId, rankInfo.deviceInfo.devicePhyId);
}

/**
 * @brief 获取 rank 的主并行平面标识。
 * @param rankInfo 输入 rank 信息。
 * @param planeToken 输出平面标识。
 * @return HcclResult
 *
 * @note 当前实现优先从 levelList 中第一个可用的 `planeId` 提取；
 *       如果所有 level 都缺失可用 planeId，则返回参数错误。
 */
HcclResult GetPrimaryPlaneToken(const RankInfo_t &rankInfo, std::string &planeToken)
{
    for (const auto &levelInfo : rankInfo.levelList) {
        for (const auto &addrInfo : levelInfo.rankAddrList) {
            if (!addrInfo.planeId.empty()) {
                planeToken = addrInfo.planeId;
                return HCCL_SUCCESS;
            }
        }
    }

    HCCL_ERROR("[OXC_HCOMM][GetPrimaryPlaneToken] rank[%u] has no valid plane token.", rankInfo.rankId);
    return HCCL_E_PARA;
}

/**
 * @brief 统计当前 rank 所在 server 的设备数量。
 * @param globalRankTable 全局 ranktable。
 * @param serverId 当前 rank 的 serverId。
 * @param npuPerServer 输出同 server 的设备数。
 * @return HcclResult
 */
HcclResult GetNpuPerServer(const RankTable_t &globalRankTable, const std::string &serverId, u32 &npuPerServer)
{
    std::set<s32> deviceIds;
    for (const auto &rankInfo : globalRankTable.rankList) {
        if (rankInfo.serverId == serverId && rankInfo.deviceInfo.devicePhyId != HOST_DEVICE_ID) {
            deviceIds.insert(rankInfo.deviceInfo.devicePhyId);
        }
    }

    CHK_PRT_RET(deviceIds.empty(),
        HCCL_ERROR("[OXC_HCOMM][GetNpuPerServer] server[%s] has no valid device.", serverId.c_str()), HCCL_E_PARA);
    npuPerServer = static_cast<u32>(deviceIds.size());
    return HCCL_SUCCESS;
}

/**
 * @brief 获取当前 rank 在 server 内的本地索引。
 * @param globalRankInfo 全局 rank 信息。
 * @param localRankInServer 输出本地索引。
 * @return HcclResult
 */
HcclResult GetLocalRankInServer(const RankInfo_t &globalRankInfo, u32 &localRankInServer)
{
    CHK_PRT_RET(globalRankInfo.localRank == INVALID_UINT,
        HCCL_ERROR("[OXC_HCOMM][GetLocalRankInServer] rank[%u] localRank is invalid.", globalRankInfo.rankId),
        HCCL_E_PARA);
    localRankInServer = globalRankInfo.localRank;
    return HCCL_SUCCESS;
}
}  // namespace

HcclResult TopoinfoPlaneTransformer::ParsePlane(const RankTable_t &globalRankTable, const RankTable_t &subRankTable,
    u32 subCommRankId, u32 &netPlaneId, u32 &netPlaneNum)
{
    netPlaneId = 0;
    netPlaneNum = 1;

    CHK_PRT_RET(globalRankTable.rankList.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlane] global rankList is empty."), HCCL_E_PARA);
    CHK_PRT_RET(subRankTable.rankList.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlane] sub rankList is empty."), HCCL_E_PARA);
    CHK_PRT_RET(subCommRankId >= subRankTable.rankList.size(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParsePlane] subCommRankId[%u] out of range[%zu].",
            subCommRankId, subRankTable.rankList.size()), HCCL_E_PARA);

    if (subRankTable.rankList.size() <= 1) {
        return HCCL_SUCCESS;
    }

    return ParseUniformPlane(globalRankTable, subRankTable, subCommRankId, netPlaneId, netPlaneNum);
}

HcclResult TopoinfoPlaneTransformer::ParseUniformPlane(const RankTable_t &globalRankTable,
    const RankTable_t &subRankTable, u32 subCommRankId, u32 &netPlaneId, u32 &netPlaneNum)
{
    std::map<RankIdentity, const RankInfo_t *> globalRankMap;
    std::vector<std::string> globalPlaneTokens;
    std::set<std::string> planeTokenDedup;

    for (const auto &globalRankInfo : globalRankTable.rankList) {
        if (globalRankInfo.deviceInfo.devicePhyId == HOST_DEVICE_ID) {
            continue;
        }
        globalRankMap[BuildRankIdentity(globalRankInfo)] = &globalRankInfo;

        std::string planeToken;
        if (GetPrimaryPlaneToken(globalRankInfo, planeToken) == HCCL_SUCCESS &&
            planeTokenDedup.insert(planeToken).second) {
            globalPlaneTokens.push_back(planeToken);
        }
    }

    CHK_PRT_RET(globalPlaneTokens.empty(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlane] no plane token found in global ranktable."),
        HCCL_E_PARA);

    const RankInfo_t &subRankInfo = subRankTable.rankList[subCommRankId];
    auto globalRankIter = globalRankMap.find(BuildRankIdentity(subRankInfo));
    CHK_PRT_RET(globalRankIter == globalRankMap.end(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlane] failed to map sub rank[%u] back to global identity.",
            subRankInfo.rankId), HCCL_E_PARA);
    const RankInfo_t &globalRankInfo = *(globalRankIter->second);

    std::string planeToken;
    CHK_RET(GetPrimaryPlaneToken(globalRankInfo, planeToken));
    auto planeTokenIter = std::find(globalPlaneTokens.begin(), globalPlaneTokens.end(), planeToken);
    CHK_PRT_RET(planeTokenIter == globalPlaneTokens.end(),
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlane] plane token[%s] not found.",
            planeToken.c_str()),
        HCCL_E_PARA);

    u32 localRankInServer = 0;
    CHK_RET(GetLocalRankInServer(globalRankInfo, localRankInServer));

    u32 npuPerServer = 0;
    CHK_RET(GetNpuPerServer(globalRankTable, globalRankInfo.serverId, npuPerServer));
    CHK_PRT_RET(localRankInServer >= npuPerServer,
        HCCL_ERROR("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlane] localRankInServer[%u] exceeds npuPerServer[%u].",
            localRankInServer, npuPerServer), HCCL_E_PARA);

    const u32 parallelCommId = static_cast<u32>(std::distance(globalPlaneTokens.begin(), planeTokenIter));
    netPlaneNum = static_cast<u32>(globalPlaneTokens.size()) * npuPerServer;
    netPlaneId = parallelCommId * npuPerServer + localRankInServer;

    HCCL_INFO("[OXC_HCOMM][TopoinfoPlaneTransformer][ParseUniformPlane] server[%s] planeToken[%s] netPlaneId[%u] netPlaneNum[%u].",
        globalRankInfo.serverId.c_str(), planeToken.c_str(), netPlaneId, netPlaneNum);
    return HCCL_SUCCESS;
}
}  // namespace hccl
