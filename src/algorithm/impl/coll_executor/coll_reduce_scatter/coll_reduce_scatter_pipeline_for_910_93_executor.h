/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
    // 由 RunLoop 构建，传入 RunL0L1Phase、RunL2Phase
    struct PipelineLoopContext {
        u64 countDataPerLoop;
        u64 countDataLastLoop;
        u64 sizeDataPerLoop;
        u64 numBlockTotal;
        u64 cclInputSizeHalved;
        DeviceMem cclInputAMem;
        DeviceMem cclInputBMem;
        DeviceMem cclOutputAMem;
        DeviceMem cclOutputBMem;
        u8 *curInputPtr;
        u8 *curOutputPtr;
    };

    HcclResult CalcStreamNum(u32 &streamNum) override;
    u64 CalcLoopMaxCount(const u32 unitSize) override;
    HcclResult RunLoop(OpParam &param, AlgResourceResponse &algRes) override;
    // 由 RunLoop 调用
    HcclResult InitPipelineLoopTask(OpParam &param, const PipelineLoopContext &ctx, u64 loopIdx, const u32 unitSize);
    // 由 RunLoop 调用
    HcclResult BuildPipelineLoopContext(OpParam &param, AlgResourceResponse &algRes,
        const u32 unitSize, PipelineLoopContext &ctx);
    // 由 RunLoop 调用
    HcclResult WaitForRemainingL2Signals(const OpParam &param, u64 numBlockTotal,
        Stream &streamL0L1, const std::shared_ptr<LocalNotify> &notifyL2toL0L1A,
        const std::shared_ptr<LocalNotify> &notifyL2toL0L1B);
    HcclResult RunIntraSeverReduceScatter(const std::string &tag, DeviceMem &inputMem, DeviceMem &outputMem,
        const u64 count, const HcclDataType &dataType, const HcclReduceOp &reductionOp,
        const std::vector<std::vector<Slice>> &multRingsSliceZero, const Stream &stream, s32 profStage,
        const u64 baseOffset = 0, const HcomCollOpInfo *opInfo = nullptr,
        const std::vector<std::vector<Slice>> &multRingsUserMemSlice = std::vector<std::vector<Slice>>(0),
        const bool disableDMAReduce = false) override;

    // 由 RunLevel0To1、RunLevel2 调用
    void SliceExecMemIfNeeded(const OpParam &param, ExecMem &execMem);

    // 由 KernelRunLevel0To1、KernelRunLevel2 调用
    HcclResult GetLevel2CommInfo(SubCommInfo &level2CommInfo);

    // 由 RunLoop 循环体调用
    HcclResult RunL0L1Phase(OpParam &param, const PipelineLoopContext &ctx, u64 blockIdx, Stream &streamL0L1);
    // 由 RunLoop 循环体调用
    HcclResult RunL2Phase(OpParam &param, const PipelineLoopContext &ctx, u64 blockIdx, Stream &streamL2);

    HcclResult RunLevel0To1(OpParam &param, ExecMem &execMem, Stream &streamL0L1, const u64 baseOffset);
    HcclResult RunLevel2(OpParam &param, ExecMem &execMem, Stream &streamL2, const u64 baseOffset);
    HcclResult KernelRunLevel0To1(const OpParam &param, ExecMem &execMem, Stream &streamL0L1, const u64 baseOffset);
    HcclResult KernelRunLevel2(const OpParam &param, ExecMem &execMem, Stream &streamL2, const u64 baseOffset);

    // 由 DoubleRingReduceScatter 调用
    HcclResult PrepareDoubleRingSlices(u32 ringNum, const HcclDataType dataType,
        const HcomCollOpInfo *opInfo,
        const std::vector<std::vector<Slice>> &multRingsSliceZero,
        const std::vector<std::vector<Slice>> &multRingsUserMemSlice,
        std::vector<std::vector<Slice>> &userMemInputSlicesOfDoubleRing,
        std::vector<std::vector<u32>> &rankOrders);

    // 由 KernelRunLevel0To1 调用
    HcclResult SelectAndRunLevel1Template(const OpParam &param, ExecMem &execMem,
        Stream &streamL0L1, u64 baseOffset, u32 commIndex, u32 sliceNum,
        u32 level1RankSize, u32 level2RankSize, u32 perDataSize);

    // 由 KernelRunLevel2 调用
    HcclResult SelectAndRunLevel2Template(const OpParam &param, ExecMem &execMem,
        Stream &streamL2, u64 baseOffset, const SubCommInfo &level2CommInfo,
        u32 level2RankSize, u32 perDataSize);

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
