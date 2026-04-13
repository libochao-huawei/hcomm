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
    char id[512] = {0}; // eid[IP_ADDRESS_BUFFER_LEN] + ifname[MAX_INTERFACE_NAME_LEN] + devid 最大不超过512字节
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
    CLUSTER_OK,
    CLUSTER_LOST,
    CLUSTER_NOTIFY,
    CLUSTER_CQE_ERR,
    CLUSTER_OPRETRY_NOT_SUPPORT,
    CLUSTER_STUCK,
    CLUSTER_INCONSISTENT
};

struct ClusterMonitorFrame {
    ClusterUIDType src;
    ClusterUIDType dst;
    ClusterUIDType crimer;
    ClusterUIDType informer;
    ClusterMonitorStatus status = ClusterMonitorStatus::CLUSTER_OK;
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

struct ClusterMonitorContext {
    HcclSocketChannelDesc socketDesc;             // 与对端连接的描述符
    std::queue<ClusterMonitorFrame> sendBuffer;   // 用来发送的帧队列
    u32 restSize = 0;                             // 剩余待发送的帧长度
    RingBuffer recvBuffer;                        // 用来接收的环形帧队列
    u32 lostNum = 0;                              // 丢失的心跳个数
    bool newConn = false;                         // TODO:是否是新增的连接
    ClusterMonitorContext() {}
    ClusterMonitorContext(bool newConn, std::shared_ptr<HcclSocket> &socket)
        : socket(socket), newConn(newConn)
    {}
};

class ClusterMonitor {
public:
    HcclResult RegisterToClusterMonitor();

private:
    bool initialized_ = false;
     // 通信域名称为key，ClusterUIDType表示1个节点, bool表示0或1，是否连接上，用来指示是否有过这个通信域以及对应通信域里待连接的节点是否以及连接上
    std::map<std::string, std::map<ClusterUIDType, bool>> groupMap_;
    // 通信域名称为key, 一个通信域有多个待连接心跳的connInfo，存储到该结构体，待monitor线程轮询拿到
    std::map<std::string, std::queue<std::pair<ClusterUIDType, ClusterMonitorContext>>> clusterLinkConnInfo_{};

};
}// namespace hccl
#endif // CLUSTER_MONITOR_H