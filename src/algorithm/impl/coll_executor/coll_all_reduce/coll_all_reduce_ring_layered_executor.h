/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_ALLREDUCE_RING_LAYERED_EXECUTOR_H
#define COLL_ALLREDUCE_RING_LAYERED_EXECUTOR_H

#include "coll_all_reduce_ring_for_910_93_executor.h"

namespace hccl {
class CollAllReduceRingLayeredExecutor : public CollAllReduceRingFor91093Executor {
public:
    explicit CollAllReduceRingLayeredExecutor(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollAllReduceRingLayeredExecutor() override = default;

protected:
    virtual HcclResult ActiveSlaveStreamsForTest(const Stream &stream);
    virtual HcclResult MultiRingReduceScatterForTest(const std::string &tag, DeviceMem inputMem, DeviceMem outputMem,
        u64 count, HcclDataType dataType, HcclReduceOp reductionOp,
        const std::vector<std::vector<Slice>> &multRingsSliceZero, Stream stream, s32 profStage,
        u64 baseOffset, const HcomCollOpInfo *opInfo);
    virtual HcclResult MultiRingAllGatherForTest(const std::string &tag, DeviceMem inputMem, DeviceMem outputMem,
        u64 count, HcclDataType dataType, const std::vector<std::vector<Slice>> &multRingsSliceZero, Stream stream,
        s32 profStage, u64 baseOffset, const HcomCollOpInfo *opInfo);

private:
    HcclResult CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult KernelRun(const OpParam &param, ExecMem &execMem) override;
    HcclResult CalcLevel1DataSegsSlice(const SubCommInfo &outerCommInfo, const SubCommInfo &innerCommInfo,
        const std::vector<Slice> &outerDataSlices, HcclDataType dataType, std::vector<Slice> &innerDataSlices) const;
    HcclResult CalcLevel2DataSegsSlice(const SubCommInfo &innerCommInfo, const SubCommInfo &crossCommInfo,
        const std::vector<Slice> &innerDataSlices, HcclDataType dataType, std::vector<Slice> &crossDataSlices) const;
};
}  // namespace hccl

#endif /* COLL_ALLREDUCE_RING_LAYERED_EXECUTOR_H */
