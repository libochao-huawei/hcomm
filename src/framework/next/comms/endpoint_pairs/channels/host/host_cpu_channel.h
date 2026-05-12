/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HOST_CPU_CHANNEL_H
#define HOST_CPU_CHANNEL_H

#include "../channel.h"
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../sockets/socket_mgr.h"

namespace hcomm {

class HostCpuChannel : public Channel {
public:
    HostCpuChannel() = default;
    virtual ~HostCpuChannel() = default;

    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;

protected:
    HcclResult StartListen();
    HcclResult BuildSocket();

    virtual Hccl::SocketConfig MakeSocketConfig(Hccl::LinkData &linkData, uint16_t port) = 0;

    static constexpr uint16_t DEFAULT_LISTENING_PORT = 60001;
    static constexpr u32 FENCE_TIMEOUT_MS = 30 * 1000;

    EndpointHandle endpointHandle_;
    HcommChannelDesc channelDesc_;
    EndpointDesc localEp_{};
    EndpointDesc remoteEp_{};
    Hccl::Socket *socket_{nullptr};
    std::unique_ptr<SocketMgr> socketMgr_{nullptr};
    RdmaHandle rdmaHandle_{nullptr};
    uint32_t wqeNum_{0};
    bool fenceFlag_{false};
};

} // namespace hcomm

#endif // HOST_CPU_CHANNEL_H
