/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_ts_thread.h"
#include "aicpu/aicpu_hccl_sqcq.h"

// specify namespaces for the macro TRY_CATCH_*
using string = std::string;
using exception = std::exception;
using HcclException = Hccl::HcclException;

namespace hccl {
AicpuTsThread::AicpuTsThread(StreamType streamType, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
    : streamType_(streamType), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType)
{}

AicpuTsThread::AicpuTsThread(const std::string &uniqueIdStr) : uniqueIdStr_(uniqueIdStr)
{}

AicpuTsThread::~AicpuTsThread()
{
    DeInit();
}

HcclResult AicpuTsThread::Init()
{
    CHK_RET(GetRunSideIsDevice(isDeviceSide_));
    if (!isDeviceSide_) {
        // host侧申请资源
        HCCL_INFO("HcclThread::%s, is hostside", __func__);
        return HostInit();
    } else {
        // device侧反序列化，恢复资源
        HCCL_INFO("HcclThread::%s, is DeviceSide", __func__);
        return DeviceInit();
    }
}

HcclResult AicpuTsThread::DeInit()
{
    streamType_ = StreamType::STREAM_TYPE_RESERVED;
    notifyNum_ = 0;
    stream_ = nullptr;
    notifys_.clear();
    uniqueIdStr_ = std::string();
    devType_ = DevType::DEV_TYPE_COUNT;
    return HCCL_SUCCESS;
}

std::string &AicpuTsThread::GetUniqueId()
{
    if (!uniqueIdStr_.empty()) {
        return uniqueIdStr_;
    }

    // 序列化信息
    std::ostringstream oss;
    oss.write(reinterpret_cast<const char_t *>(&streamType_), sizeof(streamType_));
    oss.write(reinterpret_cast<const char_t *>(&notifyLoadType_), sizeof(notifyLoadType_));
    oss.write(reinterpret_cast<const char_t *>(&devId_), sizeof(devId_));
    oss.write(reinterpret_cast<const char_t *>(&notifyNum_), sizeof(notifyNum_));

    HcclStreamParam streamParam;
    streamParam.streamInfo.streamIds = stream_->id();
    streamParam.streamInfo.sqIds = stream_->sqId();
    streamParam.streamInfo.cqIds = stream_->cqId();
    streamParam.streamInfo.logicCqids = stream_->logicCqId();
    streamParam.sqCqContextAddr = reinterpret_cast<uint64_t>(sqCqeContext_.ptr());
    streamParam.sqCqContextSize = sqCqeContext_.size();
    oss.write(reinterpret_cast<const char_t *>(&streamParam), sizeof(streamParam));

    HcclResult ret = HCCL_SUCCESS;
    for (uint32_t idx = 0; idx < notifyNum_; idx++) {
        HcclSignalInfo notifyInfo;
        ret = notifys_[idx]->GetNotifyData(notifyInfo);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[AicpuTsThread][GetUniqueId]GetNotifyData failed, ret[%d]", ret);
            uniqueIdStr_ = std::string();
            return uniqueIdStr_;
        }
        HCCL_INFO("[AicpuTsThread][GetUniqueId]get local notify data success, resId[%u], tsId:%d, devId[%u]",
            notifyInfo.resId,
            notifyInfo.tsId,
            notifyInfo.devId);
        oss.write(reinterpret_cast<const char_t *>(&notifyInfo), sizeof(notifyInfo));
    }
    HCCL_DEBUG("[AicpuTsThread][GetUniqueId] stream[%p], notifyNum[%u]", stream_->ptr(), notifyNum_);

    uniqueIdStr_ = oss.str();
    return uniqueIdStr_;
}

#ifdef CCL_KERNEL_AICPU
HcclResult AicpuTsThread::BuildComStreamInfo(const HcclStreamInfo &streamInfo, HcclComStreamInfo &comStreamInfo) const
{
    comStreamInfo.sqId = streamInfo.sqIds;
    comStreamInfo.actualStreamId = streamInfo.streamIds;
    comStreamInfo.logicCqId = streamInfo.logicCqids;
    u64 sqAddr = 0;
    CHK_RET(QuerySqBaseAddr(devId_, streamInfo.sqIds, sqAddr));
    comStreamInfo.sqBaseAddr = reinterpret_cast<void *>(sqAddr);
    if (comStreamInfo.sqBaseAddr == nullptr) {
        HCCL_ERROR("[AicpuTsThread::InitStream] sqe base addr ptr is null.");
        return HCCL_E_PARA;
    }
    CHK_RET(QuerySqStatusByType(devId_, streamInfo.sqIds, DRV_SQCQ_PROP_SQ_DEPTH, comStreamInfo.sqDepth));
    HCCL_DEBUG("[AicpuTsThread::InitStream] get stream data success, "
               "streamId[%d], sqId[%d], logicCqId[%u], sqDepth[%u]",
        comStreamInfo.actualStreamId,
        comStreamInfo.sqId,
        comStreamInfo.logicCqId,
        comStreamInfo.sqDepth);
    return HCCL_SUCCESS;
}
#endif

HcclResult AicpuTsThread::InitStream(HcclStreamParam &streamParam)
{
#ifdef CCL_KERNEL_AICPU
    HcclStreamInfo &streamInfo = streamParam.streamInfo;

    static bool isCustom = false;
    static bool init = false;

    if (UNLIKELY(!init)) {
        uint32_t cpType = DEVDRV_PROCESS_CPTYPE_MAX;
        unsigned int hostpid = 0;
        CHK_RET(HrtHalDrvQueryProcessHostPid(getpid(), nullptr, nullptr, &hostpid, &cpType));
        isCustom = cpType == static_cast<uint32_t>(DEVDRV_PROCESS_CP2) ? true : false;
        init = true;
    }
    HcclResult ret = hrtHalResourceIdRestore(devId_, 0, DRV_STREAM_ID, streamInfo.streamIds, 0);
    // custom进程需要恢复stream资源, custom进程调用失败直接报错，aicpu进程调用失败做兼容性处理
    if (ret == HCCL_E_NOT_SUPPORT) {
        CHK_PRT_RET(isCustom,
            HCCL_ERROR(
                "%s hrtHalResourceIdRestore fail, drv not support, custom[%d], ret[%d]", __func__, isCustom, ret),
            HCCL_E_DRV);
    } else if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("%s hrtHalResourceIdRestore fail, ret[%d]", __func__, ret);
        return HCCL_E_DRV;
    }

    HcclComStreamInfo comStreamInfo{0};
    CHK_RET(BuildComStreamInfo(streamInfo, comStreamInfo));

    stream_.reset(new (std::nothrow) Stream(comStreamInfo));
    CHK_SMART_PTR_NULL(stream_);

    // 初始化stream的sqeContext
    SqCqeContext *sqCqeContext = reinterpret_cast<SqCqeContext *>(streamParam.sqCqContextAddr);
    uint64_t sqCqContextSize = streamParam.sqCqContextSize;
    if (sqCqeContext == nullptr || sqCqContextSize != sizeof(SqCqeContext)) {
        HCCL_ERROR("%s fail, sqCqeContext[%p] is null or size[%llu] is not equal to SqCqeContext size[%llu]",
            __func__,
            sqCqeContext,
            sqCqContextSize,
            sizeof(SqCqeContext));
        return HCCL_E_PARA;
    }
    sqCqeContext_ = DeviceMem::create(reinterpret_cast<void *>(sqCqeContext), sqCqContextSize);

    uint32_t sqTail = 0;
    uint32_t sqHead = 0;
    CHK_RET(QuerySqStatusByType(devId_, streamInfo.sqIds, DRV_SQCQ_PROP_SQ_TAIL, sqTail));
    CHK_RET(QuerySqStatusByType(devId_, streamInfo.sqIds, DRV_SQCQ_PROP_SQ_HEAD, sqHead));
    HCCL_DEBUG("[AicpuTsThread::InitStream] sqHead[%u], sqTail[%u]", sqHead, sqTail);

    ret = stream_->InitSqAndCqeContext(sqHead, sqTail, sqCqeContext);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("%s InitSqAndCqeContext failed", __func__), ret);
    HCCL_INFO("%s success, streamId[%d]", __func__, stream_->id());
#endif
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::InitStreamLite(HcclStreamInfo &streamParam, uint32_t hostPhyId)
{
    TRY_CATCH_RETURN(streamA5_ = std::make_unique<Hccl::StreamLite>(
        streamParam.streamIds, streamParam.sqIds, hostPhyId, streamParam.cqIds));
    return HCCL_SUCCESS;
}

uint32_t AicpuTsThread::GetNotifyNum() const
{
    return notifyNum_;
}

LocalNotify *AicpuTsThread::GetNotify(uint32_t index) const
{
    if (index >= notifyNum_) {
        HCCL_ERROR("[AicpuTsThread][GetNotify] notifyNum[%u], index[%u] out of range[0, %u]",
            notifyNum_,
            index,
            notifyNum_ - 1);
        return nullptr;
    }
    return notifys_[index].get();
}

bool AicpuTsThread::IsDeviceA5() const
{
    return devType_ == DevType::DEV_TYPE_910_95;
}

// A3 Stream
Stream *AicpuTsThread::GetStream() const
{
    return stream_.get();
}

// A5 Stream
Hccl::StreamLite *AicpuTsThread::GetStreamLitePtr() const
{
    return streamA5_.get();
}

HcclResult AicpuTsThread::LaunchTask() const
{
    CHK_PTR_NULL(streamA5_);
    HCCL_INFO("[AicpuTsThread::%s] streamId[%u]", __func__, streamA5_->GetId());
    Hccl::RtsqBase *const rtsqPtr = streamA5_->GetRtsq();
    CHK_PTR_NULL(rtsqPtr);
    TRY_CATCH_RETURN(rtsqPtr->LaunchTask());
    return HCCL_SUCCESS;
}

namespace { // make the definitions file-scoped

std::unordered_map<HcommReduceOp, Hccl::ReduceOp> mapU32ToReduceOp = {
    {HCOMM_REDUCE_SUM, Hccl::ReduceOp::SUM},
    {HCOMM_REDUCE_PROD, Hccl::ReduceOp::PROD},
    {HCOMM_REDUCE_MAX, Hccl::ReduceOp::MAX},
    {HCOMM_REDUCE_MIN, Hccl::ReduceOp::MIN},
};

std::unordered_map<HcommDataType, Hccl::DataType> mapU32ToDataType = {
    {HCOMM_DATA_TYPE_INT8, Hccl::DataType::INT8},
    {HCOMM_DATA_TYPE_INT16, Hccl::DataType::INT16},
    {HCOMM_DATA_TYPE_INT32, Hccl::DataType::INT32},
    {HCOMM_DATA_TYPE_FP16, Hccl::DataType::FP16},
    {HCOMM_DATA_TYPE_FP32, Hccl::DataType::FP32},
    {HCOMM_DATA_TYPE_INT64, Hccl::DataType::INT64},
    {HCOMM_DATA_TYPE_UINT64, Hccl::DataType::UINT64},
    {HCOMM_DATA_TYPE_UINT8, Hccl::DataType::UINT8},
    {HCOMM_DATA_TYPE_UINT16, Hccl::DataType::UINT16},
    {HCOMM_DATA_TYPE_UINT32, Hccl::DataType::UINT32},
    {HCOMM_DATA_TYPE_FP64, Hccl::DataType::FP64},
    {HCOMM_DATA_TYPE_BFP16, Hccl::DataType::BFP16},
    {HCOMM_DATA_TYPE_INT128, Hccl::DataType::INT128},
#ifndef OPEN_BUILD_PROJECT
    {HCOMM_DATA_TYPE_HIF8, Hccl::DataType::HIF8},
    {HCOMM_DATA_TYPE_FP8E4M3, Hccl::DataType::FP8E4M3},
    {HCOMM_DATA_TYPE_FP8E5M2, Hccl::DataType::FP8E5M2},
    {HCOMM_DATA_TYPE_FP8E8M0, Hccl::DataType::FP8E8M0},
#endif
};

inline HcclResult CheckDataTypeAndReduceOp(HcommDataType dataType, HcommReduceOp reduceOp)
{
    if (mapU32ToDataType.find(dataType) == mapU32ToDataType.end()) {
        HCCL_ERROR("[AicpuTsThread][%s] type[%u] is not supported.", __func__, dataType);
        return HCCL_E_PARA;
    }
    if (mapU32ToReduceOp.find(reduceOp) == mapU32ToReduceOp.end()) {
        HCCL_ERROR("[AicpuTsThread][%s] op[%u] is not supported.", __func__, reduceOp);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

} // namespace

// Local Data Plane Functions
HcclResult AicpuTsThread::ThreadNotifyRecordCrossType(const NotifyEntity notifyEntity) const
{
    if (notifyEntity.type != NOTIFY_TYPE_HOST_MEM) {
        HCCL_ERROR("[AicpuTsThread::%s] non-HOST_MEM notify is NOT supported.", __func__);
        return HCCL_E_PARA;
    }
    CHK_PTR_NULL(streamA5_);
    Hccl::RtsqBase *const rtsqPtr = streamA5_->GetRtsq();
    CHK_PTR_NULL(rtsqPtr);
    
    const uint64_t notifyDeviceVA = notifyEntity.u.deviceVA;
    TRY_CATCH_RETURN(rtsqPtr->WriteValue(notifyDeviceVA, 1));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::LocalNotifyWait(uint32_t notifyId) const
{
    CHK_PTR_NULL(streamA5_);
    HCCL_INFO("[AicpuTsThread::%s] streamId[%u], notifyId[%u]", __func__, streamA5_->GetId(), notifyId);
    Hccl::RtsqBase *const rtsqPtr = streamA5_->GetRtsq();
    CHK_PTR_NULL(rtsqPtr);
    TRY_CATCH_RETURN(rtsqPtr->NotifyWait(notifyId));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::LocalNotifyRecord(uint32_t notifyId) const
{
    CHK_PTR_NULL(streamA5_);
    HCCL_INFO("[AicpuTsThread::%s] streamId[%u], notifyId[%u]", __func__, streamA5_->GetId(), notifyId);
    Hccl::RtsqBase *const rtsqPtr = streamA5_->GetRtsq();
    CHK_PTR_NULL(rtsqPtr);
    TRY_CATCH_RETURN(rtsqPtr->NotifyRecordLoc(notifyId));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::LocalCopy(void *dst, const void *src, uint64_t sizeByte) const
{
    if (sizeByte > std::numeric_limits<uint32_t>::max()) {
        HCCL_ERROR("[AicpuTsThread::%s] sizeByte[%llu] exceeds the maximum value of uint32", __func__, sizeByte);
        return HCCL_E_PARA;
    }

    CHK_PTR_NULL(streamA5_);
    Hccl::RtsqBase *const rtsqPtr = streamA5_->GetRtsq();
    CHK_PTR_NULL(rtsqPtr);

    // No need to check nullptr for dst & src
    const uint64_t dstAddr = reinterpret_cast<uint64_t>(dst);
    const uint64_t srcAddr = reinterpret_cast<uint64_t>(src);
    const uint32_t sizeByteU32 = static_cast<uint32_t>(sizeByte);
    const uint32_t partId = 0;  // partId will not be used.

    HCCL_INFO("[AicpuTsThread::%s] streamId[%u], dst[0x%llx], src[0x%llx], sizeByte[%u]",
        __func__, streamA5_->GetId(), dst, src, sizeByteU32);

    TRY_CATCH_RETURN(rtsqPtr->SdmaCopy(dstAddr, srcAddr, sizeByteU32, partId));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::LocalReduce(
    void *dst, const void *src, uint64_t sizeByte, HcommDataType dataType, HcommReduceOp reduceOp) const
{
    if (sizeByte > std::numeric_limits<uint32_t>::max()) {
        HCCL_ERROR("[AicpuTsThread::%s] sizeByte[%llu] exceeds the maximum value of uint32", __func__, sizeByte);
        return HCCL_E_PARA;
    }

    CHK_PTR_NULL(streamA5_);
    Hccl::RtsqBase *const rtsqPtr = streamA5_->GetRtsq();
    CHK_PTR_NULL(rtsqPtr);

    // No need to check nullptr for dst & src
    const uint64_t dstAddr = reinterpret_cast<uint64_t>(dst);
    const uint64_t srcAddr = reinterpret_cast<uint64_t>(src);
    const uint32_t sizeByteU32 = static_cast<uint32_t>(sizeByte);
    const uint32_t partId = 0;  // partId will not be used.
    CHK_RET(CheckDataTypeAndReduceOp(dataType, reduceOp));
    const Hccl::DataType dataTypeA5 = mapU32ToDataType[dataType];
    const Hccl::ReduceOp reduceOpA5 = mapU32ToReduceOp[reduceOp];
    const Hccl::ReduceIn reduceIn{dataTypeA5, reduceOpA5};

    HCCL_INFO("[AicpuTsThread::%s] streamId[%u], dst[0x%llx], src[0x%llx], sizeByte[%u], dataType[%d], reduceOp[%d]", 
        __func__, streamA5_->GetId(), dst, src, sizeByteU32, dataTypeA5, reduceOpA5);

    TRY_CATCH_RETURN(rtsqPtr->SdmaReduce(dstAddr, srcAddr, sizeByteU32, partId, reduceIn));
    return HCCL_SUCCESS;
}

// Private functions
HcclResult AicpuTsThread::HostInit()
{
    CHK_PRT_RET(!uniqueIdStr_.empty(),
        HCCL_ERROR("[AicpuTsThread][Init]not support init with uniqueId on host"),
        HCCL_E_NOT_SUPPORT);
    s32 deviceLogicId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(deviceLogicId), devId_));
    CHK_RET(hrtGetDeviceType(devType_));
    if (rtStream_ == nullptr) {
        stream_.reset(new (std::nothrow) Stream(streamType_));
        CHK_SMART_PTR_NULL(stream_);
        rtStream_ = stream_->ptr();
    }
    notifys_.reserve(notifyNum_);
    for (uint32_t idx = 0; idx < notifyNum_; idx++) {
        notifys_.emplace_back(nullptr);
        notifys_[idx].reset(new (std::nothrow) LocalNotify());
        CHK_SMART_PTR_NULL(notifys_[idx]);
        CHK_RET(notifys_[idx]->Init(notifyLoadType_));
        if (devType_ != DevType::DEV_TYPE_910_95) {
            CHK_RET(notifys_[idx]->SetIpc());
        }
    }

    if (streamType_ == StreamType::STREAM_TYPE_DEVICE && devType_ != DevType::DEV_TYPE_910_95) {
        uint64_t size = sizeof(SqCqeContext);
        sqCqeContext_ = DeviceMem::alloc(size);
        CHK_PTR_NULL(sqCqeContext_.ptr());
        CHK_RET(hrtMemSet(sqCqeContext_.ptr(), size, size));
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::DeviceInit()
{
    CHK_PRT_RET(uniqueIdStr_.empty(), HCCL_ERROR("[AicpuTsThread][Init]uniqueIdStr is empty"), HCCL_E_INTERNAL);
    std::istringstream iss(uniqueIdStr_);
    CHK_RET(hrtGetDeviceType(devType_));
    uint32_t hostPhyId = 0;
    iss.read(reinterpret_cast<char_t *>(&streamType_), sizeof(streamType_));
    iss.read(reinterpret_cast<char_t *>(&notifyLoadType_), sizeof(notifyLoadType_));
    HCCL_INFO("[AicpuTsThread][Init]streamType[%d], notifyLoadType[%d].", streamType_, notifyLoadType_);
    iss.read(reinterpret_cast<char_t *>(&hostPhyId), sizeof(hostPhyId));
    CHK_RET(hrtDrvGetLocalDevIDByHostDevID(hostPhyId, &devId_));
    iss.read(reinterpret_cast<char_t *>(&notifyNum_), sizeof(notifyNum_));

    HcclStreamParam streamParam;
    iss.read(reinterpret_cast<char_t *>(&streamParam), sizeof(streamParam));
    // 91095初始化streamlite，初始化rtsq接口
    HCCL_INFO("AicpuTsThread::DeviceInit InitStreams start");
    if (devType_ == DevType::DEV_TYPE_910_95) {
        HCCL_INFO("AicpuTsThread::DeviceInit InitStreamLite start");
        CHK_RET(InitStreamLite(streamParam.streamInfo, hostPhyId));
    } else {
        HCCL_INFO("AicpuTsThread::DeviceInit InitStream start");
        CHK_RET(InitStream(streamParam));
    }
    HCCL_INFO("AicpuTsThread::DeviceInit InitStreams end");

    notifys_.reserve(notifyNum_);

    for (uint32_t idx = 0; idx < notifyNum_; idx++) {
        notifys_.emplace_back(nullptr);
        HcclSignalInfo notifyInfo;
        iss.read(reinterpret_cast<char_t *>(&notifyInfo), sizeof(notifyInfo));
        notifys_[idx].reset(new (std::nothrow) LocalNotify());
        CHK_SMART_PTR_NULL(notifys_[idx]);
        if (devType_ == DevType::DEV_TYPE_910_95) {
            CHK_RET(notifys_[idx]->InitNotifyLite(notifyInfo));
            HCCL_INFO("[AicpuTsThread][Init]local notifyLite init success, resId[%u], devId[%u]",
                notifyInfo.resId, notifyInfo.devId);
        } else {
            CHK_RET(notifys_[idx]->Init(notifyInfo, notifyLoadType_));
            HCCL_INFO("[AicpuTsThread][Init]local notifyLite init success, resId[%u], tsId:%d, devId[%u]",
                notifyInfo.resId,
                notifyInfo.tsId,
                notifyInfo.devId);
        }
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::GetSqHeadAndTail(uint32_t& sqHead, uint32_t& sqTail)
{
#ifdef CCL_KERNEL_AICPU
    CHK_PTR_NULL(streamA5_);

    const uint32_t sqIds = streamA5_->GetSqId();

    HCCL_INFO("[AicpuTsThread::%s] START. devId=%u, sqId=%u.", __func__, devId_, sqIds);

    CHK_RET(QuerySqStatusByType(devId_, sqIds, DRV_SQCQ_PROP_SQ_TAIL, sqTail));
    CHK_RET(QuerySqStatusByType(devId_, sqIds, DRV_SQCQ_PROP_SQ_HEAD, sqHead));

    HCCL_INFO("[AicpuTsThread::%s] SUCCESS. sqHead=%u, sqTail=%u.", __func__, sqHead, sqTail);
#endif
    return HCCL_SUCCESS;
}

}  // namespace hccl
