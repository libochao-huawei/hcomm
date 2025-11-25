/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COLL_COMM_H
#define COLL_COMM_H

#include <memory>
#include <string>
#include "rank/my_rank.h"
#include "rank_graphs/rank_graph.h"

namespace hccl {
/**
 * @note 职责：集合通信通信域上下文管理，包括RankGraph和本rank信息资源等内容。
 * 当前需包含原有的91092/91093的通信域、原有的91095的通信域void *指针、及新独立算子架构的通信域（支持91092/91093/91095...）。
 */
class CollComm {
public:
    CollComm(uint32_t rankId, const std::string& rankTable, const CommConfig& config);
    ~CollComm();
    
    // 初始化通信域
    HcclResult Init();
    
    // 获取MyRank
    std::shared_ptr<MyRank> GetMyRank() const { return myRank_; }
    
    // 获取RankGraph
    std::shared_ptr<RankGraph> GetRankGraph() const { return rankGraph_; }
    
    // 获取Rank ID
    uint32_t GetMyRankId() const;
    
    // 获取Rank数量
    uint32_t GetRankSize() const;

private:
    uint32_t rankId_{};
    std::string rankTable_{};
    CommConfig config_{};
    std::shared_ptr<MyRank> myRank_{};
    std::shared_ptr<RankGraph> rankGraph_{};
};
}

#endif // COLL_COMM_H
