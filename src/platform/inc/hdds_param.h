/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HDDS_PARAM_H
#define HDDS_PARAM_H

#include <vector>
#include <queue>
#include "driver/ascend_hal.h"
#include "hccl_types.h"
#include "hccl/base.h"

namespace hccl {

constexpr unsigned int EMBEBDING_GROUP_ID = 20;

enum class EmbeddingType {
    LOOK_UP,
    UPDATE
};

struct WorkSpace {
    void *sendWorkSpace = nullptr;
    void *recvWorkSpace = nullptr;
};

struct EmbeddingData {
    u64 keyMaxNum;
    s64 *keys;
    void *value;
    u64  valueItemSize;
    WorkSpace workSpace;
    s32 tableId;
    s32 insertFlag;
    std::map<u32, u32> psMap; // {psId, rankId}
    std::map<u32, u64> waitLookUpRanks;
    u32 devId;
};

struct PsEnvelopeInfo {
    u32 envelopeSize;
    std::vector<std::queue<void *>> envelopeList;  // 以rankId为下标的信封队列
};

struct EventInfo {
    u32 devId;
    u32 threadId;
    unsigned long long eventBitmap;
};

struct DeviceCommInitInfo {
    const char* rankTableM;
    uint32_t rank;
    const CommAttr* attr;
};

}
#endif
