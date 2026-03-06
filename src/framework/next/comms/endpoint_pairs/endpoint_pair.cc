/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "endpoint_pair.h"
#include "socket_config.h"
#include "hcomm_c_adpt.h"
#include "orion_adpt_utils.h"

namespace hcomm {

EndpointPair::~EndpointPair() 
{
    (void)HcommChannelDestroy(channelHandles_.data(), channelHandles_.size());
}

HcclResult EndpointPair::Init()
{
    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);
    channelHandles_.clear();
    return HCCL_SUCCESS;
}

HcclResult EndpointPair::GetSocket(const std::string &socketTag, Hccl::Socket*& socket)
{
    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEndpointDesc_, remoteEndpointDesc_, linkData));
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, socketTag);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket));
    return HCCL_SUCCESS;
}

HcclResult EndpointPair::CreateChannel(EndpointHandle endpointHandle, CommEngine engine, u32 reuseIdx,
        HcommChannelDesc *channelDescs, ChannelHandle *channels)
{
    if (channelHandles_.size() <= reuseIdx) {
        CHK_RET(HcommChannelCreate(endpointHandle, engine, channelDescs, 1, channels));
        channelHandles_.push_back(channels[0]);
        return HCCL_SUCCESS;
    }
    channels[0] = channelHandles_[reuseIdx];
    return HCCL_SUCCESS;
}

} // namespace hcomm