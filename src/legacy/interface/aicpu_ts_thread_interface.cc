/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_ts_thread_interface.h"

#include <memory>
#include <limits>

#include "stream_lite.h"
#include "rtsq_a5.h"

namespace Hccl {

namespace { // make the definitions file-scoped

inline HcclResult GetRtsqWithNullCheck(void *streamLiteVoidPtr, RtsqBase *&rtsqPtr)
{
    StreamLite *streamLitePtr = static_cast<StreamLite *>(streamLiteVoidPtr);
    CHK_PTR_NULL(streamLitePtr);

    rtsqPtr = streamLitePtr->GetRtsq();
    CHK_PTR_NULL(rtsqPtr);

    return HCCL_SUCCESS;
}

std::unordered_map<uint32_t, ReduceOp> mapU32ToReduceOp
    = {{0, ReduceOp::SUM}, {1, ReduceOp::PROD}, {2, ReduceOp::MAX}, {3, ReduceOp::MIN}};

std::unordered_map<uint32_t, DataType> mapU32ToDataType
    = {{0, DataType::INT8},    {1, DataType::INT16},  {2, DataType::INT32},    {3, DataType::FP16},
       {4, DataType::FP32},    {5, DataType::INT64},  {6, DataType::UINT64},   {7, DataType::UINT8},
       {8, DataType::UINT16},  {9, DataType::UINT32}, {10, DataType::FP64},    {11, DataType::BFP16},
       {12, DataType::INT128}, {14, DataType::HIF8},  {15, DataType::FP8E4M3}, {16, DataType::FP8E5M2},
       {17, DataType::FP8E8M0}};

inline HcclResult CheckDataTypeAndReduceOp(uint32_t dataType, uint32_t reduceOp)
{
    if (mapU32ToDataType.find(dataType) == mapU32ToDataType.end()) {
        HCCL_ERROR("[IAicpuTsThread][%s] type[%u] is not supported.", __func__, dataType);
        return HCCL_E_PARA;
    }
    if (mapU32ToReduceOp.find(reduceOp) == mapU32ToReduceOp.end()) {
        HCCL_ERROR("[IAicpuTsThread][%s] op[%u] is not supported.", __func__, reduceOp);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

} // namespace

IAicpuTsThread::IAicpuTsThread() : streamLiteVoidPtr_(nullptr)
{
}

IAicpuTsThread::~IAicpuTsThread()
{
    StreamLite *streamLitePtr = static_cast<StreamLite *>(streamLiteVoidPtr_);
    if(streamLitePtr != nullptr){
        delete streamLitePtr;
        streamLiteVoidPtr_ = nullptr;
    }
}

void IAicpuTsThread::StreamLiteInit(uint32_t id, uint32_t sqIds, uint32_t phyId, uint32_t logicCqids)
{
    StreamLite *streamLitePtr = new StreamLite(id, sqIds, phyId, logicCqids, true);
    streamLiteVoidPtr_        = static_cast<void *>(streamLitePtr);
}

void IAicpuTsThread::LaunchTask() const
{
    RtsqBase *rtsqA5 = nullptr;
    HcclResult ret = GetRtsqWithNullCheck(streamLiteVoidPtr_, rtsqA5);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[IAicpuTsThread::%s] GetRtsqWithNullCheck FAILED.", __func__);
        return;
    }
    
    HCCL_INFO("[IAicpuTsThread::%s] Launch Task @ Stream id [%u]",
        __func__,
        static_cast<StreamLite *>(streamLiteVoidPtr_)->GetId());

    rtsqA5->LaunchTask();
    return;
}

HcclResult IAicpuTsThread::NotifyWait(uint32_t notifyId) const
{
    RtsqBase *rtsqA5 = nullptr;
    CHK_RET(GetRtsqWithNullCheck(streamLiteVoidPtr_, rtsqA5));
    
    HCCL_INFO("[IAicpuTsThread::%s] @ Stream id [%u], notifyId [%u]",
        __func__,
        static_cast<StreamLite *>(streamLiteVoidPtr_)->GetId(),
        notifyId);

    rtsqA5->NotifyWait(notifyId);

    return HCCL_SUCCESS;
}

HcclResult IAicpuTsThread::NotifyRecordLoc(uint32_t notifyId) const
{
    RtsqBase *rtsqA5 = nullptr;
    CHK_RET(GetRtsqWithNullCheck(streamLiteVoidPtr_, rtsqA5));

    HCCL_INFO("[IAicpuTsThread::%s] @ Stream id [%u], notifyId [%u]",
        __func__,
        static_cast<StreamLite *>(streamLiteVoidPtr_)->GetId(),
        notifyId);

    rtsqA5->NotifyRecordLoc(notifyId);

    return HCCL_SUCCESS;
}

HcclResult IAicpuTsThread::SdmaCopy(uint64_t dstAddr, uint64_t srcAddr, uint64_t sizeByte) const
{
    if (sizeByte > std::numeric_limits<uint32_t>::max()) {
        HCCL_ERROR("[%s] sizeByte [%ld] exceeds the maximum value of uint32", __func__, sizeByte);
        return HCCL_E_PARA;
    }

    RtsqBase *rtsqA5 = nullptr;
    CHK_RET(GetRtsqWithNullCheck(streamLiteVoidPtr_, rtsqA5));

    uint32_t partId           = 0; // partId will not be used
    uint32_t sizeByteNarrowed = static_cast<uint32_t>(sizeByte);

    HCCL_INFO("[IAicpuTsThread::%s] @ Stream id [%u], dstAddr [%llx], srcAddr [%llx], sizeByteNarrowed [%u]",
        __func__,
        static_cast<StreamLite *>(streamLiteVoidPtr_)->GetId(),
        dstAddr,
        srcAddr,
        sizeByteNarrowed);

    rtsqA5->SdmaCopy(srcAddr, dstAddr, sizeByteNarrowed, partId);

    return HCCL_SUCCESS;
}

HcclResult IAicpuTsThread::SdmaReduce(uint64_t dstAddr, uint64_t srcAddr, uint64_t sizeByte, uint32_t dataTypeRaw,
                                          uint32_t reduceOpRaw) const
{
    if (sizeByte > std::numeric_limits<uint32_t>::max()) {
        HCCL_ERROR("[%s] sizeByte [%ld] exceeds the maximum value of uint32", __func__, sizeByte);
        return HCCL_E_PARA;
    }

    RtsqBase *rtsqA5 = nullptr;
    CHK_RET(GetRtsqWithNullCheck(streamLiteVoidPtr_, rtsqA5));

    CHK_RET(CheckDataTypeAndReduceOp(dataTypeRaw, reduceOpRaw));
    DataType dataType = mapU32ToDataType.at(dataTypeRaw);
    ReduceOp reduceOp = mapU32ToReduceOp.at(reduceOpRaw);
    ReduceIn reduceIn{dataType, reduceOp};

    uint32_t partId           = 0; // partId will not be used
    uint32_t sizeByteNarrowed = static_cast<uint32_t>(sizeByte);

    HCCL_INFO("[IAicpuTsThread::%s] @ Stream id [%u], dstAddr [%llx], srcAddr [%llx], sizeByteNarrowed [%u], dataType [%u][%s], reduceOp [%u][%s]",
        __func__,
        static_cast<StreamLite *>(streamLiteVoidPtr_)->GetId(),
        dstAddr,
        srcAddr,
        sizeByteNarrowed,
        dataTypeRaw,
        dataType.Describe().c_str(), 
        reduceOpRaw,
        reduceOp.Describe().c_str());

    rtsqA5->SdmaReduce(srcAddr, dstAddr, sizeByteNarrowed, partId, reduceIn);

    return HCCL_SUCCESS;
}

HcclResult IAicpuTsThread::GetStreamLitePtr(void **streamLitePtrPtr) const
{
    CHK_PTR_NULL(streamLiteVoidPtr_);
    *streamLitePtrPtr = streamLiteVoidPtr_;
    return HCCL_SUCCESS;
}

HcclResult IAicpuTsThread::GetSqId(uint32_t &sqId) const
{
    CHK_PTR_NULL(streamLiteVoidPtr_);
    StreamLite *streamLitePtr = static_cast<StreamLite *>(streamLiteVoidPtr_);
    sqId                      = streamLitePtr->GetSqId();
    return HCCL_SUCCESS;
}
} // namespace Hccl