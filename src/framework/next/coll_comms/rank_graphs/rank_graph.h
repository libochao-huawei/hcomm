/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RANK_GRAPH_H
#define RANK_GRAPH_H

#include <memory>
#include <vector>
#include <string>

namespace hccl {
/**
 * @note 职责：集合通信域内的RankGraph的C++抽象接口类声明，以及RankGraph的创建工厂C++接口声明
 */
class RankGraph {
public:
    virtual ~RankGraph() = default;

    // 获取Rank ID
    virtual uint32_t GetRankId() const = 0;

    // 获取Rank数量
    virtual uint32_t GetRankSize() const = 0;

    static std::shared_ptr<RankGraph> CreateRankGraph(const std::string& rankTable,
        const std::string& topoFile = "");
private:
    uint32_t myRankId_{};
    uint32_t totalRanks_{};
};
}

#endif // RANK_GRAPH_H
