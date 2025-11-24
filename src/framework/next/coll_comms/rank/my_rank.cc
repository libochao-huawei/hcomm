/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "my_rank.h"
#include "exception_handler.h"

namespace hccl {
MyRank::MyRank(uint32_t rankId, const RankInfo& rankInfo, const CommConfig& config)
    : rankId_(rankId), rankInfo_(rankInfo), config_(config)
{
}

HcclResult MyRank::Init()
{
    EXCEPTION_HANDLE_BEGIN
    // 创建通信内存管理器
    commMems_ = std::make_shared<CommMems>(config_.bufferSize);

    // 初始化通信内存
    CHK_RET(commMems_->Init());

    // 创建通信引擎资源管理器
    commEngineResMgr_ = std::make_shared<CommEngineResMgr>();

    // 创建端点管理器
    endPointMgr_ = std::make_shared<EndPointMgr>();

    // 创建Rank对管理器
    return HCCL_SUCCESS;
    EXCEPTION_HANDLE_END
}

HcclResult MyRank::CreateChannels(CommEngine engine,
    const std::vector<ChannelDesc>& channelDescs, std::vector<ChannelHandle>& channels) {
    EXCEPTION_HANDLE_BEGIN

    // 获取MyRank上的内存
    std::vector<MemHandle> memHandles;
    CHK_RET(commMems_->GetMemoryHandles(memHandles));

    // 注册内存到端点
    for (const auto& channelDesc : channelDescs) {
        CHK_RET(endPointMgr_->RegisterMemory(channelDesc.srcEndPoint, memHandles));
    }

    // 获取当前端点注册内存信息
    std::vector<MemRegion> memRegions;
    CHK_RET(endPointMgr_->GetAllRegisteredMemory(channelDesc.srcEndPoint, memRegions));

    // 创建Channel
    rankPairMgr_.at(channelDesc.remoteRankId)->CreateChannels(channelDescs, memRegions, channels);

    return HCCL_SUCCESS;
    EXCEPTION_HANDLE_END
}
}
