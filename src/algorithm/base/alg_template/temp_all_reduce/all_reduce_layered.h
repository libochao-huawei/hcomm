/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ALL_REDUCE_LAYERED_H
#define ALL_REDUCE_LAYERED_H

#include "layered_base.h"
#include "all_reduce_ring.h"
#include "all_reduce_nb.h"
#include "all_reduce_nhr.h"
#include "all_reduce_nhr_v1.h"
#include "all_reduce_recursive_hd.h"
#include "all_gather_ring.h"
#include "all_gather_nb.h"
#include "all_gather_nhr.h"
#include "all_gather_nhr_v1.h"
#include "all_gather_recursive_hd.h"
#include "reduce_scatter_ring.h"
#include "reduce_scatter_nb.h"
#include "reduce_scatter_nhr.h"
#include "reduce_scatter_nhr_v1.h"
#include "reduce_scatter_recursive_hd.h"

namespace hccl {
class AllReduceLayered : public LayeredBase {
public:
    explicit AllReduceLayered(const HcclDispatcher dispatcher, u64 reduceAttr);
    ~AllReduceLayered() override;

    HcclResult Prepare(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam) override;
    HcclResult RunAsync(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam);

protected:
    virtual HcclResult RunInnerReduceScatter();
    virtual HcclResult RunCrossAllReduce();
    virtual HcclResult RunInnerAllGather();

private:
    HcclResult PrepareSlicesAndBuffers(const ExecMem &execMem);
    HcclResult RunTemplateOnComm(std::unique_ptr<AlgTemplateBase> tempAlg, const SubCommInfo &commInfo,
        DeviceMem &inputMem, DeviceMem &outputMem, DeviceMem &scratchMem, u64 count, u64 offset,
        const std::vector<Slice> &slices, HcclReduceOp reduceOp, s32 stage, bool isCrossLevel);
    std::unique_ptr<AlgTemplateBase> CreateLevel1ReduceScatterTemplate() const;
    std::unique_ptr<AlgTemplateBase> CreateLevel1AllGatherTemplate() const;
    std::unique_ptr<AlgTemplateBase> CreateLevel2AllReduceTemplate() const;
    HcclResult PrepareAllGatherTemplate(AlgTemplateBase &tempAlg) const;

    u64 reduceAttr_ = 0;
};
}  // namespace hccl

#endif /* ALL_REDUCE_LAYERED_H */
