/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "host_cpu_channel.h"
#include "../../../endpoints/endpoint.h"
#include "hcomm_res.h"
#include "orion_adpt_utils.h"

namespace hcomm {

HcclResult HostCpuChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    CHK_PTR_NULL(notifyNum);
    *notifyNum = channelDesc_.notifyNum;
    return HCCL_SUCCESS;
}

HcclResult HostCpuChannel::StartListen()
{
    uint16_t port = channelDesc_.port;
    HCCL_INFO("[HostCpuChannel::%s] Start. EndpointHandle[0x%llx], port[%u]", __func__, reinterpret_cast<uint64_t>(endpointHandle_), port);
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    CHK_RET(static_cast<HcclResult>(HcommEndpointStartListen(endpointHandle_, port, nullptr)));
    HCCL_INFO("[HostCpuChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult HostCpuChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[HostCpuChannel::%s] socket ptr is NULL, rebuild Socket", __func__);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[HostCpuChannel::%s] built linkData: %s", __func__, linkData.Describe().c_str());
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    Hccl::SocketConfig socketConfig = MakeSocketConfig(linkData, port);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));
    HCCL_INFO("[HostCpuChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

} // namespace hcomm
