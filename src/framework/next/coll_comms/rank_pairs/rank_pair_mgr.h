/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RANK_PAIR_MGR_H
#define RANK_PAIR_MGR_H

#include <unordered_map>
#include <mutex>
#include "rank_pair.h"

namespace hccl {

class RankPairMgr {
public:
    RankPairMgr(){};

    ~RankPairMgr() = default;

    HcclResult Get(RankIdPair rankIdPair, RankPair*& out);

private:
    std::unordered_map<RankIdPair, std::unique_ptr<RankPair>> rankPairMap_{};
};

} // namespace hccl

#endif // RANK_PAIR_MGR_H