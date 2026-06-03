/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rank_graph.h"
#include "rank_graph_v1.h"
#include "rank_graph_v2.h"

namespace hccl {
/**
 * @note 职责：集合通信域内的RankGraph的创建工厂C++接口实现
 */
std::shared_ptr<RankGraph> RankGraph::CreateRankGraph(const std::string &rankTable, const std::string &topoFile)
{
    if (topoFile.empty()) {
        // 使用V1版本
        return std::make_shared<RankGraphV1>(rankTable);
    } else {
        // 使用V2版本
        return std::make_shared<RankGraphV2>(rankTable, topoFile);
    }
}
}
