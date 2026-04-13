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
    // 构造唯一的uid: netLayer/phyId/commAddrStr
    s32 ret = snprintf_s(uid.id, sizeof(uid.id), sizeof(uid.id) - 1, "%s%s%s%s%s",
        std::to_string(ctxt.netLayer).c_str(), "/",
        std::to_string(ctxt.phyId).c_str(), "/",
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
    auto myRank = collComm->GetMyRank();
    auto rankGraph = collComm->GetRankGraph();
    CHK_PTR_NULL(rankGraph);
    CHK_RET(HcclRankGraphGetLayers(comm, &netLayers, netLayerNum));
    std::vector<uint32_t> netLayersVector(netLayers, netLayers + *netLayerNum);

    for (auto netLayer : netLayersVector) {
        const NetInstance *netInstance = rankGraph->GetNetInstanceByRankId(netLayer, myRank);
        CHK_PTR_NULL(netInstance); // 有可能netInstance为空
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
            if (commLinks.find(netLayer) == commLinks.end()) {
                commLinks.insert(std::make_pair(netLayer, std::vector<CommLinkContext>()));
            }
            CommLinkContext commLinkCtx{};
            commLinkCtx.commLink = linkList[0];
            commLinkCtx.localRank = myRank;
            commLinkCtx.remoteRank = rankId;
            commLinks[netLayer].emplace_back(commLinkCtx);

            // 构造UID存入map里
            ClusterUidCtxt uidCtxt(netLayer, GetPhyId(linkList[0].dstEndpointDesc),
                CommAddr2Str(linkList[0].dstEndpointDesc.commAddr));
            ClusterUIDType remoteUID{};
            CHK_RET(FormatUID(uidCtxt, remoteUID));
            uid2FrameStatusMap_.insert(remoteUID, FrameStatus());
            commIdMap_[collComm->GetCommId()].insert(std::make_pair(remoteUID, false)); //初始状态均为未连接
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

HcclResult ClusterMonitor::InsertClusterMonitorCxt(hccl::CollComm* collComm, std::pair<uint32_t, CommLinkContext> commLinkCtx,
    std::map<ClusterUIDType, ClusterMonitorContext> &needConnectRank)
{
    bool newConn = true;
    HcommSocketDesc socketDesc{};
    uint32_t netLayer = commLinkCtx.first;
    uint32_t localRank = commLinkCtx.second.localRank;
    uint32_t remoteRank = commLinkCtx.second.remoteRank;
    CommLink commLink = commLinkCtx.second.commLink;


    // 构造本端的UID
    ClusterUidCtxt srcUidCtxt(netLayer, GetPhyId(commLink.srcEndpointDesc),
        CommAddr2Str(commLink.srcEndpointDesc.commAddr));
    ClusterUIDType srcUID{};
    CHK_RET(FormatUID(srcUidCtxt, srcUID));

    // 构造远端的UID
    ClusterUidCtxt dstUidCtxt(netLayer, GetPhyId(commLink.dstEndpointDesc),
        CommAddr2Str(commLink.commAddr));
    ClusterUIDType dstUID{};
    CHK_RET(FormatUID(dstUidCtxt, dstUID));

    // socket建链需要心跳专用的tag，用来区分业务的socket以及心跳的sockt
    socketDesc.tag = FormatConnTag(role, std::make_pair(srcUID, dstUID));

    std::unique_lock<std::mutex> lock(threadLock_);
    if (monitorLinkStatusMap_.find(dstUID) == monitorLinkStatusMap_.end()) {
        monitorLinkStatusMap_[dstUID] = MonitorLinkStatus::MONITOR_LINK_NOT_START;
    } else if (monitorLinkStatusMap_[dstUID] == MonitorLinkStatus::MONITOR_LINK_BUILDING ||
        monitorLinkStatusMap_[dstUID] == MonitorLinkStatus::MONITOR_LINK_COMPLETED) {
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
    ClusterMonitorContext ctx(newConn, socketDesc);
    needConnectRank.insert(std::make_pair(dstUID, ctx));

    return HCCL_SUCCESS;
}

HcclResult ClusterMonitor::GetSamePlaneRank(hccl::CollComm* collComm, std::vector<std::pair<uint32_t, CommLinkContext>> singlePlaneLinks,
    std::map<ClusterUIDType, ClusterMonitorContext> &needConnectRank)
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
    std::map<ClusterUIDType, ClusterMonitorContext> &needConnectRank,
    std::map<uint32_t, std::vector<CommLinkContext>> commLinks, uint32_t netLayerNum)
{
    if (netLayerNum == 0) {
        return HCCL_SUCCESS;
    }

    std::vector<std::pair<uint32_t, CommLinkContext>> layer0CommLinks; // 需要存入netLayer, CommLinkContext
    // 先处理netLayer=0，按照devicePhyId升序排列，在level0不需要考虑host网卡的场景，host网卡只会在level1及以上的层级
    std::sort(commLinks[0].begin(), commLinks[0].end(), [&](const CommLinkContext& a, const CommLinkContext& b) {
        return a.commLink.dstEndpointDesc.loc.device.devPhyId < b.commLink.dstEndpointDesc.loc.device.devPhyId;
    });
    for (auto it = commLinks[0].begin(); it != commLinks[0].end(); ++it) {
        layer0CommLinks.push_back(std::make_pair(0, it)); // netLayer为0
    }

    // 从layer=1开始，将commLinks存入vector中，找到所有与当前devicePhyId相同的节点
    std::vector<std::pair<uint32_t, CommLinkContext>> highLayerCommLinks;
    std::vector<std::pair<uint32_t, CommLinkContext>> hostDpuCommLinks;
    for (uint32_t netLayer = 1; netLayer < netLayerNum; netLayer++) {
        for (auto it = commLinks[netLayer].begin(); it != commLinks[netLayer].end(); ++it) {
            if (it.dstEndpointDesc.loc.locType == ENDPOINT_LOC_TYPE_HOST) {
                // host dpu场景把所有的host dpu节点作为同一个平面成环，与device NPU分隔开
                hostDpuCommLinks.push_back(std::make_pair(netLayer, it));
            } else if (it.dstEndpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
                if (it.dstEndpointDesc.loc.device.devPhyId == this->devicePhyId_) {
                    // 在跨server、跨pod、跨超节点的场景，统一拿到devPhyId，打平处理为同一个平面，类似layer=0的情况
                    highLayerCommLinks.push_back(std::make_pair(netLayer, it));
                }
            } else {
                continue;
            }
        }
    }

    // 每个平面都分别成环
    CHK_RET(GetSamePlaneRank(collComm, layer0CommLinks, needConnectRank));
    CHK_RET(GetSamePlaneRank(collComm, highLayerCommLinks, needConnectRank));
    CHK_RET(GetSamePlaneRank(collComm, hostDpuCommLinks, needConnectRank));
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
        BeginMonitorThread();
    }

    lock.unlock();
    // 存放以myRank为起点，netLayer为key，可以连接到的所有对端的<rankid, CommLink>
    std::map<uint32_t, std::vector<CommLinkContext>> commLinks;
    uint32_t netLayerNum = 0;
    // 获取从myRank出发，所有的对端
    CHK_RET(GetRemEndpointDescs(collComm, commLinks, &netLayerNum));
    lock.unlock();

    // TODO:解析heartbeat环境变量

    // 获取需要连接的endPointDesc
    std::map<ClusterUIDType, ClusterMonitorContext> needConnectRank;
    CHK_RET(GetConnectRank(needConnectRank, commLinks, netLayerNum));

    std::unique_lock<std::mutex> linkInfolock(hbLinkConnInfoMtx_);
    for (auto &item : needConnectRank) {
        if (item.second.newConn == true) {
            hbLinkConnInfo_[group].push(std::move(item));
        }
    }
    linkInfolock.unlock();

    lock.lock();
    for (auto &item : needConnectRank) {
        if (item.second.newConn == true) {
            rankId2LinkStatusMap_[item.first] = HBLinkStatus::HEARTBEAT_LINK_BUILDING;
        } else if (groupMap_[group].find(item.first) == groupMap_[group].end() ||
            (groupMap_[group].count(item.first) && groupMap_[group][item.first] == NO_CONN)) {
            rankId2SocketMap_.ref(item.first);
            HCCL_RUN_INFO("group:[%s], establish rank[%s] to rank[%s] heartbeat connection success.", group.c_str(),
                FormatUId(uid_).c_str(), FormatUId(item.first).c_str());
            groupMap_[group][item.first] = HAS_CONN;
        }
    }
    lock.unlock();
}

HcclResult ClusterMonitor::UnRegisterToClusterMonitor(HcclComm comm)
{

}

}// namespace hccl