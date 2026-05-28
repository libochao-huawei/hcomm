/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AICPU_TS_UB_CHANNEL_BASE_H
#define AICPU_TS_UB_CHANNEL_BASE_H

#include "../channel.h"
#include "../../sockets/socket_mgr.h"
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/framework/resource_manager/socket/socket_manager.h"
#include "../../../../../../legacy/unified_platform/pub_inc/buffer_key.h"
#include "rma_connection.h"
#include "ub_mem_transport.h"
#include "dev_ub_connection.h"
#include "ub_local_notify.h"

namespace hcomm {

class AicpuTsUbChannelBase : public Channel {
public:
    AicpuTsUbChannelBase(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);
    ~AicpuTsUbChannelBase() override = default;

protected:
    HcclResult BuildNotify();
    HcclResult BuildBuffer(std::vector<std::shared_ptr<Hccl::Buffer>> &bufs);

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
    RdmaHandle                                                  rdmaHandle_{nullptr};
    std::unique_ptr<Hccl::UbMemTransport>                       memTransport_{nullptr};
    Hccl::BaseMemTransport::CommonLocRes                        commonRes_{};
    std::vector<Hccl::LocalRmaBuffer *>                         bufferVecTemp_;
    std::vector<std::unique_ptr<Hccl::DevUbConnection>>         connections_{};
    std::vector<std::unique_ptr<Hccl::LocalUbRmaBuffer>>        localRmaBuffers_{};
    std::vector<std::unique_ptr<Hccl::UbLocalNotify>>           localNotifies_{};
    std::unique_ptr<Hccl::Socket>                               serverSocket_;
    const Hccl::SocketConfig*                                   socketConfig_{nullptr};
    uint32_t                                                    devicePhyId_{};
};

} // namespace hcomm

#endif // AICPU_TS_UB_CHANNEL_BASE_H