/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ALL_GATHER_LAYERED_H
#define ALL_GATHER_LAYERED_H

#include "layered_base.h"
#include "all_gather_ring.h"
#include "all_gather_nb.h"
#include "all_gather_nhr.h"
#include "all_gather_nhr_v1.h"
#include "all_gather_recursive_hd.h"

namespace hccl {
class AllGatherLayered : public LayeredBase {
public:
    explicit AllGatherLayered(const HcclDispatcher dispatcher);
    ~AllGatherLayered() override;

    HcclResult Prepare(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam) override;
    HcclResult CopyIn(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam);
    HcclResult RunAsync(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam);
    HcclResult CopyOut(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam);

protected:
    virtual HcclResult RunCrossAllGather();
    virtual HcclResult RunInnerAllGather();
    virtual HcclResult CopyBypassForTest(DeviceMem &dstMem, const DeviceMem &srcMem, Stream &stream);

private:
    HcclResult PrepareSlicesAndBuffers(const ExecMem &execMem);
    HcclResult RunTemplateOnComm(std::unique_ptr<AlgTemplateBase> tempAlg, const SubCommInfo &commInfo,
        DeviceMem &inputMem, DeviceMem &outputMem, DeviceMem &scratchMem, u64 count, u64 offset,
        const std::vector<Slice> &slices, s32 stage);
    std::unique_ptr<AlgTemplateBase> CreateLevel1Template() const;
    std::unique_ptr<AlgTemplateBase> CreateLevel2Template() const;
    HcclResult PrepareTemplate(AlgTemplateBase &tempAlg) const;

    u64 unitBytes_ = 0;
    u64 localCopyOffset_ = 0;
};
}  // namespace hccl

#endif /* ALL_GATHER_LAYERED_H */
