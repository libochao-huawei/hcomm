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
            if (Is310PDevice()) {
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
    HCCL_ERROR("[CpuTsThread][%s] Not supported", __func__);
    static std::string emptyStr = std::string();
    return emptyStr;
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
    return false;  // Not implemented
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
