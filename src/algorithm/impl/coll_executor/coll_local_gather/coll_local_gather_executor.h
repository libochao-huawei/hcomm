/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_LOCAL_GATHER_EXECUTOR_H
#define COLL_LOCAL_GATHER_EXECUTOR_H

#include "coll_native_executor_base.h"
#include "coll_alg_exec_registry.h"

namespace hccl {

class CollLocalGatherExecutor : public CollNativeExecutorBase {
public:
    explicit CollLocalGatherExecutor(const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollLocalGatherExecutor() override = default;

    HcclResult Orchestrate(OpParam& param, AlgResourceResponse& algRes) override;

private:
    void ParseParam(const OpParam& param) override;
    HcclResult CalcStreamNum(u32& streamNum) override;
    HcclResult CalcNotifyNum(u32 streamNum, u32 &notifyNum) override;
    HcclResult KernelRun(const OpParam &param, ExecMem &execMem) override;

    u32 splitNum_ = 0;
    u32 numBufs_ = 0;
};

} // namespace hccl

#endif // COLL_LOCAL_GATHER_EXECUTOR_H
