/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "coll_comm.h"
#include "rank_graphs/rank_graph.h"

namespace hccl {
CollComm::CollComm(uint32_t rankId, const std::string& rankTable, const CommConfig& config)
    : rankId_(rankId), rankTable_(rankTable), config_(config) {
}

CollComm::~CollComm() {
}

HcclResult CollComm::Init()
{
    EXCEPTION_HANDLE_BEGIN
    // 创建RankGraph
    rankGraph_ = RankGraph::CreateRankGraph(rankTable_, "");
    
    // 获取Rank信息
    RankInfo rankInfo;
    rankInfo.rankId = rankId_;
    rankInfo.totalRanks = rankGraph_->GetRankSize();
    
    // 创建MyRank
    myRank_ = std::make_shared<MyRank>(rankId_, rankInfo, config_);
    
    // 初始化MyRank
    CHK_RET(myRank_->Init());
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

uint32_t CollComm::GetMyRankId() const {
    return rankId_;
}

uint32_t CollComm::GetRankSize() const {
    if (rankGraph_) {
        return rankGraph_->GetRankSize();
    }
    return 0;
}
}