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
#include "hccl_comm_pub.h"
#include "op_base_v2.h"
#include "env_config/env_config.h"

namespace hcomm {
constexpr u32 EVENT_MAX_CNT = 5000;          // 防止内存泄漏，同时不能太短，防止正常事件被冲掉
constexpr u32 HEARTBEAT_INTERVAL = 1000;                                 // 心跳帧发送周期为1000 ms
constexpr u32 BROADCAST_INTERVAL = 50; // 背景线程执行周期为50 ms
constexpr u32 HEARTBEAT_COUNT = HEARTBEAT_INTERVAL / BROADCAST_INTERVAL; // 心跳帧发送间隔数

HcclResult ClusterMonitor::FormatUID(ClusterUidCtxt ctxt, ClusterUIDType &uid)
{
    // 构造唯一的uid: netInstanceId + local_id
    s32 ret = snprintf_s(uid.id, sizeof(uid.id), sizeof(uid.id) - 1, "%s/%s",
        ctxt.netInstId.c_str(), std::to_string(ctxt.localId).c_str());
    CHK_PRT_RET((ret == -1),
        HCCL_ERROR("[%s] snprintf_s failed, errno:%d, error:%s",
            __func__, errno, strerror(errno)), HCCL_E_SYSCALL);

    return HCCL_SUCCESS;
}

std::string ClusterMonitor::GetUID(const ClusterUIDType &uid) const
{
    return uid.id;
}

HcclResult ClusterMonitor::GetRemEndpointDescs(HcclComm comm,
    std::map<uint32_t, std::vector<UidContext>> &uidctxs,
    uint32_t *netLayerNum)
{
    // 将所有远端的rank都加入到状态维护map中
    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    Hccl::HcclCommunicator* commV2 = static_cast<Hccl::HcclCommunicator*>(collComm->GetCommunicatorV2());
    CHK_PTR_NULL(commV2); // 获取到legacy communicator，说明v2通信域
    void *rankGraphPtr = nullptr;
    commV2->GetRankGraphV2(rankGraphPtr);
    CHK_PTR_NULL(rankGraphPtr);

    uint32_t *netLayers = nullptr;
    CHK_RET(HcclRankGraphGetLayers(commV2, &netLayers, netLayerNum));
    if (*netLayerNum == 0) {
        HCCL_ERROR("[%s] no netLayer in RankGraph", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    auto myRankId = collComm->GetMyRankId();
    Hccl::RankGraph *rankGraph = static_cast<Hccl::RankGraph*>(rankGraphPtr);
    std::vector<uint32_t> netLayersVector(netLayers, netLayers + *netLayerNum);
    std::set<uint32_t> rankIdsSet; // 存放通信域的唯一标识ranks，防止在netLayer>=1的时候，查到了netLayer=0已经存放的ranks
    for (auto netLayer : netLayersVector) {
        uint32_t *RanksPerLayer = nullptr;
        uint32_t rankNum = 0;
        HcclRankGraphGetRanksByLayer(comm, netLayer, &RanksPerLayer, &rankNum); // 获取每层netLayer的所有rank
        for (uint32_t rankIdx = 0; rankIdx < rankNum; rankIdx++) {
            uint32_t rankId = RanksPerLayer[rankIdx];
            if (rankIdsSet.find(rankId) != rankIdsSet.end()) { // rankSet维护了所有的ranks，如果已经加到Set说明该rank已经在更低的netLayer层级加入
                continue;
            }
            rankIdsSet.insert(rankId);
            auto *netInstance = rankGraph->GetNetInstanceByRankId(0, rankId); // 查询对应rankId在netLayer=0的netInsId
            CHK_PTR_NULL(netInstance); // 有可能netInstance为空
            auto netInstanceId = netInstance->GetNetInstId();
            auto localId = rankGraph->GetLocalId(rankId); // 根据rank查localId
            ClusterUidCtxt uidCtxt(netInstanceId, localId);
            ClusterUIDType uid{};
            CHK_RET(FormatUID(uidCtxt, uid));
            if (myRankId == rankId) {
                myRankUid_ = uid;
                localId_ = localId;
            }
            uid2FrameStatusMap_.insert(uid, FrameStatus());
            commIdMap_[collComm->GetCommId()].insert(std::make_pair(uid, false)); //初始状态均为未连接，包含自己
            if (uidctxs.find(netLayer) == uidctxs.end()) {
                uidctxs.insert(std::make_pair(netLayer, std::vector<UidContext>()));
            }
            UidContext uidCtx(uid, netLayer, rankId, localId);
            uidctxs[netLayer].emplace_back(uidCtx);
            HCCL_INFO("commId[%s] insert remoteUID[%s]",
                collComm->GetCommId().c_str(), GetUID(uid).c_str());
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

HcclResult ClusterMonitor::InsertClusterMonitorCxt(HcclComm comm, UidContext remoteCtx,
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank)
{
    bool newConn = true;
    HcommSocketDesc socketDesc{};
    auto remoteUid = remoteCtx.uid;
    auto remoteRank = remoteCtx.rankId;
    auto netLayer = remoteCtx.netLayer;

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
    hccl::CollComm* collComm = static_cast<hccl::hcclComm*>(comm)->GetCollComm();
    auto rankGraph = collComm->GetRankGraph();
    auto myRankId = collComm->GetMyRankId();
    CHK_PTR_NULL(rankGraph);
    CHK_RET(rankGraph->GetDevicePort(remoteRank, &rmtPort));
    if (rmtPort > Hccl::MAX_VALUE_DEVICEPORT) {
        HCCL_ERROR("[%s] Invalid port[%u] of Rank[%u]", __func__, rmtPort, remoteRank);
        return HCCL_E_PARA;
    }
    CommLink *links = nullptr;
    uint32_t linkNum = 0;
    CHK_RET(HcclRankGraphGetLinks(comm, netLayer, myRankId, remoteRank, &links, &linkNum));
    if (linkNum == 0) {
        HCCL_ERROR("[%s]no links betwen myRank[%u] to remoteRankId[%u]", __func__, myRankId, remoteRank);
        return HCCL_E_PARA;
    }
    // 查询该socket链接的server端监听的端口（监听方的选择策略需要跟SocketConfig中保持一致）
    Hccl::IpAddress localIpAddr{};
    Hccl::IpAddress remoteIpAddr{};
    CHK_RET(CommAddrToIpAddress(links[0].srcEndpointDesc.commAddr, localIpAddr));
    CHK_RET(CommAddrToIpAddress(links[0].dstEndpointDesc.commAddr, remoteIpAddr));
    if (localIpAddr < remoteIpAddr) { // local地址比remote地址小时，local作为server监听端
        // 查询localRankId对应的devPort
        CHK_RET(rankGraph->GetDevicePort(myRankId, &listenPort));
        socketDesc.role = HcommSocketRole::HCOMM_SOCKET_ROLE_SERVER;
        if (listenPort > Hccl::MAX_VALUE_DEVICEPORT) {
            HCCL_ERROR("[%s] Invalid port[%u] of Rank[%u]", __func__, listenPort, myRankId);
            return HCCL_E_PARA;
        }
        socketDesc.port = (uint16_t)listenPort; // socketDesc.port中填监听端口号
    } else {
        socketDesc.role = HcommSocketRole::HCOMM_SOCKET_ROLE_CLIENT;
        socketDesc.port = (uint16_t)rmtPort; // socketDesc.port中填对端端口号(此场景下对端端口号也就是监听端口号)
    }
    // socket建链需要心跳专用的tag，用来区分业务的socket以及心跳的sockt
    socketDesc.tag = FormatConnTag(socketDesc.role, std::make_pair(myRankUid_, remoteUid));
    socketDesc.localEndpoint = links[0].srcEndpointDesc;
    socketDesc.remoteEndpoint = links[0].dstEndpointDesc;
    ClusterMonitorSocketCtx ctx(socketDesc, newConn);
    needConnectRank.insert(std::make_pair(remoteUid, ctx));

    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::GetSamePlaneRank(HcclComm comm, std::vector<UidContext> singlePlaneCtx,
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank)
{
    uint32_t index = 0;
    for (; index < singlePlaneCtx.size();index++) {
        if (singlePlaneCtx[index].uid == this->myRankUid_) { // 找出myRank在vector中的下标
            break;
        }
    }

    uint32_t singlePlaneSize = singlePlaneCtx.size(); // 包含myRank自己，一个平面所有的节点
    if (singlePlaneSize <= 1) { // 待连接的节点个数为0或1，无需连接
        HCCL_INFO("[%s] no need to connect", __func__);
        return HCCL_SUCCESS;
    } else if (singlePlaneSize == 2) { // 待连接的节点个数为2，不需要双ring环，一条边就够了
        uint32_t nextIndex = (index + 1) % singlePlaneSize; // 算出与本Rank相连，对端的节点
        CHK_RET(InsertClusterMonitorCxt(comm, singlePlaneCtx[nextIndex], needConnectRank));
    } else {
        uint32_t nextIndex = (index + 1) % singlePlaneSize; // 算出与本Rank相连，右手的节点
        uint32_t preIndex = (index + singlePlaneSize - 1) % singlePlaneSize; // 算出与本Rank相连，左手或回绕环的节点
        CHK_RET(InsertClusterMonitorCxt(comm, singlePlaneCtx[nextIndex], needConnectRank)); //以本rank为起点，环的右手
        CHK_RET(InsertClusterMonitorCxt(comm, singlePlaneCtx[preIndex], needConnectRank)); // 以本rank为起点，环的左手
    }

    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::GetConnectRank(HcclComm comm,
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank,
    std::map<uint32_t, std::vector<UidContext>> uidctxs, uint32_t netLayerNum)
{
    if (netLayerNum == 0) {
        return HCCL_SUCCESS;
    }

    std::vector<UidContext> layer0CommLinks; // 需要存入UidContext，待后续查出对应的port/remoteUid
    // 先处理netLayer=0，按照netLayer=0全局唯一的localId升序排列，在level0不需要考虑host网卡的场景，host网卡只会在level1及以上的层级
    std::sort(uidctxs[0].begin(), uidctxs[0].end(), [&](const UidContext& a, const UidContext& b) {
        return a.localId < b.localId;
    });
    for (auto it = uidctxs[0].begin(); it != uidctxs[0].end(); ++it) {
        layer0CommLinks.push_back(*it); // netLayer为0
    }

    // 从layer=1开始，将commLinks存入vector中，找到所有与当前localId相同的节点
    std::vector<UidContext> highLayerCommLinks;
    for (uint32_t netLayer = 1; netLayer < netLayerNum; netLayer++) {
        for (auto it = uidctxs[netLayer].begin(); it != uidctxs[netLayer].end(); ++it) {
            if (it->localId == this->localId_) {
                // 在跨server、跨pod、跨超节点的场景，统一拿到local，打平处理为同一个平面，类似layer=0的情况
                // 由于A5上的devPhyId在64卡的场景下8个[0,7]，所以使用localId
                highLayerCommLinks.push_back(*it);
            }
        }
    }

    // 每个平面都分别成环
    CHK_RET(GetSamePlaneRank(comm, layer0CommLinks, needConnectRank));
    CHK_RET(GetSamePlaneRank(comm, highLayerCommLinks, needConnectRank));
    return HCCL_SUCCESS;
}

void ClusterMonitor::CreateHBLinksAsync()
{
    // 异步建链线程
}

void ClusterMonitor::SendFrame()
{
    // TODO:遍历所有的连接，发送心跳帧
}

void ClusterMonitor::DelErrorSocket()
{
    // TODO:处理socket错误的句柄
}

void ClusterMonitor::ProcessExceptionEvent()
{
    // TODO:处理error cqe
}

HcclResult ClusterMonitor::RecvFrame(ClusterUIDType rem)
{
    // TODO:遍历所有的连接，接收心跳帧
    return HCCL_SUCCESS;
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
    uint32_t count = 0;
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
                SetStatus(rem, myRankUid_, ClusterMonitorStatus::CLUSTER_MONITOR_LOST, true);
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
            HCCL_INFO("[%s] thread has joined. Remote uid is [%s]", __func__, GetUID(pair.first).c_str());
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
    // TODO:请注意：如果是send-recv算子要单独处理对端
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
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(collComm->GetDeviceLogicId()), devicePhyId_));

    if (!initialized_) {
        // 开始起监控线程
        CHK_RET(RunMonitorThread());
    }

    // 存放所有节点的rankId/localId/netLayer/uid
    std::map<uint32_t, std::vector<UidContext>> uidctxs;
    uint32_t netLayerNum = 0;

    // 获取从myRank出发，所有的对端，并维护commIdMap_及uid2FrameStatusMap_
    lock.lock();
    CHK_RET(GetRemEndpointDescs(comm, uidctxs, &netLayerNum));
    lock.unlock();

    // 解析heartbeat环境变量，如果配置为off则不去注册对应的rank
    auto clusterHeartBeatEnable = Hccl::EnvConfig::GetInstance().GetLogConfig().GetDfsConfig().clusterHeartBeatEnable;
    if (!clusterHeartBeatEnable) {
        HCCL_RUN_INFO("[%s] HCCL_DFS_CONFIG cluster_heartbeat is off. It's unnecessary to "
            "register Ranks. commId[%s]", __func__, commId.c_str());
        return HCCL_SUCCESS;
    }

    // 从所有连接中，选择双ring环，存放到needConnectRank
    std::map<ClusterUIDType, ClusterMonitorSocketCtx> needConnectRank;
    CHK_RET(GetConnectRank(comm, needConnectRank, uidctxs, netLayerNum));

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
    return HCCL_SUCCESS;
}

}// namespace hccl