/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef REG_MEM_MANAGER_H
#define REG_MEM_MANAGER_H

#include <array>
#include <utility>
#include <vector>
#include "hcomm_res_defs.h"
#include "hccl/hccl_res.h"

namespace hccl {

class MyRank;

struct RegMemInfo {
    CommMem mem{};
    std::array<char, HCCL_RES_TAG_MAX_LEN> memTag{};
};

class RegMemMgr {
public:
    explicit RegMemMgr(MyRank *myRank) : myRank_(myRank) {}
    ~RegMemMgr() = default;

    HcclResult RegisterMemory(CommMem mem, const char *memTag, HcommMemHandle *memHandle);

private:
    MyRank *myRank_{nullptr};
    std::vector<std::pair<HcommMemHandle, RegMemInfo>> memInfoVec_{};
};

}

#endif