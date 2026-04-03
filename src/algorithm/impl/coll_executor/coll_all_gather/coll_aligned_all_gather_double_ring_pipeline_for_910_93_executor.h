/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_ALIGNED_ALLGATHER_DOUBLE_RING_PIPELINE_FOR_910_93_EXECUTOR_H
#define COLL_ALIGNED_ALLGATHER_DOUBLE_RING_PIPELINE_FOR_910_93_EXECUTOR_H

#include "coll_all_gather_ring_for_910_93_executor.h"

namespace hccl {
class CollAlignedAllGatherDoubleRingPipelineFor91093Executor
    : public CollAllGatherRingFor91093Executor {
public:
    explicit CollAlignedAllGatherDoubleRingPipelineFor91093Executor(
        const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollAlignedAllGatherDoubleRingPipelineFor91093Executor() override = default;

    // 重写基类方法
    HcclResult CalcStreamNum(u32& streamNum) override;
    HcclResult KernelRun(const OpParam &param, ExecMem &execMem) override;

private:
    HcclResult PreparePipelineParameters(const OpParam &param, ExecMem &execMem);
    HcclResult RunPipelineAsync(const OpParam &param, ExecMem &execMem);

    // 三级流水线核心方法
    HcclResult RunThreeLevelPipeline(const OpParam &param, ExecMem &execMem);

    // 成员变量
    SubCommInfo level2CommInfo_;           // L2层通信域信息
    DeviceMem dmaBuffers_[2];              // 双缓冲DMA内存
};

} // namespace hccl

#endif