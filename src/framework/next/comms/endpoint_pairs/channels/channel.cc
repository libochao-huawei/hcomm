/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <string>
#include <unordered_map>

#include "log.h"
#include "channel.h"
#include "./aicpu/aicpu_ts_urma_channel.h"
#include "./host/host_cpu_roce_channel.h"
#include "./ccu/ccu_urma_channel.h"
#include "./aiv/aiv_ub_mem_channel.h"

namespace hcomm {
std::unordered_map<ChannelHandle, ChannelHandle> channelD2HHandleMap_;
HcclResult Channel::CreateChannel(
    EndpointHandle endpointHandle, CommEngine engine, 
    HcommChannelDesc channelDesc, std::unique_ptr<Channel>& channelPtr)
{
    channelPtr.reset();
    // TODO: 通过引擎 + 协议
    // Endpoint 只区分协议
    switch (engine) {
        case COMM_ENGINE_CPU:
            // TODO: if 判断 EndpointDesc 里面的协议
            if (channelDesc.remoteEndpoint.protocol == COMM_PROTOCOL_ROCE) {
                EXECEPTION_CATCH(channelPtr = std::make_unique<HostCpuRoceChannel>(endpointHandle, channelDesc),
                    return HCCL_E_PARA);
                break;
            }
            HCCL_ERROR("[Channel][%s] CommEngine[COMM_ENGINE_CPU] not support", __func__);
            return HCCL_E_NOT_SUPPORT;
        case COMM_ENGINE_CPU_TS:
            HCCL_ERROR("[Channel][%s] CommEngine[COMM_ENGINE_CPU_TS] not support", __func__);
            return HCCL_E_NOT_SUPPORT;
        case COMM_ENGINE_AICPU:
        case COMM_ENGINE_AICPU_TS:
            channelPtr.reset(new (std::nothrow) AicpuTsUrmaChannel(
                endpointHandle, channelDesc
            ));
            break; 
        case COMM_ENGINE_AIV:
            channelPtr.reset(
                new (std::nothrow) AivUbMemChannel(endpointHandle, channelDesc));
            break; 
        case COMM_ENGINE_CCU:
            channelPtr.reset(
                new (std::nothrow) CcuUrmaChannel(endpointHandle, channelDesc));
            break;
        default:
            HCCL_ERROR("[Channel][%s] invalid type of CommEngine", __func__);
            return HCCL_E_NOT_FOUND;
    }
    CHK_PTR_NULL(channelPtr);
    CHK_RET(channelPtr->Init());
    return HCCL_SUCCESS;
}

HcclResult Channel::GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    return HCCL_SUCCESS;
}
} // namespace hcomm