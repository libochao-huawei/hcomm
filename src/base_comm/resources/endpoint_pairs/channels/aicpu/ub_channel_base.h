/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UB_CHANNEL_BASE_H
#define UB_CHANNEL_BASE_H

#include "channel.h"
#include "../../../../../legacy/ascend950/unified_platform/resource/notify/ub_local_notify.h"
#include "../../../../../legacy/ascend950/unified_platform/resource/transport/ub_mem_transport.h"
#include "../../../../../legacy/ascend950/unified_platform/resource/connection/dev_ub_connection.h"

namespace hcomm {

class UbChannelBase : public Channel {
public:
    UbChannelBase() = default;
    ~UbChannelBase() override = default;

    UbChannelBase(const UbChannelBase&) = delete;
    UbChannelBase& operator=(const UbChannelBase&) = delete;
    UbChannelBase(UbChannelBase&&) = default;
    UbChannelBase& operator=(UbChannelBase&&) = default;

protected:
    HcclResult BuildNotify();
    virtual HcclResult BuildBuffer(std::vector<std::shared_ptr<Hccl::Buffer>> &bufs);

    uint32_t notifyNum_{0};
    RdmaHandle rdmaHandle_{nullptr};
    Hccl::BaseMemTransport::CommonLocRes commonRes_{};
    std::vector<Hccl::LocalRmaBuffer *> bufferVecTemp_;
    std::vector<std::unique_ptr<Hccl::DevUbConnection>> connections_{};
    std::vector<std::unique_ptr<Hccl::LocalUbRmaBuffer>> localRmaBuffers_{};
    std::vector<std::unique_ptr<Hccl::UbLocalNotify>> localNotifies_{};
};

} // namespace hcomm

#endif // UB_CHANNEL_BASE_H
