/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef REDUCE_SCATTER_LAYERED_H
#define REDUCE_SCATTER_LAYERED_H

#include "layered_base.h"
#include "reduce_scatter_halving_doubling.h"
#include "reduce_scatter_nb.h"
#include "reduce_scatter_nhr.h"
#include "reduce_scatter_nhr_v1.h"
#include "reduce_scatter_recursive_hd.h"
#include "reduce_scatter_ring.h"

namespace hccl {
class ReduceScatterLayered : public LayeredBase {
public:
    explicit ReduceScatterLayered(const HcclDispatcher dispatcher, u64 reduceAttr);
    ~ReduceScatterLayered() override;

    HcclResult Prepare(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam) override;
    void SetOuterReduceScatterContext(const HcomCollOpInfo *opInfo,
        const std::vector<std::vector<Slice>> &multiStreamSlice,
        const std::vector<std::vector<Slice>> &level0DataSegsSlice, u32 perDataSize);
    HcclResult CopyIn(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam);
    HcclResult RunAsync(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam);
    HcclResult CopyOut(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam);
    const std::vector<std::vector<Slice>> &GetOuterUserMemSlices() const;

protected:
    virtual HcclResult RunInnerReduceScatter();
    virtual HcclResult RunCrossReduceScatter();
    virtual HcclResult CopyOutBypassForTest(DeviceMem &dstMem, const DeviceMem &srcMem, Stream &stream);

private:
    HcclResult PrepareSlicesAndBuffers(const ExecMem &execMem);
    HcclResult BuildOuterUserMemSlices(const OpParam &opParam, const ExecMem &execMem);
    HcclResult UpdateOuterUserMemSlicesForStride(const OpParam &opParam);
    u64 CalcCopyOutSrcOffset(const ExecMem &execMem, const OpParam &opParam, u32 perDataSize) const;
    HcclResult RunOnComm(std::unique_ptr<ExecutorBase> executor, const SubCommInfo &commInfo,
        DeviceMem &inputMem, DeviceMem &outputMem, DeviceMem &scratchMem, u64 count, u64 offset,
        const std::vector<Slice> &slices, s32 stage);
    std::unique_ptr<ExecutorBase> CreateLevel1Executor() const;
    std::unique_ptr<ExecutorBase> CreateLevel2Executor() const;
    HcclResult PrepareExecutor(ExecutorBase &executor, DeviceMem &inputMem, DeviceMem &outputMem,
        DeviceMem &scratchMem, u64 count, u64 offset, const std::vector<Slice> &slices,
        bool useInterAlgForLevel2) const;

    u64 reduceAttr_ = 0;
    const HcomCollOpInfo *outerOpInfo_ = nullptr;
    u32 outerPerDataSize_ = 0;
    std::vector<std::vector<Slice>> outerMultiStreamSlice_;
    std::vector<std::vector<Slice>> outerLevel0DataSegsSlice_;
    std::vector<std::vector<Slice>> outerUserMemSlices_;
};
}  // namespace hccl

#endif /* REDUCE_SCATTER_LAYERED_H */
