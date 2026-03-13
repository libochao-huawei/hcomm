/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cpu_ts_thread.h"

namespace hccl {

CpuTsThread::CpuTsThread(rtStream_t rtStream, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
    : rtStream_(rtStream), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType)
{}

CpuTsThread::CpuTsThread(StreamType streamType, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
    : streamType_(streamType), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType)
{}

CpuTsThread::~CpuTsThread()
{
    DeInit();
}

HcclResult CpuTsThread::Init()
{
    // Host 侧初始化
    CHK_RET(GetRunSideIsDevice(isDeviceSide_));
    CHK_RET(hrtGetDeviceType(devType_));
    if (!isDeviceSide_) {
        s32 deviceLogicId;
        CHK_RET(hrtGetDevice(&deviceLogicId));
        CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(deviceLogicId), devId_));
        if (streamType_ == StreamType::STREAM_TYPE_DEVICE || notifyLoadType_ == NotifyLoadType::DEVICE_NOTIFY) {
            return HCCL_E_NOT_SUPPORT;
        }
        if (rtStream_ == nullptr) {
            stream_.reset(new (std::nothrow) Stream(streamType_));
            CHK_SMART_PTR_NULL(stream_);
            rtStream_ = stream_->ptr();
        } else {
            stream_.reset(new (std::nothrow) Stream(rtStream_));
            CHK_SMART_PTR_NULL(stream_);
        }
        notifys_.reserve(notifyNum_);
        for (uint32_t idx = 0; idx < notifyNum_; idx++) {
            notifys_.emplace_back(nullptr);
            notifys_[idx].reset(new (std::nothrow) LocalNotify());
            CHK_SMART_PTR_NULL(notifys_[idx]);
            CHK_RET(notifys_[idx]->Init(notifyLoadType_));
            if (devType_ != DevType::DEV_TYPE_950) {
                CHK_RET(notifys_[idx]->SetIpc());
            }
        }
        return HCCL_SUCCESS;
    } else {
        return HCCL_E_NOT_SUPPORT;
    }
}

HcclResult CpuTsThread::DeInit()
{
    streamType_ = StreamType::STREAM_TYPE_RESERVED;
    notifyNum_ = 0;
    stream_ = nullptr;
    notifys_.clear();
    return HCCL_SUCCESS;
}

std::string &CpuTsThread::GetUniqueId()
{
    if (!uniqueIdStr_.empty()) {
        return uniqueIdStr_;
    }

    // 序列化信息
    uniqueIdStr_ = std::string();
    std::ostringstream oss;
    StreamType streamType = StreamType::STREAM_TYPE_DEVICE;
    oss.write(reinterpret_cast<const char_t *>(&streamType), sizeof(streamType));
    oss.write(reinterpret_cast<const char_t *>(&notifyLoadType_), sizeof(notifyLoadType_));
    oss.write(reinterpret_cast<const char_t *>(&devId_), sizeof(devId_));
    oss.write(reinterpret_cast<const char_t *>(&notifyNum_), sizeof(notifyNum_));

    // 临时申请一条流，用于在device侧资源展开时initStream
    streamDevice_.reset(new (std::nothrow) Stream(streamType));
    if (streamDevice_ == nullptr) {
        HCCL_ERROR("[CpuTsThread][%s]reset stream failed, stream type[%d]",__func__, streamType);
        return uniqueIdStr_;
    }

    uint64_t size = sizeof(SqCqeContext);
    sqCqeContext_ = DeviceMem::alloc(size);
    if (sqCqeContext_.ptr() == nullptr) {
        HCCL_ERROR("[CpuTsThread][%s]alloc mem failed, mem size[%llu]",__func__, size);
        return uniqueIdStr_;
    }
    HcclResult ret = hrtMemSet(sqCqeContext_.ptr(), size, size);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[CpuTsThread][%s]mem set failed, mem size[%llu], ptr[%p]",__func__, size, sqCqeContext_.ptr());
        return uniqueIdStr_;
    }

    HcclStreamParam streamParam;
    streamParam.streamInfo.streamIds = streamDevice_->id();
    streamParam.streamInfo.sqIds = streamDevice_->sqId();
    streamParam.streamInfo.cqIds = streamDevice_->cqId();
    streamParam.streamInfo.logicCqids = streamDevice_->logicCqId();
    streamParam.sqCqContextAddr = reinterpret_cast<uint64_t>(sqCqeContext_.ptr());
    streamParam.sqCqContextSize = sqCqeContext_.size();
    oss.write(reinterpret_cast<const char_t *>(&streamParam), sizeof(streamParam));

    ret = HCCL_SUCCESS;
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

uint32_t CpuTsThread::GetNotifyNum() const
{
    return notifyNum_;
}

LocalNotify *CpuTsThread::GetNotify(uint32_t index) const
{
    if (index >= notifyNum_) {
        HCCL_ERROR(
            "[CpuTsThread][GetNotify] notifyNum[%u], index[%u] out of range[0, %u]", notifyNum_, index, notifyNum_ - 1);
        return nullptr;
    }
    return notifys_[index].get();
}

bool CpuTsThread::IsDeviceA5() const
{
    return devType_ == DevType::DEV_TYPE_950;
}

// A3 Stream
Stream *CpuTsThread::GetStream() const
{
    return stream_.get();
}

// A5 Stream
void *CpuTsThread::GetStreamLitePtr() const
{
    return nullptr;  // Not implemented
}

void CpuTsThread::LaunchTask() const
{
    return;
}

// Local Data Plane Functions
HcclResult CpuTsThread::LocalNotifyRecord(uint32_t notifyId) const
{
    return HCCL_E_NOT_SUPPORT;
}
HcclResult CpuTsThread::LocalNotifyWait(uint32_t notifyId) const
{
    return HCCL_E_NOT_SUPPORT;
}
HcclResult CpuTsThread::LocalCopy(void *dst, const void *src, uint64_t sizeByte) const
{
    return HCCL_E_NOT_SUPPORT;
}
HcclResult CpuTsThread::LocalReduce(
    void *dst, const void *src, uint64_t sizeByte, HcommDataType dataType, HcommReduceOp reduceOp) const
{
    return HCCL_E_NOT_SUPPORT;
}

}  // namespace hccl
