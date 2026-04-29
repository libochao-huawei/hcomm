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
#include "channel_process.h"

#include "hcom_common.h"
#include "exception_handler.h"

namespace hcomm {

EndpointPair::~EndpointPair() 
{
    for (auto &channels : channelHandles_) {
        (void)ChannelProcess::ChannelDestroy(channels.second.data(), channels.second.size());
    }
}

HcclResult EndpointPair::Init()
{
    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);
    channelHandles_.clear();
    return HCCL_SUCCESS;
}

HcclResult EndpointPair::GetSocket(const std::string &socketTag, const uint32_t listenPort,
    u32 reuseIdx, Hccl::Socket*& socket)
{
    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEndpointDesc_, remoteEndpointDesc_, linkData, reuseIdx));
    std::string linkTag = socketTag;
    if (linkData.GetReuseIdx() != "0") {
        linkTag += ("_" + linkData.GetReuseIdx());
    }
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, listenPort, linkTag);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket));
    return HCCL_SUCCESS;
}

HcclResult EndpointPair::GetSocket(const uint32_t myRank, const uint32_t rmtRank,
    const std::string &socketTag, u32 reuseIdx, const uint32_t listenPort, Hccl::Socket*& socket)
{
    // 临时方案：支持混跑新增，非Roce场景走orion socketMgr实现server socket复用
    if (localEndpointDesc_.loc.locType == EndpointLocType::ENDPOINT_LOC_TYPE_HOST) {
        std::string socketTagPrefix = socketTag;
        if (myRank <= rmtRank) {
            socketTagPrefix += "_" + std::to_string(myRank) + "_" + std::to_string(rmtRank);
        } else {
            socketTagPrefix += "_" + std::to_string(rmtRank) + "_" + std::to_string(myRank);
        }
        CHK_RET(this->GetSocket(socketTagPrefix, listenPort, reuseIdx, socket));
        return HCCL_SUCCESS;
    }

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkDataWithRankIds(myRank, rmtRank,
        localEndpointDesc_, remoteEndpointDesc_, linkData, reuseIdx));

    // 复用orion流程可能抛异常
    EXCEPTION_HANDLE_BEGIN
    if (!socketMgrCompat_) {
        int32_t devLogicId = HcclGetThreadDeviceId();
        uint32_t devPhyId{0};
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(devLogicId), devPhyId));

        EXECEPTION_CATCH(socketMgrCompat_ =
            std::make_unique<Hccl::SocketManager>(myRank, devPhyId, devLogicId, socketTag),
            return HCCL_E_PTR);
    }

    socketMgrCompat_->BatchCreateSockets({linkData}); // 内部同时处理server端和connect端两类socket

    std::string linkTag = socketTag;
    if (linkData.GetReuseIdx() != "0") {
        linkTag += ("_" + linkData.GetReuseIdx());
    }
    Hccl::SocketConfig socketConfig(linkData.GetRemoteRankId(), linkData, linkTag);
    socket = socketMgrCompat_->GetConnectedSocket(socketConfig);
    CHK_PTR_NULL(socket);
    EXCEPTION_HANDLE_END

    return HCCL_SUCCESS;
}

HcclResult EndpointPair::CreateChannel(EndpointHandle endpointHandle, CommEngine engine, u32 reuseIdx,
        HcommChannelDesc *channelDescs, ChannelHandle *channels)
{
    if (channelHandles_.find(engine) == channelHandles_.end() || channelHandles_[engine].size() <= reuseIdx) {
        CHK_RET(static_cast<HcclResult>(
            HcommCollectiveChannelCreate(endpointHandle, engine, channelDescs, 1, channels)));
        channelHandles_[engine].push_back(channels[0]);
        return HCCL_SUCCESS;
    }

    channels[0] = channelHandles_[engine][reuseIdx];
    if (channelDescs->memHandleNum > 1) {
        CHK_RET(static_cast<HcclResult>(HcommChannelUpdateMemInfo(channelDescs->memHandles + 1, channelDescs->memHandleNum - 1, channels[0])));
    }
    return HCCL_SUCCESS;
}

const std::unordered_map<CommEngine, std::vector<ChannelHandle>>& EndpointPair::GetChannelHandles()
{
    return channelHandles_;
}

} // namespace hcomm
