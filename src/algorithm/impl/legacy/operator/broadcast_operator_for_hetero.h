/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BROADCAST_OPERATOR_FOR_HETERO_H
#define BROADCAST_OPERATOR_FOR_HETERO_H

#include "coll_alg_operator.h"

namespace hccl {
class BroadCastOperatorForHetero : public CollAlgOperator {
public:
    BroadCastOperatorForHetero(AlgConfigurator* algConfigurator, CCLBufferManager &cclBufferManager,
        HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher);
    ~BroadCastOperatorForHetero();
    HcclResult Broadcast(const std::string &tag, void *ptr, u64 count, HcclDataType dataType, u32 root,
        Stream stream, HcomCollOpInfo *opInfo = nullptr);
private:
    // broadcast
    HcclResult RunBroadCast(const std::string &tag, DeviceMem &inputMem, DeviceMem &outputMem, u64 count,
        HcclDataType dataType, HcclReduceOp op, u32 root, Stream &stream, HcomCollOpInfo *opInfo = nullptr);
    HcclResult BroadcastStarExecutor(const std::string &tag, DeviceMem &inputMem, DeviceMem &outputMem, u64 count,
        HcclDataType dataType, HcclReduceOp op, u32 root, Stream &stream);
};
}

#endif /* BROADCAST_OPERATOR_FOR_HETERO_H */