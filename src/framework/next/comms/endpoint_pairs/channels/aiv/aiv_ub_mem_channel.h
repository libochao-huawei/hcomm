/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AIV_UB_MEM_CHANNEL_H
#define AIV_UB_MEM_CHANNEL_H

#include "../channel.h"

// Orion
#include "../../../../../../legacy/unified_platform/resource/socket/socket.h"
#include "../../../../../../legacy/unified_platform/pub_inc/buffer_key.h"
#include "aiv_ub_mem_transport.h"

namespace hcomm {

class AivUbMemChannel : public Channel {
public:
    AivUbMemChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    ChannelStatus GetStatus() override;
    HcclResult GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum) override;

private:
    HcclResult ParseInputParam();
    HcclResult BuildTransport();

    // --------------------- 入参 ---------------------
    EndpointHandle                                              endpointHandle_;
    HcommChannelDesc                                            channelDesc_;

    // --------------------- 具体成员 ---------------------
    Hccl::Socket*                                               socket_{nullptr};
    std::unique_ptr<AivUbMemTransport>                          transport_{nullptr};
};

} // namespace hcomm

#endif // AIV_UB_MEM_CHANNEL_H