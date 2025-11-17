/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GATHER_OPERATOR_H
#define GATHER_OPERATOR_H

#include "coll_alg_operator.h"

namespace hccl {
class GatherOperator : public CollAlgOperator {
public:
    GatherOperator(AlgConfigurator* algConfigurator, CCLBufferManager &cclBufferManager,
        HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher);
    ~GatherOperator();
    HcclResult Gather(const std::string &tag, void *inputPtr, void *outputPtr, u32 rootRank, u64 inputCount,
        HcclDataType dataType, Stream stream);
private:
    // gather
    HcclResult GatherStarExecutor(const std::string &tag, DeviceMem &inputMem, DeviceMem &outputMem, u64 count,
        HcclDataType dataType, HcclReduceOp op, u32 root, Stream &stream);
};
}

#endif /* __GATHER_OPERATOR_H__ */