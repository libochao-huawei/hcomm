/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcomm_nic_plugin.h"

#include "cpu_urma_endpoint.h"
#include "host_cpu_urma_channel.h"
#include "host_nic_plugin_ops.h"

namespace {

struct HostUbPluginTraits {
    using EndpointT = hcomm_host_nic::CpuUrmaEndpoint;
    using ChannelT = hcomm_host_nic::HostCpuUrmaChannel;

    static const char *LogTag()
    {
        return "HostUbPlugin";
    }

    static HcommResult GetUserRemoteMem(ChannelT *channel, CommMem **remoteMem, char ***memTags, uint32_t *memNum)
    {
        (void)channel;
        (void)remoteMem;
        (void)memTags;
        (void)memNum;
        return HCCL_E_NOT_SUPPORT;
    }
};

using PluginOps = hcomm_host_nic::HostNicPluginOps<HostUbPluginTraits>;

const HcommNicPluginInfo kPluginInfo = {
    HCOMM_NIC_PLUGIN_ABI_VERSION,
    "hcomm_host_ub",
    2U,
    {HCOMM_NIC_PROTO_UBC_TP, HCOMM_NIC_PROTO_UBC_CTP, HCOMM_NIC_PROTO_HCCS, HCOMM_NIC_PROTO_HCCS}
};

HcommNicEndpointOps kEndpointOps = PluginOps::EndpointOps();
HcommNicChannelOps kChannelOps = PluginOps::ChannelOps();
} // namespace

extern "C" const HcommNicPluginInfo *HcommNicPluginGetInfo(void)
{
    return &kPluginInfo;
}

extern "C" int32_t HcommNicPluginCreateEndpoint(const void *endpointDescRaw, uint32_t epDescLen, void **outCtx,
    HcommNicEndpointOps **outOps)
{
    return PluginOps::CreateEndpointExport(endpointDescRaw, epDescLen, outCtx, outOps, &kEndpointOps);
}

extern "C" int32_t HcommNicPluginCreateChannel(void *epCtx, const void *channelDescRaw, uint32_t chDescLen,
    void **outCtx, HcommNicChannelOps **outOps)
{
    return PluginOps::CreateChannelExport(epCtx, channelDescRaw, chDescLen, outCtx, outOps, &kChannelOps);
}
