/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_ub_mem_channel.h"
#include "../../../endpoints/endpoint.h"
#include "orion_adpt_utils.h"

// #include "exception_handler.h"
// #include "aiv_ub_transport.h"
// #include "dev_buffer.h"

namespace hcomm {

AivUbMemChannel::AivUbMemChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc):
    endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

HcclResult AivUbMemChannel::ParseInputParam() 
{
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    return HCCL_SUCCESS;
}

HcclResult AivUbMemChannel::BuildTransport()
{
    EXECEPTION_CATCH(transport_ = std::make_unique<AivUbMemTransport>(socket_, channelDesc_), return HCCL_E_PTR);
    CHK_PTR_NULL(transport_);
    CHK_RET(transport_->Init());
    return HCCL_SUCCESS;
}

HcclResult AivUbMemChannel::Init()
{
    // TODO: 处理抛异常
    CHK_RET(ParseInputParam());
    CHK_RET(BuildTransport());
    return HCCL_SUCCESS;
}

ChannelStatus AivUbMemChannel::GetStatus()
{
    Hccl::TransportStatus transportStatus = transport_->GetStatus();
    ChannelStatus out = ChannelStatus::INIT;
    switch (transportStatus) {
        case Hccl::TransportStatus::INIT:
            out = ChannelStatus::INIT;
            break;
        case Hccl::TransportStatus::SOCKET_OK:
            out = ChannelStatus::SOCKET_OK;
            break;
        case Hccl::TransportStatus::SOCKET_TIMEOUT:
            out = ChannelStatus::SOCKET_TIMEOUT;
            break;
        case Hccl::TransportStatus::READY:
            out = ChannelStatus::READY;
            break;
        default:
            HCCL_ERROR("[AivUbMemChannel][%s] Invalid TransportStatus[%d]", __func__, transportStatus);
            out = ChannelStatus::INVALID;
            break;
    }
    return out;
}

HcclResult AivUbMemChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    HCCL_INFO("AivUbMemChannel GetNotifyNum is not supported.");
    return HCCL_SUCCESS;
}

HcclResult AivUbMemChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    return transport_->GetRemoteMem(remoteMem, memNum, memTags);
}

HcclResult AivUbMemChannel::GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    return transport_->GetUserRemoteMem(remoteMem, memTag, memNum);
}
}
