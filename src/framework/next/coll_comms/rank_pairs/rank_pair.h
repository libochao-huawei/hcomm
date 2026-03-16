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
#include "hcomm_res_defs.h"
#include "endpoint_pair_mgr.h"

using RankId = uint32_t;
using RankIdPair = std::pair<RankId, RankId>;

namespace std {

template <>
struct hash<RankIdPair> {
    size_t operator()(const RankIdPair& p) const noexcept {
        uint64_t key = (uint64_t(p.first) << 32) | uint64_t(p.second);
        return std::hash<uint64_t>{}(key);
    }
};

} // namespace std

namespace hccl {

/**
 * @note 职责：管理一个rank对（MyRank+DstRank）中的多个源EndPoint下的EndPointPair。
 * 当前先只考虑一个源EndPoint下只有一个EndPointPair的场景。
 */
class RankPair {
public:
    RankPair(RankIdPair rankIdPair):
        localRankId_(rankIdPair.first), remoteRankId_(rankIdPair.second) {}
    ~RankPair() = default;

    HcclResult Init();
    HcclResult GetEndpointPair(CommEngine engine, const EndpointDescPair &epDescPair, hcomm::EndpointPair*& out);

private:
    RankId localRankId_{};
    RankId remoteRankId_{};
    std::unique_ptr<hcomm::EndpointPairMgr> endpointPairMgr_{};
};
}

#endif // RANK_PAIR_H
