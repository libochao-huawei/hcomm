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

void SetCollopDataDes(Hccl::CollOperator& collOp, const HcclDfxOpInfo& dfxOpInfo) {
    if (collOp.opType == Hccl::OpType::ALLGATHERV) {
        collOp.vDataDes.counts = dfxOpInfo.vDataDes.counts;
        collOp.vDataDes.displs = dfxOpInfo.vDataDes.displs;
        collOp.vDataDes.dataType = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.vDataDes.dataType));
    } else if (collOp.opType == Hccl::OpType::ALLTOALL) {
        collOp.all2AllDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.all2AllDataDes.sendType));
        collOp.all2AllDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.all2AllDataDes.recvType));
        collOp.all2AllDataDes.sendCount = dfxOpInfo.all2AllDataDes.sendCount;
        collOp.all2AllDataDes.recvCount = dfxOpInfo.all2AllDataDes.recvCount;
    } else if (collOp.opType == Hccl::OpType::ALLTOALLV) {
        collOp.all2AllVDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.all2AllVDataDes.sendType));
        collOp.all2AllVDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.all2AllVDataDes.recvType));
        collOp.all2AllVDataDes.sendCounts = dfxOpInfo.all2AllVDataDes.sendCounts;
        collOp.all2AllVDataDes.recvCounts = dfxOpInfo.all2AllVDataDes.recvCounts;
        collOp.all2AllVDataDes.sdispls = dfxOpInfo.all2AllVDataDes.sdispls;
        collOp.all2AllVDataDes.rdispls = dfxOpInfo.all2AllVDataDes.rdispls;
    } else if (collOp.opType == Hccl::OpType::ALLTOALLVC) {
        collOp.all2AllVCDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.all2AllVCDataDes.sendType));
        collOp.all2AllVCDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.all2AllVCDataDes.recvType));
        collOp.all2AllVCDataDes.sendCountMatrix = dfxOpInfo.all2AllVCDataDes.sendCountMatrix;
    } else if (collOp.opType == Hccl::OpType::BATCHSENDRECV) {
        collOp.batchSendRecvDataDes.itemNum = dfxOpInfo.batchSendRecvDataDes.itemNum;
        collOp.batchSendRecvDataDes.sendRecvItemsPtr = static_cast<void *>(dfxOpInfo.batchSendRecvDataDes.sendRecvItemsPtr);
    } else {
        collOp.dataDes.dataCount = dfxOpInfo.dataDes.dataCount;
        collOp.dataDes.dataType = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.dataDes.dataType));
        collOp.dataDes.strideCount = dfxOpInfo.dataDes.dataCount;
    }
}


std::shared_ptr<Hccl::DfxOpInfo> ConvertToDfxOpInfo(const HcclDfxOpInfo& dfxOpInfo) {
    auto dfxOpInfoOnce = std::make_shared<Hccl::DfxOpInfo>();
    Hccl::CollOperator collOp{};
    collOp.opMode = static_cast<Hccl::OpMode::Value>(dfxOpInfo.opMode); 
    collOp.opType = Hccl::OP_TYPE_MAP.at(static_cast<HcclCMDType>(dfxOpInfo.opType));
    collOp.reduceOp = Hccl::REDUCE_OP_MAP.at(static_cast<HcclReduceOp>(dfxOpInfo.reduceOp));
    collOp.dataType = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.dataType));
    collOp.outputDataType = Hccl::DATA_TYPE_MAP.at(static_cast<HcclDataType>(dfxOpInfo.outputType));
    collOp.dataCount = dfxOpInfo.dataCount;
    collOp.root = dfxOpInfo.root;
    collOp.staticAddr = dfxOpInfo.staticAddr;
    collOp.staticShape = dfxOpInfo.staticShape;
    collOp.numBlocksLimit = dfxOpInfo.numBlocksLimit;
    SetCollopDataDes(collOp, dfxOpInfo);
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