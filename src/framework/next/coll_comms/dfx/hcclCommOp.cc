/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
//TODO:以后都搬走

#include "hcclCommOp.h"
namespace hccl {

std::shared_ptr<Hccl::DfxOpInfo> ConvertToDfxOpInfo(const HcclDfxOpInfo& dfxOpInfo) {
    auto dfxOpInfoOnce = std::make_shared<Hccl::DfxOpInfo>();
    Hccl::CollOperator collOp{};
    collOp.opMode = static_cast<Hccl::OpMode::Value>(dfxOpInfo.opMode); 
    collOp.opType = Hccl::OP_TYPE_MAP.at(static_cast<HcclCMDType>(dfxOpInfo.opType));
    collOp.reduceOp = Hccl::HcclReduceOpToReduceOp(static_cast<HcclReduceOp>(dfxOpInfo.reduceOp));
    collOp.dataType = Hccl::HcclDataTypeToDataType(static_cast<HcclDataType>(dfxOpInfo.dataType));
    collOp.dataCount = dfxOpInfo.dataCount;
    collOp.root = dfxOpInfo.root;
    collOp.staticAddr = dfxOpInfo.staticAddr;
    collOp.staticShape = dfxOpInfo.staticShape;
    collOp.inputMem = std::make_shared<Hccl::Buffer>(
        reinterpret_cast<uintptr_t>(dfxOpInfo.inputMemPtr),
        static_cast<std::size_t>(dfxOpInfo.inputMemSize)
    );
    collOp.outputMem = std::make_shared<Hccl::Buffer>(
        reinterpret_cast<uintptr_t>(dfxOpInfo.outputMemPtr),
        static_cast<std::size_t>(dfxOpInfo.outputMemSize)
    );
    collOp.scratchMem = std::make_shared<Hccl::Buffer>(
        reinterpret_cast<uintptr_t>(dfxOpInfo.scratchMemPtr),
        static_cast<std::size_t>(dfxOpInfo.scratchMemSize)
    );
    dfxOpInfoOnce->op_= std::move(collOp);
    dfxOpInfoOnce->tag_ = dfxOpInfo.tag_;
    dfxOpInfoOnce->algType_ = Hccl::AlgType::MESH;
    dfxOpInfoOnce->index_ = dfxOpInfo.index_;
    dfxOpInfoOnce->beginTime_ = dfxOpInfo.beginTime_;
    return dfxOpInfoOnce;
}

}