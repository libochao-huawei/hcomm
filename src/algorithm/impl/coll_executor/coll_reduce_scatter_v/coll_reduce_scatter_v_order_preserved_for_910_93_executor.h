/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_REDUCESCATTERV_ORDER_PRESERVED_FOR_910_93_EXECUTOR_H
#define COLL_REDUCESCATTERV_ORDER_PRESERVED_FOR_910_93_EXECUTOR_H
#include "coll_reduce_scatter_v_executor.h"

namespace hccl {
class CollReduceScatterVOrderPreservedFor91093Executor : public CollReduceScatterVExecutor {
public:
    explicit CollReduceScatterVOrderPreservedFor91093Executor(const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher);

private:
    void ParseParam(const OpParam& param) override;
    /* *************** 资源计算 *************** */
    HcclResult CalcScratchMemSize(u64& scratchMemSize) override;
    u32 CalReduceStreamNum(const u32& localRankSize) const;
    HcclResult CalcStreamNum(u32& streamNum) override;
    HcclResult CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult CalcLevel1CommInfo(TransportMemType inputType, TransportMemType outputType,
        std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult CalcLevel2CommInfo(TransportMemType inputType, TransportMemType outputType,
        std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult CalcTransportMemType(TransportMemType &inputType, TransportMemType &outputType) const;
    u64 CalcLoopMaxCount(const u32 unitSize) override;
    HcclResult CalcCurCountsAndCurDispls(const u64 maxTotalCount, std::vector<u64> &countsLeft,
        std::vector<u64> &displs, std::vector<u64> &curCounts, std::vector<u64> &curDispls, bool &finished) override;

    /* *************** 算法编排 *************** */
    HcclResult KernelRun(const OpParam &param, ExecMem &execMem) override;
    HcclResult RunReduceScatterVLevel1(const OpParam &param, ExecMem &execMem, SubCommInfo &level1CommInfo);
    HcclResult RunReduceScatterVLevel2(const OpParam &param, ExecMem &execMem, SubCommInfo &level1CommInfo);

    u32 all2allOffset_ = 0;
    u64 maxCount_ = 0;
    u64 minBiasOffset_ = 0;
};

} // namespace hccl

#endif