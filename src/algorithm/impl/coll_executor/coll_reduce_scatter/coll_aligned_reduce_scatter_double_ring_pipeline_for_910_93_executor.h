/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_ALIGNED_REDUCESCATTER_DOUBLE_RING_PIPELINE_FOR_910_93_EXECUTOR_H
#define COLL_ALIGNED_REDUCESCATTER_DOUBLE_RING_PIPELINE_FOR_910_93_EXECUTOR_H
#include "coll_aligned_reduce_scatter_double_ring_for_910_93_executor.h"

namespace hccl {
class CollAlignedReduceScatterDoubleRingPipelineFor91093Executor
    : public CollAlignedReduceScatterDoubleRingFor91093Executor {
public:
    explicit CollAlignedReduceScatterDoubleRingPipelineFor91093Executor(
        const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollAlignedReduceScatterDoubleRingPipelineFor91093Executor() override = default;

private:
    HcclResult CalcStreamNum(u32 &streamNum) override;
    u64 CalcLoopMaxCount(const u32 unitSize) override;
    HcclResult RunLoop(OpParam &param, AlgResourceResponse &algRes) override;

    HcclResult RunLevel0To1(OpParam &param, const ReduceType &reduceType, ExecMem &execMem, Stream &streamL0L1);
    HcclResult RunLevel2(OpParam &param, const ReduceType &reduceType, ExecMem &execMem, Stream &streamL2);
    HcclResult KernelRunLevel0To1(const OpParam &param, ExecMem &execMem, Stream &streamL0L1);
    HcclResult KernelRunLevel2(const OpParam &param, ExecMem &execMem, Stream &streamL2);
};

}  // namespace hccl

#endif
