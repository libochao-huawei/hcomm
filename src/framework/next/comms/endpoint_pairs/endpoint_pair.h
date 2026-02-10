/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ENDPOINT_PAIR_H
#define ENDPOINT_PAIR_H

#include <memory>
#include <vector>
#include "channels/channel.h"
#include "../endpoints/endpoint.h"
#include "socket_mgr.h"
#include "socket.h"
#include "../../../../legacy/unified_platform/resource/socket/socket.h"
#include "topo_common_types.h"

using EndpointDescPair = std::pair<EndpointDesc, EndpointDesc>;

namespace std {
template <>
struct hash<EndpointDescPair> {
    size_t operator()(const EndpointDescPair& p) const noexcept {
        size_t h1 = std::hash<EndpointDesc>{}(p.first);
        size_t h2 = std::hash<EndpointDesc>{}(p.second);
        size_t h = h1;
        h ^= h2 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

} // namespace std

namespace hcomm {
/**
 * @note 职责：通信设备Endpoint对（EndpointPair，或连接）的C++类声明，
 * 管理该Endpoint对上的本地和远端注册内存、多个Channel、以及socket等。两个Endpoint的通信协议一致。
 */
class EndpointPair {
public:
    EndpointPair(EndpointDesc localEndpointDesc, EndpointDesc remoteEndpointDesc):
        localEndpointDesc_(localEndpointDesc), remoteEndpointDesc_(remoteEndpointDesc) {}
    ~EndpointPair();

    HcclResult Init();
    HcclResult GetSocket(const std::string &socketTag, Hccl::Socket*& socket);
    HcclResult CreateChannel(EndpointHandle endpointHandle, CommEngine engine, 
        HcommChannelDesc *channelDescs, ChannelHandle *channels);

private:
    EndpointDesc localEndpointDesc_{};
    EndpointDesc remoteEndpointDesc_{};
    std::unique_ptr<SocketMgr> socketMgr_;
    std::vector<ChannelHandle> channelHandles_{};
};

} // namespace hcomm

#endif // ENDPOINT_PAIR_H