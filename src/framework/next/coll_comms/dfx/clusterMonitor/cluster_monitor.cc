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
#include "adapter_rts_common.h"
#include "externalinput_pub.h"

namespace hccl {
constexpr u32 BASE_NUMBER = 2;
constexpr u32 MAX_SENDBUFF_SIZE = 3072; // SendBuff[dst] 最大数量

HcclResult ClusterMonitor::FormatUID(ClusterUidCtxt ctxt, ClusterUIDType &uid)
{
    // 构造唯一的uid: netLayer/netInstanceId/deviceLogicId/commAddrStr
    s32 ret = snprintf_s(uid.id, sizeof(uid.id), sizeof(uid.id) - 1, "%s%s%s%s%s%s%s",
        std::to_string(ctxt.netLayer).c_str(), "/", ctxt.netInstanceId.c_str(), "/",
        std::to_string(ctxt.deviceLogicId).c_str(), "/", ctxt.commAddrStr.c_str());
    CHK_PRT_RET((ret == -1), HCCL_ERROR("[%s] snprintf_s failed, errno:%d, error:%s", __func__, errno, strerror(errno)),
        HCCL_E_SYSCALL);

    return HCCL_SUCCESS;
}

std::string ClusterMonitor::GetUID(const ClusterUIDType &uid) const
{
    return uid.id;
}

HcclResult ClusterMonitor::RegisterToClusterMonitor(HcclComm comm)
{
    // CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    // auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    // CHK_PTR_NULL(hcclComm);
    // hccl::CollComm* collComm = hcclComm->GetCollComm();
    // CHK_PTR_NULL(collComm);

    // // 单rank无对端，不支持心跳检测
    // const std::string &commId = collComm->GetCommId();
    // uint32_t rankSize = collComm->GetRankSize();
    // CHK_PRT_RET(rankSize == 1,
    //     HCCL_WARNING("[%s] commId[%s] rankSize[%u] no need to register to ClusterMonitor",
    //         __func__, commId.c_str(), rankSize), HCCL_SUCCESS);

    // // 判断该通信域是否曾经添加到commIdMap_中
    // std::unique_lock<std::mutex> lock(threadLock_);
    // auto iter = commIdMap_.find(commId);
    // if (iter != commIdMap_.end()) {
    //     HCCL_INFO("commId[%s] has Registered, skip.", commId.c_str());
    //     return HCCL_SUCCESS;
    // }

    // if (!initialized_) {
    //     // 开始起监控线程
    //     BeginMonitorThread();
    // }
    // lock.unlock();

    // // 将所有远端的rank都加入到状态维护map中
    // uint32_t *netLayers;
    // uint32_t netLayerNum;
    // auto myRank = collComm->GetMyRank();
    // auto rankGraph = collComm->GetRankGraph();
    // CHK_RET(HcclRankGraphGetLayers(comm, &netLayers, &netLayerNum));
    // std::vector<uint32_t> netLayersVector(netLayers, netLayers + netLayerNum);
    // lock.lock();
    // for (auto netLayer : netLayersVector) {
    //     if (netLayer >= 2) { // 仅考虑跨server/pod
    //         break;
    //     }
    //     const NetInstance *netInstance = rankGraph->GetNetInstanceByRankId(netLayer, myRank);
    //     auto netInstanceId = netInstance->GetNetInstId();
    //     auto myRankId = collComm->GetMyRankId();
    //     std::set<RankId> rankSet = netInstance->GetRankIds();
    //     for (RankId rankId : rankSet) {
    //         if (rankId == myRank) {
    //             continue;
    //         }
    //         CommLink *linkList = nullptr; // 必须初始化为nullptr
    //         uint32_t listSize = 0;
    //         CHK_RET(HcclRankGraphGetLinks(comm, netLayer, myRankId, rankId, &linkList, &listSize));
    //         // 如果listSize为0，表示这两个rank之间没有直接link，直接进入下一轮循环
    //         if (listSize == 0) {
    //             HCCL_DEBUG("[GetPairLinkCounter]No links found between srcRank[%u] and dstRank[%u]", myRankId,
    //             rankId); continue;
    //         }

    //         // linkList[0]存入到对应netLayer的容器里，待选择对应的双ring环

    //         // 构造UID存入map里
    //         ClusterUidCtxt uidCtxt(netLayer, netInstanceId, collComm->deviceLogicId_,
    //             CommAddr2Str(linkList[0].dstEndpointDesc.commAddr));
    //         ClusterUIDType remoteUID{};
    //         CHK_RET(FormatUID(uidCtxt, remoteUID));
    //         uid2FrameStatusMap_.insert(remoteUID, FrameStatus());
    //         commIdMap_[commId].insert(std::make_pair(remoteUID, false)); //初始状态均为未连接
    //         HCCL_INFO("commId[%s] insert my rank[%u] remoteUID[%s]",
    //             commId.c_str(), myRankId, GetUID(remoteUID).c_str());
    //     }
    // }
    // lock.unlock();

    // // 解析heartbeat环境变量

    // // 获取需要连接的endPointDesc
    // std::map<ClusterUIDType, ClusterMonitorContext> needConnectRank;
    // CHK_RET(GetConnectRank(locRank, rankInfos, needConnectRank));

    //         channelDesc.remoteRank = rank;
    //         channelDesc.localEndpoint.protocol = link.srcEndpointDesc.protocol;
    //         channelDesc.localEndpoint.commAddr = link.srcEndpointDesc.commAddr;
    //         channelDesc.localEndpoint.loc = link.srcEndpointDesc.loc;
    //         channelDesc.remoteEndpoint.protocol = link.dstEndpointDesc.protocol;
    //         channelDesc.remoteEndpoint.commAddr = link.dstEndpointDesc.commAddr;
    //         channelDesc.remoteEndpoint.loc = link.dstEndpointDesc.loc;
    //         HCCL_DEBUG("%s local device phyId: %u, remote device phyId: %u.",
    //                     funcName.c_str(), channelDesc.localEndpoint.loc.device.devPhyId,
    //                     channelDesc.remoteEndpoint.loc.device.devPhyId);
    //         HCCL_INFO("%s Add channel request between %zu and %zu, netLayerIdx %u, "
    //                   "linkListIdx %u, protocol %zu",
    //                   funcName.c_str(), myRank, channelDesc.remoteRank, netLayer, idx,
    //                   channelDesc.remoteEndpoint.protocol);

    // std::unique_lock<std::mutex> linkInfolock(hbLinkConnInfoMtx_);
    // for (auto &item : needConnectRank) {
    //     if (item.second.newConn == true) {
    //         hbLinkConnInfo_[group].push(std::move(item));
    //     }
    // }
}

HcclResult ClusterMonitor::UnRegisterToClusterMonitor(HcclComm comm)
{
}

HcclResult ClusterMonitor::CreateMonitorLinksAsync()
{
    std::unique_lock<std::mutex> linksLock(linksConnInfoMtx_);
    if (clusterLinkContext_.empty()) {
        return;
    }
    linkRunningStatus_ = true;
    std::queue<std::tuple<std::string, ClusterUIDType, ClusterMonitorContext>> connInfoQueue;
    for (auto &pair : clusterLinkContext_) {
        const std::string &commId = pair.first;
        auto &groupConnInfoQueue = pair.second;
        while (!groupConnInfoQueue.empty()) {
            connInfoQueue.push(
                std::make_tuple(commId, groupConnInfoQueue.front().first, groupConnInfoQueue.front().second));
            groupConnInfoQueue.pop();
        }
    }
    linksLock.unlock();
    while (!connInfoQueue.empty()) {
        const std::string commId = std::get<0>(connInfoQueue.front());
        const ClusterUIDType &remUid = std::get<1>(connInfoQueue.front());
        ClusterMonitorContext &connInfo = std::get<2>(connInfoQueue.front());
        auto it = linkThreadMap_.find(remUid);
        if (it != linkThreadMap_.end() && it->second->joinable()) {
            it->second->join();
            HCCL_INFO("[CreateMonitorLinksAsync] monitor link thread has been joined. Group[%s], remote uid[%s].",
                commId.c_str(), GetUID(remUid).c_str());
        }
        linkThreadMap_[remUid].reset(
            new (std::nothrow) std::thread(&ClusterMonitor::CreateLinkWithRemotePonit, this, commId, remUid, connInfo));
        if (linkThreadMap_[remUid] == nullptr) {
            HCCL_RUN_WARNING("Group[%s] establish rank[%s] to rank[%s] heartbeat connection failed. Reason: "
                             "create thread failed.",
                commId.c_str(), GetUID(uid_).c_str(), GetUID(remUid).c_str());
        }
        connInfoQueue.pop();
    }
    return;
}

HcclResult ClusterMonitor::CreateTransportHandle(ClusterMonitorContext &info)
{
    if (info.socketHandler == nullptr) {
        HcclResult ret = HcclCommSocketCreate(&info.socketDesc, &info.socketHandler);
        if (ret != HCCL_SUCCESS) {
            HCCL_WARNING("[CreateTransportHandle] HcclCommSocketCreate failed, ret[%d]", ret);
            return HCCL_E_NETWORK;
        }
    } else {
        HCCL_WARNING("[CreateTransportHandle] socketHandler has been created, skip.");
    }
    return HCCL_SUCCESS;
}

void ClusterMonitor::CreateLinkWithRemotePonit(
    std::string commId, ClusterUIDType rem, ClusterMonitorContext needConnectRank)
{
    // 给当前线程添加名字
    const std::string threadName = "hb" + GetUID(rem);
    SetThreadName(threadName);
    HCCL_INFO("[Heartbeat][CreateLinkWithRemote] Group[%s], thread[%s] start...", commId.c_str(), threadName.c_str());
    hrtSetDevice(deviceLogicId_);

    HcclResult ret = CreateTransportHandle(needConnectRank);
    if (ret != HCCL_SUCCESS) {
        HCCL_RUN_WARNING("[CreateLinkWithRemote]CreateTransportHandle ret[%d], commId[%s], remote uid[%s].", ret,
            commId.c_str(), GetUID(rem).c_str());
        if (deviceLogicId_ != static_cast<u32>(HOST_DEVICE_ID)) {
            hrtResetDevice(deviceLogicId_);
        }
        return;
    }
    auto CREATE_LINK_TIMEOUT = std::chrono::seconds(GetExternalInputHcclLinkTimeOut());
    auto startTime = std::chrono::steady_clock::now();
    while (linkRunningStatus_.load()) {
        if ((std::chrono::steady_clock::now() - startTime) >= CREATE_LINK_TIMEOUT) {
            HCCL_RUN_WARNING("establish rank[%s] to rank[%s] connection failed. Reason: link timeout,"
                             "timeout[%llds], the HCCL_CONNECT_TIMEOUT may be insufficient. commId[%s].",
                GetUID(uid_).c_str(), GetUID(rem).c_str(), CREATE_LINK_TIMEOUT, commId.c_str());
            break;
        }

        HcclCommSocketStatus status;
        HcclResult ret = HcclCommSocketGetStatus(needConnectRank.socketHandler, &status);
        if (ret != HCCL_SUCCESS) {
            HCCL_RUN_WARNING(
                "establish rank[%s] to rank[%s] connection failed. Reason: get socket status[%d] failed, commId[%s]",
                GetUID(uid_).c_str(), GetUID(rem).c_str(), status, commId.c_str());
            HcclCommSocketDestroy(needConnectRank.socketHandler);
            break;
        }

        if (status == HcclCommSocketStatus::SOCKET_TIMEOUT) {
            HCCL_RUN_WARNING(
                "establish rank[%s] to rank[%s] connection failed. Reason: get socket status timeout, commId[%s]",
                GetUID(uid_).c_str(), GetUID(rem).c_str(), commId.c_str());
            HcclCommSocketDestroy(needConnectRank.socketHandler);
            break;
        } else if (status == HcclCommSocketStatus::SOCKET_CONNECTING) {
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
            continue;
        }

        std::unique_lock<std::mutex> lock(threadLock_);
        if (commIdMap_.find(commId) == commIdMap_.end()) {
            HCCL_RUN_WARNING(
                "establish rank[%s] to rank[%s] connection failed. Reason: commId[%s] has been Unregistered.",
                GetUID(uid_).c_str(), GetUID(rem).c_str(), commId.c_str());
            HcclCommSocketDestroy(needConnectRank.socketHandler);
            lock.unlock();
            break;
        }
        needConnectRank.newConn = false;
        uid2ContextRefMap_.insert(rem, needConnectRank);
        // 心跳socket建链完成后，需要立即及激活其心跳收发能力
        auto frameSize = sizeof(ClusterMonitorFrame);
        if (uid2ContextRefMap_[rem].recvBuffer.Init(BASE_NUMBER * frameSize) != HCCL_SUCCESS) { // 2倍帧长，确保不会溢出
            HCCL_RUN_WARNING(
                "establish rank[%s] to rank[%s] connection failed. Reason: socket recv buffer init failed. commId[%s].",
                GetUID(uid_).c_str(), GetUID(rem).c_str(), commId.c_str());
            HcclCommSocketDestroy(needConnectRank.socketHandler);
            uid2ContextRefMap_.erase(rem);
            lock.unlock();
            break;
        }
        uid2ContextRefMap_[rem] = MonitorLinkStatus::MONITOR_LINK_COMPLETED;
        commIdMap_[commId][rem] = true; // 更新状态为已连接
        lock.unlock();
        HCCL_RUN_INFO("commId:[%s], establish rank[%s] to rank[%s] heartbeat connection success.", commId.c_str(),
            GetUID(uid_).c_str(), GetUID(rem).c_str());
        break;
    }
    if (deviceLogicId_ != static_cast<u32>(HOST_DEVICE_ID)) {
        hrtResetDevice(deviceLogicId_);
    }

    HCCL_INFO("[%s] Thread [%s] end...", __func__, threadName.c_str());
    return;
}

HcclResult ClusterMonitor::SendMonitorFrame(
    ClusterUIDType &dst, ClusterUIDType &crimer, ClusterUIDType &informer, ClusterMonitorStatus status)
{
    ClusterMonitorFrame cmFrame(uid_, dst, crimer, informer, status);
    if (uid2ContextRefMap_[dst].sendBuffer.size() > 0) {
        if (status != ClusterMonitorStatus::CLUSTER_MONITOR_OK
            && uid2ContextRefMap_[dst].sendBuffer.size() < MAX_SENDBUFF_SIZE) {
            uid2ContextRefMap_[dst].sendBuffer.push(cmFrame);
        }
        while (uid2ContextRefMap_[dst].sendBuffer.size() > 0) {
            ClusterMonitorFrame hbf = uid2ContextRefMap_[dst].sendBuffer.front();
            u64 sendDis = sizeof(ClusterMonitorFrame) - uid2ContextRefMap_[dst].restSize;
            u64 compSize = 0;
            HcclResult ret = HcclCommSocketSendNb(uid2ContextRefMap_[dst].socketHandler,
                (reinterpret_cast<void *>(&hbf) + sendDis), uid2ContextRefMap_[dst].restSize,
                (reinterpret_cast<uint64_t *>(&compSize)));
            if (ret != HCCL_SUCCESS) {
                HCCL_WARNING("[CreateTransportHandle] HcclCommSocketSendNb failed, ret[%d]", ret);
                return HCCL_E_NETWORK;
            }
            if (uid2ContextRefMap_[dst].restSize == compSize) {
                uid2ContextRefMap_[dst].sendBuffer.pop();
                uid2ContextRefMap_[dst].restSize = sizeof(ClusterMonitorFrame);
                HCCL_DEBUG("[Heartbeat][SendFrame] Send Success, from [%s] to [%s] about [%s] by [%s] status[%d]",
                    GetUID(uid_).c_str(), GetUID(dst).c_str(), GetUID(crimer).c_str(), GetUID(informer).c_str(),
                    status);
            } else {
                uid2ContextRefMap_[dst].restSize = uid2ContextRefMap_[dst].restSize - compSize;
                break;
            }
        }
    } else {
        u64 compSize = 0;
        u64 expectSize = sizeof(ClusterMonitorFrame);
        HcclResult ret = HcclCommSocketSendNb(
            uid2ContextRefMap_[dst].socketHandler, &cmFrame, expectSize, (reinterpret_cast<uint64_t *>(&compSize)));
        if (ret != HCCL_SUCCESS) {
            HCCL_WARNING("[CreateTransportHandle] HcclCommSocketSendNb failed, ret[%d]", ret);
            return HCCL_E_NETWORK;
        }
        if (compSize == expectSize) {
            HCCL_DEBUG("[Heartbeat][SendFrame] Send Success, from [%s] to [%s] about [%s] by [%s] status[%d]",
                GetUID(uid_).c_str(), GetUID(dst).c_str(), GetUID(crimer).c_str(), GetUID(informer).c_str(), status);
        } else {
            HCCL_DEBUG("[Heartbeat][SendFrame] Send Not Complete, from [%s] to [%s] about [%s] by [%s] status[%d], \
                expectSize[%u], compSize[%u]",
                GetUID(uid_).c_str(), GetUID(dst).c_str(), GetUID(crimer).c_str(), GetUID(informer).c_str(), status,
                expectSize, compSize);
            uid2ContextRefMap_[dst].restSize = expectSize - compSize;
            uid2ContextRefMap_[dst].sendBuffer.push(cmFrame);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::RecvMonitorFrame(ClusterUIDType &src)
{
    ClusterMonitorFrame cmFrame;
    u64 compSize = 0;
    u64 expectSize = sizeof(ClusterMonitorFrame);
    while (true) {
        compSize = 0;
        HcclResult ret = HcclCommSocketRecvNb(
            uid2ContextRefMap_[src].socketHandler, &cmFrame, expectSize, (reinterpret_cast<uint64_t *>(&compSize)));
        if (ret == HCCL_SUCCESS && compSize > 0) {
            uid2ContextRefMap_[src].recvBuffer.PushSeg(reinterpret_cast<u8 *>(&cmFrame), compSize);
            if (uid2ContextRefMap_[src].recvBuffer.Size() >= expectSize) {
                uid2ContextRefMap_[src].recvBuffer.GetSeg(reinterpret_cast<u8 *>(&cmFrame), expectSize);
                uid2ContextRefMap_[src].recvBuffer.PopSeg(expectSize);
                CHK_RET(ParseMonitorFrame(cmFrame, src));
            }
        } else if (ret == HCCL_E_INTERNAL) {
            return HCCL_E_INTERNAL;
        } else {
            break;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::ParseMonitorFrame(ClusterMonitorFrame &cmFrame, ClusterUIDType &src)
{
    if (cmFrame.src != src || cmFrame.dst != uid_) {
        HCCL_WARNING("rank[%s] recv wrong frame", GetUID(uid_).c_str());
        return HCCL_E_INTERNAL;
    }

    HCCL_DEBUG("[ClusterMonitor][ParseMonitorFrame] Recv Success, from [%s] to [%s] about [%s] by [%s] state[%d]",
        GetUID(cmFrame.src).c_str(), GetUID(cmFrame.dst).c_str(), GetUID(cmFrame.crimer).c_str(),
        GetUID(cmFrame.informer).c_str(), cmFrame.status);

    // 能够收到进程卡住表示心跳是正常的
    if (cmFrame.status == ClusterMonitorStatus::CLUSTER_MONITOR_OK
        || cmFrame.status == ClusterMonitorStatus::CLUSTER_MONITOR_STUCK) { // 去掉stuck的判断
        uid2ContextRefMap_[src].lostNum = 0;
    }

    // 只有心跳非正常时才需要打印TRACE
    if (cmFrame.status != ClusterMonitorStatus::CLUSTER_MONITOR_OK) {
        // SetStatus(cmFrame.crimer, cmFrame.informer, cmFrame.status);  设置异常状态
    }

    return HCCL_SUCCESS;
}

} // namespace hccl