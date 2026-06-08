/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RANK_GRAPH_V1_H
#define RANK_GRAPH_V1_H

#include <unordered_map>
#include "rank_graph.h"

namespace hccl {
/**
 * @note 职责：集合通信域内的version为1.0+（91092/91093...）的RankGraph的派生实现类
 */
class RankGraphV1 : public RankGraph {
public:
    explicit RankGraphV1(const std::string& rankTable);
    ~RankGraphV1() override = default;

    uint32_t GetRankId() const override;
    uint32_t GetRankSize() const override;

private:
    void ParseRankTable(const std::string& rankTable);
};
}

#endif // RANK_GRAPH_V1_H
