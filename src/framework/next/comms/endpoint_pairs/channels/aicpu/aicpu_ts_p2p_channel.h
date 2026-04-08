/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AICPU_TS_P2P_CHANNEL_H
#define AICPU_TS_P2P_CHANNEL_H

#include "../channel.h"
#include "../../sockets/socket_mgr.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/framework/resource_manager/socket/socket_manager.h"
#include "../../../../../../legacy/unified_platform/pub_inc/buffer_key.h"
#include "rma_connection.h"
#include "p2p_transport.h"
#include "p2p_connection.h"
#include "ipc_local_notify.h"
#include "remote_notify.h"
#include "local_ipc_rma_buffer.h"
#include "remote_ipc_rma_buffer.h"

namespace hcomm {

class AicpuTsP2pChannel : public Channel {
public:
    AicpuTsP2pChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    ChannelStatus GetStatus() override;
    HcclResult GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum) override;
    HcclResult UpdateMemInfo(void **memHandles, uint32_t memHandleNum) override;

    HcclResult H2DResPack(std::vector<char>& buffer);

    virtual HcclResult Clean() override;
    virtual HcclResult Resume() override;

private:
    HcclResult Makebufs(void **memHandles, uint32_t memHandleNum, std::vector<std::shared_ptr<Hccl::Buffer>> &bufs);
    HcclResult ParseInputParam();
    HcclResult BuildAttr();
    HcclResult BuildConnection();
    HcclResult BuildNotify();
    HcclResult BuildBuffer(std::vector<std::shared_ptr<Hccl::Buffer>> &bufs);
    HcclResult BuildP2PTransport();
    HcclResult BuildSocket();

    HcclResult PackOpData(std::vector<char> &data);

private:
    // --------------------- 入参 ---------------------
    EndpointHandle                                              endpointHandle_;
    HcommChannelDesc                                            channelDesc_;

    // --------------------- 转换参数 ---------------------
    EndpointDesc                                                localEp_{};
    EndpointDesc                                                remoteEp_{};
    uint32_t                                                    notifyNum_{0};
    std::vector<std::shared_ptr<Hccl::Buffer>>                  bufs_{};
    std::vector<std::shared_ptr<Hccl::Buffer>>                  bufsTemp{};

    // --------------------- 具体成员 ---------------------
    Hccl::Socket*                                               socket_{nullptr};
    std::unique_ptr<Hccl::P2PTransport>                         memTransport_{nullptr};
    Hccl::BaseMemTransport::Attribution                         attr_{};
    Hccl::BaseMemTransport::CommonLocRes                        commonRes_{};
    std::vector<Hccl::LocalRmaBuffer *>                         bufferVecTemp_;
    std::vector<std::unique_ptr<Hccl::P2PConnection>>           connections_{};
    std::vector<std::unique_ptr<Hccl::LocalIpcRmaBuffer>>       localRmaBuffers_{};
    std::vector<std::unique_ptr<Hccl::IpcLocalNotify>>          localNotifies_{};
    std::unique_ptr<Hccl::Socket>                               serverSocket_;
    std::unique_ptr<SocketMgr>                                  socketMgr_{nullptr};
};

} // namespace hcomm

#endif // AICPU_TS_P2P_CHANNEL_H
