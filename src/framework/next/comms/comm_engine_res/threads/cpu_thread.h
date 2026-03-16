/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_CPU_THREAD_H
#define HCCL_CPU_THREAD_H
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include "thread.h"
#include "acl/acl_rt.h"
#include "ascend_hal.h"
#include "utils/service_scheduler.h"
#include "notifys/mem_notify.h"
namespace hccl {
class CpuThread : public Thread {
public:
    CpuThread(aclrtStream rtStream, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
        : dpuStream_(rtStream), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType) {};
    CpuThread(StreamType streamType, uint32_t notifyNum, const NotifyLoadType notifyLoadType)
        : streamType_(streamType), notifyNum_(notifyNum), notifyLoadType_(notifyLoadType) {};

    HcclResult Init() override;
    HcclResult DeInit() override;

    std::string &GetUniqueId() override {
        static std::string uniqueId = "CpuThread";
        return uniqueId;
    };
    LocalNotify *GetNotify(uint32_t index) const override {
        return nullptr;
    };

    // A3 Stream & A5 Stream
    bool IsDeviceA5() const {
        return true;
    };
    Stream *GetStream() const {
        return nullptr;
    };
    void *GetStreamLitePtr() const override {
        return nullptr;
    };
    void LaunchTask() const override {
        // Do nothing
    };

    ~CpuThread();

    HcclResult ServiceRegister(ThreadService serviceCb, ThreadServiceHandle* serviceHandle);
    HcclResult ServiceUnregister(ThreadServiceHandle service);
    HcclResult KernelRun();
    HcclResult GetThreadEntity(void* &threadEntity) override;
    MemNotify* GetMemNotify(uint32_t notifyIndex);
    uint32_t GetNotifyNum() const override;
    HcclResult SupplementNotify(uint32_t notifyNum) override {
        (void)notifyNum;
        return HCCL_E_NOT_SUPPORT;
    };
    bool GetMaster() const override {
        return isMaster_;
    };
    void SetIsMaster(bool isMaster) override {
        isMaster_ = isMaster;
    };

private:
    HcclResult PrepareDpuKernelResource(aclrtFuncHandle &funcHandle);
    HcclResult LaunchDpuKernel(aclrtFuncHandle &funcHandle);
    HcclResult DestroyDpuKernelResource();

    StreamType streamType_ = StreamType::STREAM_TYPE_RESERVED;
    aclrtStream dpuStream_{};
    uint32_t notifyNum_ = 0;
    NotifyLoadType notifyLoadType_ = NotifyLoadType::HOST_NOTIFY;
    
    aclrtContext dpuContext_;
    aclrtContext npuContext_;
    std::unique_ptr<ServiceScheduler> serviceScheduler_{};
    std::vector<std::unique_ptr<MemNotify>> notifys_;
    ThreadServiceHandle recordServiceHandle_;
    ThreadServiceHandle waitServiceHandle_;
    bool isMaster_{false};
    bool isInit_{false};
    void* deviceHandle_;
};

}  // namespace hccl

#endif  // HCCL_CPU_THREAD_H
