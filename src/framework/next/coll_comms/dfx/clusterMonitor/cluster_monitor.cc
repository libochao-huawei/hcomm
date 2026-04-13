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
#include "comm_addr_convert.h"

namespace hccl {

HcclResult ClusterMonitor::FormatUID(ClusterUidCtxt ctxt, ClusterUIDType &uid)
{
    // 构造唯一的uid: netLayer/netInstanceId/deviceLogicId/commAddrStr
    s32 ret = snprintf_s(uid.id, sizeof(uid.id), sizeof(uid.id) - 1, "%s%s%s%s%s%s%s",
        std::to_string(ctxt.netLayer).c_str(), "/",
        ctxt.netInstanceId.c_str(), "/",
        std::to_string(ctxt.deviceLogicId).c_str(), "/",
        ctxt.commAddrStr.c_str());
    CHK_PRT_RET((ret == -1),
        HCCL_ERROR("[%s] snprintf_s failed, errno:%d, error:%s",
            __func__, errno, strerror(errno)), HCCL_E_SYSCALL);

    return HCCL_SUCCESS;
}

std::string ClusterMonitor::GetUID(const ClusterUIDType &uid) const
{
    return uid.id;
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
    lock.lock();
    for (auto netLayer : netLayersVector) {
        if (netLayer >= 2) { // 仅考虑跨server/pod
            break;
        }
        const NetInstance *netInstance = rankGraph->GetNetInstanceByRankId(netLayer, myRank);
        auto netInstanceId = netInstance->GetNetInstId();
        auto myRankId = collComm->GetMyRankId();
        std::set<RankId> rankSet = netInstance->GetRankIds();
        for (RankId rankId : rankSet) {
            if (rankId == myRank) {
                continue;
            }
            CommLink *linkList = nullptr; // 必须初始化为nullptr
            uint32_t listSize = 0;
            CHK_RET(HcclRankGraphGetLinks(comm, netLayer, myRankId, rankId, &linkList, &listSize));
            // 如果listSize为0，表示这两个rank之间没有直接link，直接进入下一轮循环
            if (listSize == 0) {
                HCCL_DEBUG("[GetPairLinkCounter]No links found between srcRank[%u] and dstRank[%u]", myRankId, rankId);
                continue;
            }

            // linkList[0]存入到对应netLayer的容器里，待选择对应的双ring环

            // 构造UID存入map里
            ClusterUidCtxt uidCtxt(netLayer, netInstanceId, collComm->deviceLogicId_,
                CommAddr2Str(linkList[0].dstEndpointDesc.commAddr));
            ClusterUIDType remoteUID{};
            CHK_RET(FormatUID(uidCtxt, remoteUID));
            uid2FrameStatusMap_.insert(remoteUID, FrameStatus());
            commIdMap_[commId].insert(std::make_pair(remoteUID, false)); //初始状态均为未连接
            HCCL_INFO("commId[%s] insert my rank[%u] remoteUID[%s]",
                commId.c_str(), myRankId, GetUID(remoteUID).c_str());
        }
    }
    lock.unlock();

    // 解析heartbeat环境变量

    // 获取需要连接的endPointDesc
    std::map<ClusterUIDType, ClusterMonitorContext> needConnectRank;
    CHK_RET(GetConnectRank(locRank, rankInfos, needConnectRank));

            channelDesc.remoteRank = rank;
            channelDesc.localEndpoint.protocol = link.srcEndpointDesc.protocol;
            channelDesc.localEndpoint.commAddr = link.srcEndpointDesc.commAddr;
            channelDesc.localEndpoint.loc = link.srcEndpointDesc.loc;
            channelDesc.remoteEndpoint.protocol = link.dstEndpointDesc.protocol;
            channelDesc.remoteEndpoint.commAddr = link.dstEndpointDesc.commAddr;
            channelDesc.remoteEndpoint.loc = link.dstEndpointDesc.loc;
            HCCL_DEBUG("%s local device phyId: %u, remote device phyId: %u.",
                        funcName.c_str(), channelDesc.localEndpoint.loc.device.devPhyId,
                        channelDesc.remoteEndpoint.loc.device.devPhyId);
            HCCL_INFO("%s Add channel request between %zu and %zu, netLayerIdx %u, "
                      "linkListIdx %u, protocol %zu",
                      funcName.c_str(), myRank, channelDesc.remoteRank, netLayer, idx, channelDesc.remoteEndpoint.protocol);

    std::unique_lock<std::mutex> linkInfolock(hbLinkConnInfoMtx_);
    for (auto &item : needConnectRank) {
        if (item.second.newConn == true) {
            hbLinkConnInfo_[group].push(std::move(item));
        }
    }
}

HcclResult ClusterMonitor::UnRegisterToClusterMonitor(HcclComm comm)
{

}

}// namespace hccl