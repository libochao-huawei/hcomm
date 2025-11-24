/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MY_RANK_H
#define MY_RANK_H

#include <memory>
#include <vector>
#include "comm_mems/comm_mems.h"
#include "../../comms/comm_engine_res/comm_engine_res_mgr.h"
#include "../../comms/endpoints/endpoint_mgr.h"
#include "rank_pair.h"

namespace hccl {
/**
 * @note 职责：管理当前通信域下本Rank的信息和通信资源
 */
class MyRank {
public:
    MyRank(uint32_t rankId, const RankInfo& rankInfo, const CommConfig& config);
    ~MyRank() = default;

    // 初始化MyRank资源
    HcclResult Init();

    // 获取通信内存管理器
    std::shared_ptr<CommMems> GetCommMems() const { return commMems_; }

    // 获取通信引擎资源管理器
    std::shared_ptr<CommEngineResMgr> GetCommEngineResMgr() const { return commEngineResMgr_; }

    // 获取端点管理器
    std::shared_ptr<EndPointMgr> GetEndPointMgr() const { return endPointMgr_; }

    // 获取和目标rank的连接对
    std::shared_ptr<RankPair> GetRankPair(uint32_t dstRankId) const { return rankPairMgr_.at(dstRankId); }

    // 创建通信Channel
    HcclResult CreateChannels(CommEngine engine, const std::vector<ChannelDesc>& channelDescs,
        std::vector<ChannelHandle>& channels);

    // 获取Rank ID
    uint32_t GetRankId() const { return rankId_; }

    // 获取配置
    const CommConfig& GetConfig() const { return config_; }

private:
    uint32_t rankId_{};
    RankInfo rankInfo_{};
    CommConfig config_{};
    std::shared_ptr<CommMems> commMems_{};
    std::shared_ptr<CommEngineResMgr> commEngineResMgr_{};
    std::shared_ptr<EndPointMgr> endPointMgr_{};
    std::unordered_map<uint32_t, std::shared_ptr<RankPair>> rankPairMgr_{};
};
}

#endif // MY_RANK_H
