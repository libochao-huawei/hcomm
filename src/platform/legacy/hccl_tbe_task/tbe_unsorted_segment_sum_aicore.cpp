/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "tbe_unsorted_segment_sum_aicore.h"

namespace TbeReduce {
TbeUnsortedSegmentSum::TbeUnsortedSegmentSum()
{
}

TbeUnsortedSegmentSum::~TbeUnsortedSegmentSum()
{
}

HcclResult TbeUnsortedSegmentSum::Init() // 初始化
{
    return HCCL_E_NOT_SUPPORT;
}

HcclResult TbeUnsortedSegmentSum::Run(const void *input, const void *segmentids,
    u64 inputCount, u64 outputCount, u32 valueDim, const HcclDataType dataType, void *stream, const void *dst)
{
    (void)input;
    (void)segmentids;
    (void)inputCount;
    (void)outputCount;
    (void)valueDim;
    (void)dataType;
    (void)stream;
    (void)dst;
    return HCCL_E_NOT_SUPPORT;
}
}