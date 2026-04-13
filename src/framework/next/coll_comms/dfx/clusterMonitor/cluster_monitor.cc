/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "cluster_monitor.h"
#include "hccl_types.h"

namespace hccl {

HcclResult ClusterMonitor::FormatUID(ClusterUidCtxt ctxt, ClusterUIDType &uid)
{
    s32 ret = snprintf_s(uid.id, sizeof(uid.id), sizeof(uid.id) - 1, "%s%s%s%s%s",
        std::to_string(ctxt.netLayer).c_str(), "/",
        netInstanceId.c_str(), "/",
        std::to_string(ctxt.rankId).c_str());
    CHK_PRT_RET((ret == -1),
        HCCL_ERROR("[%s] snprintf_s failed, errno:%d, error:%s",
            __func__, errno, strerror(errno)), HCCL_E_SYSCALL);

    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::RegisterToClusterMonitor(HcclComm comm)
{
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);

    // 单rank无对端，不支持心跳检测
    const std::string &commId = collComm->GetCommId();
    uint32_t rankSize = collComm->GetRankSize();
    CHK_PRT_RET(rankSize == 1,
        HCCL_WARNING("[%s] commId[%s] rankSize[%u] no need to register to ClusterMonitor",
            __func__, commId.c_str(), rankSize), HCCL_SUCCESS);

    // 判断该通信域是否曾经添加到commIdMap_中
    std::unique_lock<std::mutex> lock(threadLock_);
    auto iter = commIdMap_.find(commId);
    if (iter != commIdMap_.end()) {
        HCCL_INFO("commId[%s] has Registered, skip.", commId.c_str());
        return HCCL_SUCCESS;
    }

    if (!initialized_) {
        // 开始起监控线程
        BeginMonitorThread();
    }
    lock.unlock();

    // 将所有远端的rank都加入到状态维护map中
    uint32_t *netLayers;
    uint32_t netLayerNum;
    auto myRank = collComm->GetMyRank();
    auto rankGraph = collComm->GetRankGraph();
    CHK_RET(HcclRankGraphGetLayers(comm, &netLayers, &netLayerNum));
    std::vector<uint32_t> netLayersVector(netLayers, netLayers + netLayerNum);
    for (auto netLayer : netLayersVector) {
        uint32_t *ranks = nullptr;
        uint32_t rankNum;
        const NetInstance *netInstance = rankGraph->GetNetInstanceByRankId(netLayer, myRank);
        auto netInstanceId = netInstance->GetNetInstId();
        auto myRankId = collComm->GetMyRankId();
        std::set<RankId> rankSet = netInstance->GetRankIds();
        for (RankId rankId : rankSet) {
            if (rankId == myRank) {
                continue;
            }

        }
    }
}

HcclResult ClusterMonitor::UnRegisterToClusterMonitor(HcclComm comm)
{

}

}// namespace hccl