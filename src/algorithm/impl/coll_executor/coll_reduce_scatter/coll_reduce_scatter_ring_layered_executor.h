/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_REDUCESCATTER_RING_LAYERED_EXECUTOR_H
#define COLL_REDUCESCATTER_RING_LAYERED_EXECUTOR_H

#include "coll_reduce_scatter_ring_for_910_93_executor.h"

namespace hccl {
class CollReduceScatterRingLayeredExecutor : public CollReduceScatterRingFor91093Executor {
public:
    explicit CollReduceScatterRingLayeredExecutor(const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollReduceScatterRingLayeredExecutor() override = default;

protected:
    virtual HcclResult ActiveSlaveStreamsForTest(const Stream &stream);
    virtual HcclResult MultiRingReduceScatterForTest(const std::string &tag, DeviceMem inputMem, DeviceMem outputMem,
        u64 count, HcclDataType dataType, HcclReduceOp reductionOp,
        const std::vector<std::vector<Slice>> &multRingsSliceZero, Stream stream, s32 profStage,
        u64 baseOffset, const HcomCollOpInfo *opInfo,
        const std::vector<std::vector<Slice>> &multRingsUserMemSlice);

private:
    HcclResult CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult KernelRun(const OpParam &param, ExecMem &execMem) override;
    HcclResult CalcLevel1DataSegsSlice(const ExecMem &execMem, u32 commIndex, u32 sliceNum,
        u32 innerRankSize, u32 crossRankSize, std::vector<Slice> &innerDataSegsSlice);
    HcclResult CalcLevel2DataSegsSlice(const ExecMem &execMem, u32 crossRankSize,
        std::vector<Slice> &crossDataSegsSlice);
    void FillMultiRingSlice(const ExecMem &execMem, const std::vector<std::vector<Slice>> &multiStreamSlice,
        u32 sliceNum, u32 innerRankSize, u32 crossRankSize, u32 ringIndex, std::vector<Slice> &dataSlice);
    HcclResult CalLevel0DataSegsSlice(const ExecMem &execMem, const OpParam &param, u32 ringNum, u32 sliceNum,
        u32 innerRankSize, u32 crossRankSize, HcclDataType dataType,
        std::vector<std::vector<Slice>> &multiStreamSlice, std::vector<std::vector<Slice>> &level0DataSegsSlice);
};
}  // namespace hccl

#endif /* COLL_REDUCESCATTER_RING_LAYERED_EXECUTOR_H */
