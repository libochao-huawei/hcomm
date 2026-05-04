/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AIV_URMA_CHANNEL_H
#define AIV_URMA_CHANNEL_H

#include "../channel.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/pub_inc/buffer_key.h"
#include "aiv_urma_transport.h"
#include "../../sockets/socket_mgr.h"
#include "../../../../../../legacy/unified_platform/resource/notify/ub_local_notify.h"

namespace hcomm {

class AivUrmaChannel : public Channel {
public:
    AivUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);

    HcclResult Init() override;
    ChannelStatus GetStatus() override;

    virtual HcclResult Clean() override;
    virtual HcclResult Resume() override;
    virtual HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    virtual HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    virtual HcclResult GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum) override;
    HcclResult BuildChannelEntityToDevice(void **devChannelPtr);

private:
    HcclResult Makebufs(
        HcommMemHandle *memHandles, uint32_t memHandleNum, std::vector<std::shared_ptr<Hccl::Buffer>> &bufs);
    HcclResult ParseInputParam();
    HcclResult BuildAttr();
    HcclResult BuildConnection();
    HcclResult BuildBuffer(std::vector<std::shared_ptr<Hccl::Buffer>> &bufs);
    HcclResult BuildAivUrmaTransport();
    HcclResult BuildSocket();

    // --------------------- 转换参数 ---------------------
    EndpointDesc localEp_{};
    EndpointDesc remoteEp_{};
    uint32_t notifyNum_{0};

    // --------------------- 入参 ---------------------
    EndpointHandle endpointHandle_;
    CommEngine engine_{COMM_ENGINE_RESERVED};
    HcommChannelDesc channelDesc_;
    std::vector<std::shared_ptr<Hccl::Buffer>> bufs_{};

    // --------------------- 具体成员 ---------------------
    Hccl::Socket *socket_{nullptr};
    std::unique_ptr<Hccl::AivUrmaTransport> transport_{nullptr};
    Hccl::BaseMemTransport::Attribution attr_{};
    Hccl::BaseMemTransport::CommonLocRes commonRes_{};
    std::unique_ptr<SocketMgr> socketMgr_{nullptr};
    std::vector<std::unique_ptr<Hccl::DevUbConnection>> connections_{};
    std::unique_ptr<Hccl::Socket> serverSocket_;
    RdmaHandle rdmaHandle_{nullptr};
    std::vector<std::unique_ptr<Hccl::LocalUbRmaBuffer>> localRmaBuffers_{};
};

} // namespace hcomm

#endif // AIV_UB_MEM_CHANNEL_H