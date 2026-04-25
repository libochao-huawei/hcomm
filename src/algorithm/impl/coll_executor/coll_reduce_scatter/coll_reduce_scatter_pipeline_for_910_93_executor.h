/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_REDUCESCATTER_PIPELINE_FOR_910_93_EXECUTOR_H
#define COLL_REDUCESCATTER_PIPELINE_FOR_910_93_EXECUTOR_H
#include "coll_reduce_scatter_ring_for_910_93_executor.h"

namespace hccl {
class CollReduceScatterPipelineFor91093Executor
    : public CollReduceScatterRingFor91093Executor {
public:
    explicit CollReduceScatterPipelineFor91093Executor(
        const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollReduceScatterPipelineFor91093Executor() override = default;

private:
    HcclResult CalcStreamNum(u32 &streamNum) override;
    u64 CalcLoopMaxCount(const u32 unitSize) override;
    HcclResult RunLoop(OpParam &param, AlgResourceResponse &algRes) override;
    HcclResult RunIntraSeverReduceScatter(const std::string &tag, DeviceMem &inputMem, DeviceMem &outputMem,
        const u64 count, const HcclDataType &dataType, const HcclReduceOp &reductionOp,
        const std::vector<std::vector<Slice>> &multRingsSliceZero, const Stream &stream, s32 profStage,
        const u64 baseOffset = 0, const HcomCollOpInfo *opInfo = nullptr,
        const std::vector<std::vector<Slice>> &multRingsUserMemSlice = std::vector<std::vector<Slice>>(0),
        const bool disableDMAReduce = false) override;

    HcclResult RunLevel0To1(OpParam &param, const ReduceType &reduceType, ExecMem &execMem, Stream &streamL0L1);
    HcclResult RunLevel2(OpParam &param, const ReduceType &reduceType, ExecMem &execMem, Stream &streamL2);
    HcclResult KernelRunLevel0To1(const OpParam &param, ExecMem &execMem, Stream &streamL0L1);
    HcclResult KernelRunLevel2(const OpParam &param, ExecMem &execMem, Stream &streamL2);

    HcclResult DoubleRingReduceScatter(const std::string &tag, DeviceMem inputMem, DeviceMem outputMem,
        const u64 count, const HcclDataType dataType, const HcclReduceOp reductionOp,
        const std::vector<std::vector<Slice>> multRingsSliceZero, Stream stream, s32 profStage,
        const u64 baseOffset, const HcomCollOpInfo *opInfo,
        const std::vector<std::vector<Slice>> multRingsUserMemSlice, const bool disableDMAReduce);

    HcclResult GetSubStreamInfoOnOneRing(const u32 ringIndex,
        std::vector<Stream> &subStreamsInOneRing,
        std::vector<std::shared_ptr<LocalNotify>> &mainSignalsInOneRing,
        std::vector<std::shared_ptr<LocalNotify>> &subSignalsInOneRing) override;
};

}  // namespace hccl

#endif
