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
#include <thread>
#include <map>
#include <deque>
#include <mutex>
#include "hcclCommDfx.h"
#include "ring_buffer.h"
#include "coll_comm.h"
#include "reference_map.h"
#include "log.h"
#include "hccl/hccl_types.h"
#include "hccl_common.h"
#include "hccl_comm_socket_c_adpt.h"

namespace hcomm {

using ClusterUIDType = struct HcclClusterMonitorUid {
    char id[2048] = {0}; // netInstanceId + localId 最大不超过2048字节
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
}

namespace std {
template <> class hash<hcomm::HcclClusterMonitorUid> {
public:
    size_t operator () (const hcomm::HcclClusterMonitorUid &uid) const
    {
        return hash<string>()(string(uid.id));
    }
};
}

namespace hcomm {
enum class ClusterMonitorStatus {
    CLUSTER_MONITOR_OK,
    CLUSTER_MONITOR_LOST,
    CLUSTER_MONITOR_NOTIFY,
    CLUSTER_MONITOR_CQE_ERR,
    CLUSTER_MONITOR_OPRETRY_NOT_SUPPORT,
    CLUSTER_MONITOR_STUCK,
    CLUSTER_MONITOR_INCONSISTENT
};

struct ErrorCqeInfo {
    // TODO:待填写
};


/**
 * @brief Socket描述参数
 * @note 结构体末尾扩展需要自增版本号，并补充兼容处理逻辑。
 */
struct HcommSocketDesc {
    CommAbiHeader header;            ///< ABI头部，包含版本等信息
    EndpointDesc localEndpoint;      ///< 本端网络设备端侧描述
    EndpointDesc remoteEndpoint;     ///< 远端网络设备端侧描述
    std::string  tag;                /**< tag used for whitelist must ended by '\0' */
    HcommSocketRole role;            ///< 本端角色(SERVER或CLIENT)
    uint16_t port;                   ///< 监听端口或目标端口
};

struct ClusterMonitorFrame {
    ClusterUIDType src; // 心跳建链的本端
    ClusterUIDType dst; // 心跳建链的远端
    ClusterUIDType crimer; // 异常的节点
    ClusterUIDType informer; // 把异常传输给自己的节点
    ErrorCqeInfo   cqeInfo; // error cqe节点出现错误时，信息广播到其他节点
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

struct ClusterMonitorSocketCtx { // 原ConnInfo
    HcclCommSocketDesc socketDesc;                // 与对端连接的描述符
    HcclCommSocketHandle  socketHandler;          // 引用头文件定义
    std::queue<ClusterMonitorFrame> sendBuffer;   // 用来发送的帧队列
    u32 restSize = 0;                             // 剩余待发送的帧长度
    hccl::RingBuffer recvBuffer;                  // 用来接收的环形帧队列
    u32 lostNum = 0;                              // 丢失的心跳个数
    bool newConn = false;                         // 是否是新增的连接
    ClusterMonitorSocketCtx() {}
    ClusterMonitorSocketCtx(HcclCommSocketDesc &socketDesc, bool newConn)
        : socketDesc(socketDesc), newConn(newConn)
    {}
};

struct UidContext {
    ClusterUIDType uid;
    uint32_t    netLayer{0};
    uint32_t    rankId{0};
    uint32_t    localId{0}; // 用来netLayer=0的时候排序使用
    UidContext() {}
    UidContext(ClusterUIDType &uid, uint32_t netLayer, uint32_t rankId, uint32_t localId)
        : uid(uid), netLayer(netLayer), rankId(rankId), localId(localId)
    {}
};

struct ClusterUidCtxt {
    std::string     netInstId;
    uint32_t        localId;
    ClusterUidCtxt() {}
    ClusterUidCtxt(std::string &netInstId, uint32_t localId)
        : netInstId(netInstId), localId(localId)
    {}
};

class ClusterMonitor {
public:
    HcclResult RegisterToClusterMonitor(HcclComm comm);
    HcclResult UnRegisterToClusterMonitor(HcclComm comm);
    HcclResult FormatUID(ClusterUidCtxt ctxt, ClusterUIDType &uid);
    std::string GetUID(const ClusterUIDType &uid) const;
    std::string FormatConnTag(HcommSocketRole role, std::pair<ClusterUIDType, ClusterUIDType> uidPair);
    HcclResult InsertClusterMonitorCxt(HcclComm comm, UidContext remoteCtx, std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank);
    HcclResult GetSamePlaneRank(HcclComm comm, std::vector<UidContext> singlePlaneCtx, std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank);
    HcclResult GetConnectRank(HcclComm comm, std::map<ClusterUIDType, ClusterMonitorSocketCtx> &needConnectRank, std::map<uint32_t, std::vector<UidContext>> uidctxs, uint32_t netLayerNum);
    void CreateHBLinksAsync();
    void SetStatus(ClusterUIDType &crimer, ClusterUIDType &informer, ClusterMonitorStatus status, bool needBroadcast);
    void MonitorThread();
    HcclResult RunMonitorThread();
    void SendFrame(ClusterUIDType &dst, ClusterUIDType &crimer, ClusterUIDType &informer, ClusterMonitorStatus status);
    void DelErrorSocket();
    void ProcessExceptionEvent();
    HcclResult RecvFrame(ClusterUIDType rem);
    HcclResult ParseFrame(ClusterMonitorFrame &cmFrame, ClusterUIDType &src);

private:
    ClusterMonitor() = default;
    ~ClusterMonitor()= default;
    HcclResult GetRemEndpointDescs(HcclComm comm,
        std::map<uint32_t, std::vector<UidContext>> &uidctxs, uint32_t *netLayerNum);
    
    HcclResult CreateTransportHandle(ClusterMonitorSocketCtx &info);

    void CreateLinkWithRemotePonit(std::string group, ClusterUIDType rem, ClusterMonitorSocketCtx needConnectRank);

    struct FrameStatus { // 专门用来给frame设置对应的状态
        ClusterMonitorStatus status = ClusterMonitorStatus::CLUSTER_MONITOR_OK;
        ClusterUIDType informer;
        bool needBroadcast = false;
        FrameStatus() {}
    };

    enum class MonitorLinkStatus {
        MONITOR_LINK_NOT_START,
        MONITOR_LINK_BUILDING,
        MONITOR_LINK_COMPLETED,
    };

    uint32_t        devicePhyId_;
    uint32_t        localId_;
    s32             deviceLogicId_{0};
    ClusterUIDType  myRankUid_;

    bool clusterMonitorThreadFlag_ = false;
    std::unique_ptr<std::thread> clusterMonitorThread_;
    // 防止重复初始化
    bool initialized_ = false;
    uint32_t lostThreshold_ = 0;
    bool isDeInit_ = false;
    std::atomic<bool> linkThreadRunning_{false};

    // 防止多线程同时初始化的线程锁
    std::mutex threadLock_;
    std::vector<ClusterUIDType> errorSocket_;
    std::queue<ClusterMonitorFrame> errStatusQueue_;
    
    // 通信域名称为key，ClusterUIDType表示1个节点, bool表示0或1，是否连接上，用来指示是否有过这个通信域以及对应通信域里待连接的节点是否以及连接上，原groupMap_
    std::map<std::string, std::map<ClusterUIDType, bool>> commIdMap_;
    
    // 通信域名称为key, 一个通信域有多个待连接心跳的connInfo，存储到该结构体，待monitor线程轮询拿到, 原hbLinkConnInfo_
    std::map<std::string, std::queue<std::pair<ClusterUIDType, ClusterMonitorSocketCtx>>> clusterLinkContext_{};
    std::mutex clusertMonitorLinkMtx_;
    
    // 存储UID与监控连接状态的map, NOT_START/BUILDING/COMPILETED，原rankId2LinkStatusMap_
    std::map<ClusterUIDType, MonitorLinkStatus> monitorLinkStatusMap_;
    
    // 存储UID与连接上下文的计数map，由于多个通信域都有可能使用同一个context去连接远端，需要计数处理，解注册时计数--，原rankId2SocketMap_
    hccl::ReferenceMap<ClusterUIDType, ClusterMonitorSocketCtx> uid2SocketRefMap_;
    
    // 用来做帧的统计计数，设置对应帧的状态，原rankId2StatusMap_
    hccl::ReferenceMap<ClusterUIDType, FrameStatus> uid2FrameStatusMap_;
    
    // uid与thread的维护关系，不同的remote起不同的异步建链线程，原linkThreadMap_
    std::map<ClusterUIDType, std::unique_ptr<std::thread>> linkThreadMap_{};
    
    // 保存错误的节点
    std::queue<ClusterUIDType> errRankQueue_;

    std::atomic<bool> linkRunningStatus_{false};

};
} // namespace hcomm
#endif // CLUSTER_MONITOR_H