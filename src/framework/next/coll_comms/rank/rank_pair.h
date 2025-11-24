/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RANK_PAIR_H
#define RANK_PAIR_H

#include <memory>
#include <vector>
#include "../comms/endpoint_pairs/endpoint_pair.h"

namespace hccl {
/**
 * @note 职责：管理一个rank对（MyRank+DstRank）中的多个源EndPoint下的EndPointPair。
 * 当前先只考虑一个源EndPoint下只有一个EndPointPair的场景。
 */
class RankPair {
public:
    RankPair(uint32_t srcRankId, uint32_t dstRankId);
    ~RankPair();

    HcclResult CreateChannels(CommEngine engine, const std::vector<ChannelDesc>& channelDescs,
        std::vector<ChannelHandle>& channels);

    // 获取端点对管理器
    std::shared_ptr<EndPointPairMgr> GetEndPointPair(EndPointInfo &srcEndPointInfo) const {
        return endPointPairs_->at(srcEndPointInfo);
    }

private:
    // key为源网卡EndPoint
    std::unordered_map<EndPointInfo, std::shared_ptr<EndPointPair>> endPointPairs_{};
};
}

#endif // RANK_PAIR_H
