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
#include "cpu_thread.h"

namespace hccl {
AicpuTsThread::AicpuTsThread(StreamType streamType, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
    : streamType_(streamType), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType)
{}

AicpuTsThread::AicpuTsThread(const std::string &uniqueIdStr) : uniqueIdStr_(uniqueIdStr)
{}

AicpuTsThread::~AicpuTsThread()
{
}

HcclResult AicpuTsThread::Init()
{
    for (uint32_t i = 0; i < notifyNum_; ++i) {
        notifys_.push_back(std::make_unique<LocalNotify>());
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::DeInit()
{
    notifys_.clear();
    return HCCL_SUCCESS;
}

std::string &AicpuTsThread::GetUniqueId()
{
    return uniqueIdStr_;
}

uint32_t AicpuTsThread::GetNotifyNum() const
{
    return notifys_.size();
}

LocalNotify *AicpuTsThread::GetNotify(uint32_t index) const
{
    return (index < notifys_.size()) ? notifys_[index].get() : nullptr;
}

HcclResult AicpuTsThread::GetThreadEntity(void* &threadEntity)
{
    auto threadEntityPtr = new ThreadEntity{};  // Expect manually delete by caller
    threadEntityPtr->threadObjAddr = reinterpret_cast<uint64_t>(this);
    threadEntity = threadEntityPtr;
    return HCCL_SUCCESS;
}

bool AicpuTsThread::IsDeviceA5() const
{
    return false;
}

Stream *AicpuTsThread::GetStream() const
{
    return nullptr;
}

void *AicpuTsThread::GetStreamLitePtr() const
{
    return nullptr;
}

void AicpuTsThread::LaunchTask() const
{
}

HcclResult AicpuTsThread::LocalNotifyWait(uint32_t notifyId) const
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::LocalNotifyRecord(uint32_t notifyId) const
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::LocalCopy(void *dst, const void *src, uint64_t sizeByte) const
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::LocalReduce(
    void *dst, const void *src, uint64_t sizeByte, HcommDataType dataType, HcommReduceOp reduceOp) const
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::NotifyRecord(const NotifyEntity notifyEntity) const
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsThread::GetSqHeadAndTail(uint32_t& sqHead, uint32_t& sqTail)
{
    return HCCL_SUCCESS;
}

// CpuThread stubs

CpuThread::~CpuThread()
{
}

HcclResult CpuThread::Init()
{
    for (uint32_t i = 0; i < notifyNum_; ++i) {
        notifys_.push_back(std::make_unique<MemNotify>());
    }
    return HCCL_SUCCESS;
}

HcclResult CpuThread::DeInit()
{
    notifys_.clear();
    return HCCL_SUCCESS;
}

HcclResult CpuThread::ServiceRegister(ThreadService serviceCb, ThreadServiceHandle* serviceHandle)
{
    return HCCL_SUCCESS;
}

HcclResult CpuThread::ServiceUnregister(ThreadServiceHandle service)
{
    return HCCL_SUCCESS;
}

HcclResult CpuThread::KernelRun()
{
    return HCCL_SUCCESS;
}

HcclResult CpuThread::GetThreadEntity(void* &threadEntity)
{
    threadEntity = reinterpret_cast<void*>(0x1234); // Just return a dummy pointer for testing
    return HCCL_SUCCESS;
}

MemNotify* CpuThread::GetMemNotify(uint32_t notifyIndex)
{
    return (notifyIndex < notifys_.size()) ? notifys_[notifyIndex].get() : nullptr;
}

uint32_t CpuThread::GetNotifyNum() const
{
    return notifys_.size();
}

}  // namespace hccl