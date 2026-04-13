/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CLUSTER_MONITOR_H
#define CLUSTER_MONITOR_H

namespace hccl {

using ClusterUIDType = struct HcclClusterMonitorUid {
    char id[512] = {0}; // netLayerId + netInstanceId + deviceLogicId + eid 最大不超过512字节
    bool operator == (const HcclClusterMonitorUid &that) const
    {
        return std::string(this->id) == std::string(that.id);
    }
    bool operator != (const HcclClusterMonitorUid &that) const
    {
        return std::string(this->id) != std::string(that.id);
    }
    bool operator < (const HcclClusterMonitorUid &that) const
    {
        return std::string(this->id) < std::string(that.id);
    }
};

enum class ClusterMonitorStatus {
    CLUSTER_MONITOR_OK,
    CLUSTER_MONITOR_LOST,
    CLUSTER_MONITOR_NOTIFY,
    CLUSTER_MONITOR_CQE_ERR,
    CLUSTER_MONITOR_OPRETRY_NOT_SUPPORT,
    CLUSTER_MONITOR_STUCK,
    CLUSTER_MONITOR_INCONSISTENT
};

enum class MonitorLinkStatus {
    MONITOR_LINK_NOT_START;
    MONITOR_LINK_BUILDING;
    MONITOR_LINK_COMPLETED;
};

struct ClusterMonitorFrame {
    ClusterUIDType src;
    ClusterUIDType dst;
    ClusterUIDType crimer;
    ClusterUIDType informer;
    ClusterMonitorStatus status = ClusterMonitorStatus::CLUSTER_MONITOR_OK;
    HcclUs TOARelative; // time of arrival (Relative)
    HcclSystemTime TOASystem; // time of arrival (System)
    ClusterMonitorFrame() {}
    ClusterMonitorFrame(ClusterUIDType &crimer, ClusterUIDType &informer, ClusterMonitorStatus status, HcclUs TOARelativeIn,
        HcclSystemTime TOASystemIn)
        : crimer(crimer), informer(informer), status(status), TOARelative(TOARelativeIn),
        TOASystem(TOASystemIn)
    {}
    ClusterMonitorFrame(ClusterUIDType &src, ClusterUIDType &dst, ClusterUIDType &crimer, ClusterUIDType &informer, ClusterMonitorStatus status)
        : src(src), dst(dst), crimer(crimer), informer(informer), status(status)
    {}
};

struct ClusterMonitorContext { // 原ConnInfo
    HcclSocketChannelDesc socketDesc;             // 与对端连接的描述符
    socketHandle
    std::queue<ClusterMonitorFrame> sendBuffer;   // 用来发送的帧队列
    u32 restSize = 0;                             // 剩余待发送的帧长度
    RingBuffer recvBuffer;                        // 用来接收的环形帧队列
    u32 lostNum = 0;                              // 丢失的心跳个数
    bool newConn = false;                         // 是否是新增的连接
    ClusterMonitorContext() {}
    ClusterMonitorContext(bool newConn, HcclSocketChannelDesc &socketDesc)
        : socket(socketDesc), newConn(newConn)
    {}
};

struct ClusterUidCtxt {
    uint32_t        netLayer;
    std::string     netInstanceId;
    s32             deviceLogicId;
    ClusterUidCtxt() {}
    ClusterUidCtxt(uint32_t netLayer, std::string &netInstanceId, s32 deviceLogicId)
    : netLayer(netLayer), netInstanceId(netInstanceId), deviceLogicId(deviceLogicId)
    {}
};

class ClusterMonitor {
public:
    HcclResult RegisterToClusterMonitor(HcclComm comm);
    HcclResult UnRegisterToClusterMonitor(HcclComm comm);

private:
    ClusterMonitor() = default;
    ~ClusterMonitor();
    struct FrameStatus { // 专门用来给frame设置对应的状态
        ClusterMonitorStatus status = ClusterMonitorStatus::CLUSTER_MONITOR_OK;
        ClusterUIDType informer;
        bool needBroadcast = false;
        FrameStatus() {}
    };

    // 防止多线程同时初始化的线程锁
    std::mutext threadLock_;

    // 防止重复初始化
    bool initialized_ = false;
    
    // 通信域名称为key，ClusterUIDType表示1个节点, bool表示0或1，是否连接上，用来指示是否有过这个通信域以及对应通信域里待连接的节点是否以及连接上，原groupMap_
    std::map<std::string, std::map<ClusterUIDType, bool>> commIdMap_;
    
    // 通信域名称为key, 一个通信域有多个待连接心跳的connInfo，存储到该结构体，待monitor线程轮询拿到, 原hbLinkConnInfo_
    std::map<std::string, std::queue<std::pair<ClusterUIDType, ClusterMonitorContext>>> clusterLinkContext_{};
    
    // 存储UID与监控连接状态的map, NOT_START/BUILDING/COMPILETED，原rankId2LinkStatusMap_
    std::map<ClusterUIDType, MonitorLinkStatus> monitorLinkStatusMap_;
    
    // 存储UID与连接上下文的计数map，由于多个通信域都有可能使用同一个context去连接远端，需要计数处理，解注册时计数--，原rankId2SocketMap_
    ReferenceMap<ClusterUIDType, ClusterMonitorContext> uid2ContextRefMap_;
    
    // 用来做帧的统计计数，设置对应帧的状态，原rankId2StatusMap_
    ReferenceMap<ClusterUIDType, FrameStatus> uid2FrameStatusMap_;
    
    // uid与thread的维护关系，不同的remote起不同的异步建链线程，原linkThreadMap_
    std::map<ClusterUIDType, std::unique_ptr<std::thread>> linkThreadMap_{};
    
    // 保存错误的节点
    std::queue<ClusterUIDType> errRankQueue_;

};
}// namespace hccl
#endif // CLUSTER_MONITOR_H