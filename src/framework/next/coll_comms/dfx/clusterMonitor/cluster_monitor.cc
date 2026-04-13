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
#include "comm_addr_logger.h"

namespace hccl {

HcclResult ClusterMonitor::FormatUID(ClusterUidCtxt ctxt, ClusterUIDType &uid)
{
    // 构造唯一的uid: netInstanceId + local_id
    s32 ret = snprintf_s(uid.id, sizeof(uid.id), sizeof(uid.id) - 1, "%s%s%s",
        ctxt.netInstId.c_str(), "/", std::to_string(ctxt.localId).c_str());
    CHK_PRT_RET((ret == -1),
        HCCL_ERROR("[%s] snprintf_s failed, errno:%d, error:%s",
            __func__, errno, strerror(errno)), HCCL_E_SYSCALL);

    return HCCL_SUCCESS;
}

std::string ClusterMonitor::GetUID(const ClusterUIDType &uid) const
{
    return uid.id;
}

uint32_t ClusterMonitor::GetPhyId(EndpointDesc desc) const
{
    uint32_t phyId = 0;
    if (desc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        phyId = desc.device.devPhyId;
    } else if (desc.loc.locType == ENDPOINT_LOC_TYPE_HOST) {
        phyId = desc.host.id;
    } else {
        HCCL_ERROR("[%s] locType invalid, is[%d]", __func__, desc.loc.locType);
    }

    return phyId;
}

HcclResult ClusterMonitor::GetRemEndpointDescs(hccl::CollComm* collComm,
    std::map<uint32_t, std::vector<CommLinkContext>> &commLinks,
    uint32_t *netLayerNum)
{
    // 将所有远端的rank都加入到状态维护map中
    uint32_t *netLayers;
    auto myRankId = collComm->GetMyRankId();
    auto rankGraph = collComm->GetRankGraph();
    CHK_PTR_NULL(rankGraph);
    CHK_RET(HcclRankGraphGetLayers(comm, &netLayers, netLayerNum));
    std::vector<uint32_t> netLayersVector(netLayers, netLayers + *netLayerNum);
    for (auto netLayer : netLayersVector) {
        const NetInstance *netInstance = rankGraph->GetNetInstanceByRankId(netLayer, myRankId);
        CHK_PTR_NULL(netInstance); // 有可能netInstance为空
        auto netInstanceId = netInstance->GetNetInstId();
        auto peersMap = netInstance->GetPeers();
        std::set<RankId> rankSet = netInstance->GetRankIds();
        for (RankId rankId : rankSet) {
            if (peersMap.find(rankId) == peersMap.end()) {
                HCCL_ERROR("rankId[%u] not found in peersMap", rankId);
                return HCCL_E_PARA;
            }
            auto localId = peersMap[rankId].GetLocalId();
            ClusterUidCtxt uidCtxt(localId, netInstanceId);
            if (rankId == myRankId) {
                if (netLayer == 0) {
                    CHK_RET(FormatUID(uidCtxt, myRankUid_));
                    localId_ = localId;
                }
                continue;
            }
            if (netLayer == 0) {
                // 构造UID存入map里
                ClusterUIDType remoteUID{};
                CHK_RET(FormatUID(uidCtxt, remoteUID));
                commLinkCtx.remoteUid = remoteUID;
                uid2FrameStatusMap_.insert(remoteUID, FrameStatus());
                commIdMap_[collComm->GetCommId()].insert(std::make_pair(remoteUID, false)); //初始状态均为未连接
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
            if (commLinks.find(netLayer) == commLinks.end()) {
                commLinks.insert(std::make_pair(netLayer, std::vector<CommLinkContext>()));
            }
            CommLinkContext commLinkCtx{};
            commLinkCtx.commLink = linkList[0];
            commLinkCtx.localRank = myRankId;
            commLinkCtx.remoteRank = rankId;
            commLinkCtx.localId = localId;
            commLinks[netLayer].emplace_back(commLinkCtx);
            HCCL_INFO("commId[%s] insert my rank[%u] remoteUID[%s]",
                commId.c_str(), myRankId, GetUID(remoteUID).c_str());
        }
    }

    return HCCL_SUCCESS;
}

std::string ClusterMonitor::FormatConnTag(HcommSocketRole role,
    std::pair<ClusterUIDType, ClusterUIDType> uidPair)
{
    std::string tag;
    if (role == HcommSocketRole::HCOMM_SOCKET_ROLE_CLIENT) {
        tag = "HeartBeat_" + GetUID(uidPair.first) + "_to_" + GetUID(uidPair.second);
    } else {
        tag = "HeartBeat_" + GetUID(uidPair.second) + "_to_" + GetUID(uidPair.first);
    }

    return tag;
}

HcclResult ClusterMonitor::InsertClusterMonitorCxt(hccl::CollComm* collComm, CommLinkContext commLinkCtx,
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank)
{
    bool newConn = true;
    HcommSocketDesc socketDesc{};
    std::string remoteUid = commLinkCtx.remoteUid;
    uint32_t localRank = commLinkCtx.localRank;
    uint32_t remoteRank = commLinkCtx.remoteRank;
    CommLink commLink = commLinkCtx.commLink;

    // socket建链需要心跳专用的tag，用来区分业务的socket以及心跳的sockt
    socketDesc.tag = FormatConnTag(role, std::make_pair(myRankUid_, remoteUid));

    std::unique_lock<std::mutex> lock(threadLock_);
    if (monitorLinkStatusMap_.find(remoteUid) == monitorLinkStatusMap_.end()) {
        monitorLinkStatusMap_[remoteUid] = MonitorLinkStatus::MONITOR_LINK_NOT_START;
    } else if (monitorLinkStatusMap_[remoteUid] == MonitorLinkStatus::MONITOR_LINK_BUILDING ||
        monitorLinkStatusMap_[remoteUid] == MonitorLinkStatus::MONITOR_LINK_COMPLETED) {
        newConn = false;// 说明之前已经有remoteUID在建链
    }

    // 获取端口号用来建链
    uint32_t rmtPort = 0;
    uint32_t listenPort = 0;
    auto rankGraph = collComm->GetRankGraph();
    CHK_PTR_NULL(rankGraph);
    CHK_RET(rankGraph->GetDevicePort(remoteRank, &rmtPort));
    if (rmtPort > Hccl::MAX_VALUE_DEVICEPORT) {
        HCCL_ERROR("[%s] Invalid port[%u] of Rank[%u]", __func__, rmtPort, remoteRank);
        return HCCL_E_PARA;
    }
    // 查询该socket链接的server端监听的端口（监听方的选择策略需要跟SocketConfig中保持一致）
    Hccl::IpAddress localIpAddr{};
    Hccl::IpAddress remoteIpAddr{};
    CHK_RET(CommAddrToIpAddress(commLink.srcEndpointDesc.commAddr, localIpAddr));
    CHK_RET(CommAddrToIpAddress(commLink.dstEndpointDesc.commAddr, remoteIpAddr));
    if (localIpAddr < remoteIpAddr) { // local地址比remote地址小时，local作为server监听端
        // 查询localRankId对应的devPort
        CHK_RET(rankGraph->GetDevicePort(localRank, &listenPort));
        socketDesc.role = HcommSocketRole::HCOMM_SOCKET_ROLE_SERVER;
        if (listenPort > Hccl::MAX_VALUE_DEVICEPORT) {
            HCCL_ERROR("[%s] Invalid port[%u] of Rank[%u]", __func__, listenPort, localRank);
            return HCCL_E_PARA;
        }
        socketDesc.port = (uint16_t)listenPort; // socketDesc.port中填监听端口号
    } else {
        socketDesc.role = HcommSocketRole::HCOMM_SOCKET_ROLE_CLIENT;
        socketDesc.port = (uint16_t)rmtPort; // socketDesc.port中填对端端口号(此场景下对端端口号也就是监听端口号)
    }

    socketDesc.localEndpoint = commLink.srcEndpointDesc;
    socketDesc.remoteEndpoint = commLink.dstEndpointDesc;
    ClusterMonitorSocketCtx ctx(newConn, socketDesc);
    needConnectRank.insert(std::make_pair(remoteUid, ctx));

    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::GetSamePlaneRank(hccl::CollComm* collComm, std::vector<CommLinkContext> singlePlaneLinks,
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank)
{
    uint32_t singlePlaneSize = singlePlaneLinks.size(); // myRank可以与本平面连接的边数量，netLayer=0时已经按照devicePhyId排序好
    if (singlePlaneSize < 1) { // 以本rank为出发点，待连接的节点个数为0，无需连接
        HCCL_INFO("[%s] no need to connect", __func__);
        return HCCL_SUCCESS;
    } else if (singlePlaneSize == 1) { // 以本rank为出发点，待连接的节点个数为1，对于ring环来说没有回绕的边
        CommAddrLogger::CommLinkPrint(singlePlaneLinks[0].second.commLink);
        CHK_RET(InsertClusterMonitorCxt(collComm, singlePlaneLinks[0], needConnectRank));
    } else {
        std::pair<uint32_t, CommLinkContext> nextCtx = singlePlaneLinks[0];
        std::pair<uint32_t, CommLinkContext> preCtx = singlePlaneLinks[singlePlaneSize - 1];
        CHK_RET(InsertClusterMonitorCxt(collComm, nextCtx, needConnectRank)); //以本rank为起点，环的右手
        CHK_RET(InsertClusterMonitorCxt(collComm, preCtx, needConnectRank)); // 以本rank为起点，环的左手
    }

    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::GetConnectRank(hccl::CollComm* collComm,
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank,
    std::map<uint32_t, std::vector<CommLinkContext>> commLinksCtx, uint32_t netLayerNum)
{
    if (netLayerNum == 0) {
        return HCCL_SUCCESS;
    }

    std::vector<CommLinkContext> layer0CommLinks; // 需要存入CommLinkContext，待后续查出对应的port/remoteUid
    // 先处理netLayer=0，按照netLayer=0全局唯一的localId升序排列，在level0不需要考虑host网卡的场景，host网卡只会在level1及以上的层级
    std::sort(commLinksCtx[0].begin(), commLinksCtx[0].end(), [&](const CommLinkContext& a, const CommLinkContext& b) {
        return a.localId < b.localId;
    });
    for (auto it = commLinksCtx[0].begin(); it != commLinksCtx[0].end(); ++it) {
        layer0CommLinks.push_back(it); // netLayer为0
    }

    // 从layer=1开始，将commLinks存入vector中，找到所有与当前localId相同的节点
    std::vector<CommLinkContext> highLayerCommLinks;
    for (uint32_t netLayer = 1; netLayer < netLayerNum; netLayer++) {
        for (auto it = commLinksCtx[netLayer].begin(); it != commLinksCtx[netLayer].end(); ++it) {
            if (it.localId == this->localId_) {
                // 在跨server、跨pod、跨超节点的场景，统一拿到local，打平处理为同一个平面，类似layer=0的情况
                // 由于A5上的devPhyId在64卡的场景下8个[0,7]，所以使用localId
                highLayerCommLinks.push_back(it);
            }
        }
    }

    // 每个平面都分别成环
    CHK_RET(GetSamePlaneRank(collComm, layer0CommLinks, needConnectRank));
    CHK_RET(GetSamePlaneRank(collComm, highLayerCommLinks, needConnectRank));
    return HCCL_SUCCESS;
}

void ClusterMonitor::CreateHBLinksAsync()
{
    // 异步建链线程
}

void ClusterMonitor::SetStatus(ClusterUIDType &crimer, ClusterUIDType &informer,
    ClusterMonitorStatus status, bool needBroadcast)
{
    if (uid2FrameStatusMap_[crimer].status != status) {
        uid2FrameStatusMap_[crimer].informer = informer;
        uid2FrameStatusMap_[crimer].status = status;
        uid2FrameStatusMap_[crimer].needBroadcast = needBroadcast;
        if (needBroadcast) {
            errRankQueue_.push(crimer);
        }

        errStatusQueue_.push(ClusterMonitorFrame(crimer, informer, status, TIME_NOW(), std::chrono::system_clock::now()));
        if (errStatusQueue_.size() > EVENT_MAX_CNT) {
            errStatusQueue_.pop();
        }
        // HCCL_RUN_INFO("[%s][%s]local rank [%s]: crimer rank [%s] status[%s] by informer rank [%s]",
        //     LOG_KEYWORDS_TASK_EXEC.c_str(), LOG_KEYWORDS_HEARTBEAT_EVETN.c_str(), FormatUId(uid_).c_str(),
        //     FormatUId(crimer).c_str(), GetHeartBeatStatusStr(status).c_str(), FormatUId(informer).c_str());
    }
}

void ClusterMonitor::MonitorThread()
{
    // 给当前线程添加名字
    SetThreadName("Hccl_HeartBeat");

    hrtSetDevice(deviceLogicId_);
    HcclResult ret;
    uint32_t count;
    while (clusterMonitorThreadFlag_) {
        CreateHBLinksAsync(); // 内部起线程对所有的socket进行异步建链
        threadLock_.lock();
        count++;
        if (count >= HEARTBEAT_COUNT) {
            count = 0;
            for (auto iter = uid2SocketRefMap_.begin(); iter != uid2SocketRefMap_.end(); iter++) {
                ClusterUIDType rem = iter->first;
                uid2SocketRefMap_[rem].lostNum++;
                SendFrame();
            }
            DelErrorSocket(); // 处理socket错误的句柄
        }

        for (auto iter = uid2SocketRefMap_.begin(); iter != uid2SocketRefMap_.end(); iter++) {
            ClusterUIDType rem = iter->first;
            ret = RecvFrame(rem);
            if (ret == HCCL_E_INTERNAL) {
                errorSocket_.push_back(rem);
            } else if (uid2SocketRefMap_[rem].lostNum >= lostThreshold_) {
                SetStatus(rem, uid_, ClusterMonitorStatus::CLUSTER_MONITOR_LOST);
            }
        }
        DelErrorSocket(); // 处理socket错误的句柄
        ProcessExceptionEvent(); // 处理error cqe
        threadLock_.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(BROADCAST_INTERVAL));
    }

    linkThreadRunning_ = false;
    // 在心跳进程结束之前join所有的建链线程
    for (auto &pair : linkThreadMap_) {
        if (pair.second != nullptr && pair.second->joinable()) {
            pair.second->join();
            HCCL_INFO("[%s] thread has joined. Remote uid is [%s]", __func__, GetUId(pair.first).c_str());
        }
    }

    hrtResetDevice(deviceLogicId_);
}

HcclResult ClusterMonitor::RunMonitorThread()
{
    clusterMonitorThreadFlag_ = true;
    clusterMonitorThread_.reset(new (std::nothrow) std::thread(&ClusterMonitor::MonitorThread, this));
    CHK_SMART_PTR_NULL(clusterMonitorThread_);
    lostThreshold_ = 30; // 心跳丢失阈值为30s
    initialized_ = true;
    isDeInit_ = false;
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

    // 对象中的私有成员变量赋值
    deviceLogicId_ = collComm->deviceLogicId_;
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(collComm->deviceLogicId_), devicePhyId_));

    if (!initialized_) {
        // 开始起监控线程
        CHK_RET(RunMonitorThread());
    }

    // 存放以myRank为起点，netLayer为key，可以连接到的所有对端的<rankid, CommLink>
    std::map<uint32_t, std::vector<CommLinkContext>> commLinksCtx;
    uint32_t netLayerNum = 0;

    // 获取从myRank出发，所有的对端，并维护commIdMap_及uid2FrameStatusMap_
    lock.lock();
    CHK_RET(GetRemEndpointDescs(collComm, commLinksCtx, &netLayerNum));
    lock.unlock();

    // 解析heartbeat环境变量，如果配置为off则不去注册对应的rank
    if (!EnvConfig::GetInstance().GetLogConfig().GetDfsConfig().cluseterHeartBeatEnable) {
        HCCL_RUN_INFO("[%s] HCCL_DFS_CONFIG cluster_heartbeat is off. It's unnecessary to "
            "register Ranks. commId[%s]", __func__, commId.c_str());
        return HCCL_SUCCESS;
    }

    // 从所有连接中，选择双ring环，存放到needConnectRank
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> needConnectRank;
    CHK_RET(GetConnectRank(needConnectRank, commLinksCtx, netLayerNum));

    // 将双ring环的pair放入clusterLinkContext_管理多个通信域
    std::unique_lock<std::mutex> linkCtxlock(clusertMonitorLinkMtx_);
    for (auto &item : needConnectRank) {
        if (item.second.newConn == true) {
            // 一旦放入clusterLinkContext_中，就会被后台的异步建链线程推动建链
            clusterLinkContext_[commId].push(std::move(item));
        }
    }
    linkCtxlock.unlock();

    lock.lock();
    for (auto &item : needConnectRank) {
        if (item.second.newConn == true) {
            // 由于newConn==true的item已经入队，后台推动异步建链，所以状态迁移为建链中
            monitorLinkStatusMap_[item.first] = MonitorLinkStatus::MONITOR_LINK_BUILDING;
        } else if (commIdMap_[commId].find(item.first) == commIdMap_[commId].end() ||
            (commIdMap_[commId].count(item.first) && !commIdMap_[commId][item.first])) {
            // 若newConn=false，说明不是新增的连接
            // 1. 通信域找不到，2.通信域内能找到但还没有连接，计数++
            uid2SocketRefMap_.ref(item.first);
            // HCCL_RUN_INFO("commId:[%s], establish rank[%s] to rank[%s] heartbeat connection success.", group.c_str(),
            //     FormatUId(uid_).c_str(), FormatUId(item.first).c_str());
            commIdMap_[commId][item.first] = true; // 认为通信域中对应的连接已经建立
        }
    }
    lock.unlock();

    HCCL_INFO("[%s] commId[%s] RegisterRanks Completed", __func__, commId.c_str());
    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::UnRegisterToClusterMonitor(HcclComm comm)
{

}

}// namespace hccl