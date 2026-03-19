/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CHANNEL_TRANSPORT_PROCESS_H
#define CHANNEL_PROCESS_H

#include "channel.h"
#include "mem_host_pub.h"
#include <mutex>
#include <string>
#include "channel_param.h"

namespace hcomm {

class ChannelTransportProcess {
public:
    ChannelTransportProcess() = default;
    ~ChannelTransportProcess() = default;

    static HcclResult DeepCopyH2DChannelP2p(const HcclChannelP2p &hostChannelP2p, HcclChannelP2p &deviceChannelP2p,
        std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector);
    static HcclResult DeepCopyH2DChannelHccsRes(const HcclChannelHccsRes &hostChannelHccsRes, 
        HcclChannelHccsRes &deviceChannelHccsRes, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector);
    static HcclResult DeepCopyH2DChannelParam(const HcclChannelTransportResSet &hostChannelParam, 
        HcclChannelTransportResSet &deviceChannelParam, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector);
    static HcclResult ChannelPackTransportHccsRes(ChannelHandle *hostChannelHandles, uint32_t listNum,
        HcclChannelTransportResSet &channelTransportResSetDevice, std::vector<std::shared_ptr<hccl::DeviceMem>> &channelParamMemVector);
    static HcclResult LaunchChannelKernelForTransportBase(ChannelHandle *channelHandles, ChannelHandle *hostChannelHandles,
        uint32_t listNum, aclrtBinHandle binHandle, const std::string &kernelName);
    static HcclResult ChannelKernelLaunchForTransport(ChannelHandle *channelHandles, 
        ChannelHandle *hostChannelHandles, uint32_t listNum, aclrtBinHandle binHandle);
};
}
#endif // CHANNEL_PROCESS_H