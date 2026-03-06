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
        collOp.vDataDes.dataType = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.vDataDes.dataType);
    } else if (collOp.opType == Hccl::OpType::ALLTOALL) {
        collOp.all2AllDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllDataDes.sendType);
        collOp.all2AllDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllDataDes.recvType);
        collOp.all2AllDataDes.sendCount = dfxOpInfo.all2AllDataDes.sendCount;
        collOp.all2AllDataDes.recvCount = dfxOpInfo.all2AllDataDes.recvCount;
    } else if (collOp.opType == Hccl::OpType::ALLTOALLV) {
        collOp.all2AllVDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVDataDes.sendType);
        collOp.all2AllVDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVDataDes.recvType);
        collOp.all2AllVDataDes.sendCounts = dfxOpInfo.all2AllVDataDes.sendCounts;
        collOp.all2AllVDataDes.recvCounts = dfxOpInfo.all2AllVDataDes.recvCounts;
        collOp.all2AllVDataDes.sdispls = dfxOpInfo.all2AllVDataDes.sdispls;
        collOp.all2AllVDataDes.rdispls = dfxOpInfo.all2AllVDataDes.rdispls;
    } else if (collOp.opType == Hccl::OpType::ALLTOALLVC) {
        collOp.all2AllVCDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVCDataDes.sendType);
        collOp.all2AllVCDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVCDataDes.recvType);
        collOp.all2AllVCDataDes.sendCountMatrix = dfxOpInfo.all2AllVCDataDes.sendCountMatrix;
    } else if (collOp.opType == Hccl::OpType::BATCHSENDRECV) {
        collOp.batchSendRecvDataDes.itemNum = dfxOpInfo.batchSendRecvDataDes.itemNum;
        collOp.batchSendRecvDataDes.sendRecvItemsPtr = static_cast<void *>(dfxOpInfo.batchSendRecvDataDes.sendRecvItemsPtr);
    } else {
        collOp.dataDes.dataCount = dfxOpInfo.dataDes.dataCount;
        collOp.dataDes.dataType = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.dataDes.dataType);
        collOp.dataDes.strideCount = dfxOpInfo.dataDes.dataCount;
    }
}

std::shared_ptr<Hccl::Buffer> CreateBufferShared(const Hccl::Buffer* buffer) {
    return buffer ? std::make_shared<Hccl::Buffer>(buffer->GetAddr(), buffer->GetSize()) : nullptr;
}

std::shared_ptr<Hccl::DfxOpInfo> ConvertToDfxOpInfo(const HcclDfxOpInfo& dfxOpInfo) {
    auto dfxOpInfoOnce = std::make_shared<Hccl::DfxOpInfo>();
    Hccl::CollOperator collOp{};
    collOp.opMode = dfxOpInfo.opMode; 
    collOp.opType = Hccl::OP_TYPE_MAP.at(dfxOpInfo.opType);
    collOp.reduceOp = Hccl::REDUCE_OP_MAP.at(dfxOpInfo.reduceOp);
    collOp.dataType = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.dataType);
    collOp.outputDataType = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.outputType);
    collOp.dataCount = dfxOpInfo.dataCount;
    collOp.root = dfxOpInfo.root;
    collOp.staticAddr = dfxOpInfo.staticAddr;
    collOp.staticShape = dfxOpInfo.staticShape;
    collOp.numBlocksLimit = dfxOpInfo.numBlocksLimit;
    SetCollopDataDes(collOp, dfxOpInfo);
    collOp.inputMem = CreateBufferShared(dfxOpInfo.inputMem.get());
    collOp.outputMem = CreateBufferShared(dfxOpInfo.outputMem.get());
    collOp.scratchMem = CreateBufferShared(dfxOpInfo.scratchMem.get());
    dfxOpInfoOnce->op_= std::move(collOp);
    dfxOpInfoOnce->tag_ = dfxOpInfo.tag_;
    dfxOpInfoOnce->algType_ = dfxOpInfo.algType;
    dfxOpInfoOnce->index_ = dfxOpInfo.index_;
    dfxOpInfoOnce->mainStreamId_ = dfxOpInfo.mainStreamId_;
    dfxOpInfoOnce->beginTime_ = dfxOpInfo.beginTime_;
    return dfxOpInfoOnce;
}

}