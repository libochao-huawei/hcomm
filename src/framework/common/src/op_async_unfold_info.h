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

constexpr uint32_t ASYNC_UNFOLD_MAX_BSR_ITEM_NUM = 4096;
constexpr uint32_t ASYNC_UNFOLD_MAX_RANK_SIZE = 128;
constexpr uint32_t ASYNC_UNFOLD_MAX_DYNAMIC_SIZE = 129 * 1024;

// 定义结构体封装算子信息, 用于aicpu异步展开单算子 (< 1 + 129 = 130 KiB)
// Part 1: static part of tiling data (~576 B < 1 KiB)
// Part 2: dynamic part of tiling data (<= 132 + 128 KiB < 129 KiB)
//     NOTE: refer to CalcOpTilingDynamicDataSize/SetDynamicTilingData*
//     (1) batchsendrcv: sizeof(OpTilingBatchSendRecvDataDes) + itemNum * sizeof(HcclSendRecvItem) + rankSize * sizeof(u8)
//     <= 4 + 4096 * 32 + 128 * 1 = 132 + 128 KiB < 129 KiB
//     (2) alltoall: sizeof(struct OpTilingAllToAllDataDes) = 16 B
//     (3) alltoallv: sizeof(struct OpTilingAlltoallvDataDes) + rankSize * ALLTOALL_INFO_MATRIX_SIZE * sizeof(u64) + hostCollectBuffer_.size
//     <= 2 + 128 * 4 * 8 + userOutput.size = 4098 + 0 (now alltoall does not support pipeline alg) < 5 KiB
//     (4) alltoallvc: sizeof(struct OpTilingAlltoallvcDataDes) + rankSize * rankSize * sizeof(u64)
//     <= 2 + 128 * 128 * 8 = 2 + 128 KiB < 129 KiB
//     (5) allgatherv/reducescatterv: sizeof(struct OpTilingVDataDes) + rankSize * 2 * sizeof(u64)
//     <= 9 + 128 * 2 * 8 = 9 + 2 KiB < 3 KiB
//     (6) other operators: sizeof(struct OpTilingDataDes) = 9 B
typedef uint8_t OpAsyncUnfoldInfo[sizeof(struct OpTilingData) + ASYNC_UNFOLD_MAX_DYNAMIC_SIZE];

} // namespace hccl

#endif // __OP_ASYNC_UNFOLD_INFO__