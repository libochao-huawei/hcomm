/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __OP_ASYNC_UNFOLD_INFO__
#define __OP_ASYNC_UNFOLD_INFO__

#include "aicpu_operator_pub.h"

namespace hccl {

constexpr uint32_t ASYNC_UNFOLD_MAX_BSR_ITEM_NUM = 512;
constexpr uint32_t ASYNC_UNFOLD_MAX_RANK_SIZE = 128;

// 定义结构体封装算子信息, 用于aicpu异步展开单算子
struct OpAsyncUnfoldInfo {
    // static part of tiling data
    OpTilingData staticTilingData;

    // dynamic part of tiling data (refer to CalcOpTilingDynamicDataSize/SetDynamicTilingData*)
    // (1) batchsendrcv: sizeof(OpTilingBatchSendRecvDataDes) + itemNum * sizeof(HcclSendRecvItem) + rankSize * sizeof(u8)
    // <= 4 + 512 * 32 + 128 * 1 = 16516 < 16.2 KiB
    // (2) alltoallv: sizeof(struct OpTilingAllToAllDataDes) = 16 B
    // (2) alltoallvc: TODO
    // TODO: 按照最大值分配
    // TODO: 检查约束时, 校验itemNum和rankSize
    // TODO: 异步展开下发前, 校验dynamicSize与分配空间的大小, 如果大于分配空间, 则报错
}; // struct OpAsyncUnfoldInfo

} // namespace hccl

#endif // __OP_ASYNC_UNFOLD_INFO__