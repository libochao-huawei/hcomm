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
#define SVM_REGISTER_FLAG_WITH_ACCESS_VA (1ULL << 0ULL)
#define SVM_MEM_READ (0x1 << 0)
#define SVM_MEM_WRITE (0x1 << 1)
constexpr u32 WAIT_TIMEOUT_MS = 3 * 1000; // 定义最大等待3秒
extern drvError_t __attribute__((weak)) halSvmRegister(uint32_t dev_id, uint64_t va, uint64_t size, uint64_t flag, uint64_t *access_va);
extern drvError_t __attribute__((weak)) halSvmAccess(uint32_t dev_id, uint64_t dst, uint64_t src, uint64_t size, uint64_t flag);
extern drvError_t __attribute__((weak)) halSvmUnregister(uint32_t dev_id, uint64_t va, uint64_t size, uint64_t flag);

namespace hccl {
class MemNotify {
public:
    ~MemNotify() {
        if (!isInit_) {
            return;
        }
        s32 devId = 0;
        hrtGetDevice(&devId);
        int64_t connectType = 0;
        hrtHalGetDeviceInfo(devId, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, &connectType); // ?
        if (connectType == 1) { // PCIe
            // 释放notify
            if (notifyDeviceVa_) {
                aclrtFree(&notifyDeviceVa_); // device
            }
            // 对齐
            uint64_t va = reinterpret_cast<uint64_t>(notifyDeviceVa_);
            uint64_t registerVa = (va + (4096ULL - 1ULL)) & ~(4096ULL - 1ULL);
            int32_t ret = halSvmUnregister(devId, registerVa, sizeof(uint8_t), SVM_REGISTER_FLAG_WITH_ACCESS_VA);
            if (ret != 0) {
                HCCL_ERROR("halSvmUnregister failed, ret = %d", ret);
            }
        } else {
            if (notifyHostVa_) {
                free(notifyHostVa_);
                aclError aclRet = aclrtHostUnregister(notifyHostVa_);
                if (aclRet != ACL_SUCCESS) {
                    HCCL_ERROR("aclrtHostUnregister failed, ret = %d", aclRet);
                }
            }
        }
    };
    HcclResult Record() {
        return HCCL_SUCCESS;
    };
    HcclResult Wait() {
        // 等待notify被写入（flag从0变为非0）
        s32 devId = 0;
        CHK_RET(hrtGetDevice(&devId));
        int64_t connectType = 0;
        hrtHalGetDeviceInfo(devId, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, &connectType);
        auto timeout = std::chrono::milliseconds(WAIT_TIMEOUT_MS);
        auto startTime = std::chrono::steady_clock::now();
        if (connectType == 1) { // PCIe
            // 轮询notifyDeviceVa_地址，等待被写入
            while (true) {
                uint8_t flag;
                // halSvmAccess(devId, reinterpret_cast<uint64_t>(notifyHostVa_), reinterpret_cast<uint64_t>(notifyDeviceVa_), sizeof(uint8_t), SVM_MEM_READ);
                aclError err = aclrtMemcpy(&flag, sizeof(uint8_t), notifyDeviceVa_, sizeof(uint8_t), ACL_MEMCPY_DEVICE_TO_HOST);
                if (err != ACL_ERROR_NONE) {
                    return HCCL_E_RUNTIME;
                }
                if (flag != 0) {
                    // reset flag
                    uint8_t resetFlag = 0;
                    err = aclrtMemcpy(notifyDeviceVa_, sizeof(uint8_t), &resetFlag, sizeof(uint8_t), ACL_MEMCPY_HOST_TO_DEVICE);
                    if (err != ACL_ERROR_NONE) {
                        return HCCL_E_RUNTIME;
                    }
                    break; // notify被写入
                }

                if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                    HCCL_ERROR("[HostCpuRoceChannel][%s] call ibv_poll_cq timeout.", __func__);
                    return HCCL_E_TIMEOUT;
                }
            }
        } else {
            // 轮询notifyHostVa_地址，等待被写入
            while (true) {
                uint8_t flag = *reinterpret_cast<uint8_t*>(notifyHostVa_);
                if (flag != 0) {
                    // reset flag
                    *reinterpret_cast<uint8_t*>(notifyHostVa_) = 0;
                    break; // notify被写入
                }

                if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                    HCCL_ERROR("[HostCpuRoceChannel][%s] call ibv_poll_cq timeout.", __func__);
                    return HCCL_E_TIMEOUT;
                }
            }
        }
        return HCCL_SUCCESS;
    };
    HcclResult Alloc() {
        s32 devId = 0;
        CHK_RET(hrtGetDevice(&devId));
        int64_t connectType = 0;
        hrtHalGetDeviceInfo(devId, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, &connectType); // ?
        if (connectType == 1) { // PCIe
            // 申请notify
            aclrtMalloc(&notifyDeviceVa_, sizeof(uint8_t) + 4096ULL, ACL_MEM_MALLOC_HUGE_ONLY); // device// 对齐
            uint64_t va = reinterpret_cast<uint64_t>(notifyDeviceVa_);
            uint64_t registerVa = (va + (4096ULL - 1ULL)) & ~(4096ULL - 1ULL);
            uint64_t accessVa;
            int32_t ret = halSvmRegister(devId, registerVa, sizeof(uint8_t), SVM_REGISTER_FLAG_WITH_ACCESS_VA, &accessVa);
            if (ret != 0) {
                HCCL_ERROR("halSvmRegister failed, ret = %d", ret);
                aclrtFree(&notifyDeviceVa_);
                return HCCL_E_INTERNAL;
            }
            notifyHostVa_ = reinterpret_cast<void*>(accessVa);
        } else {
            // 申请host内存
            notifyHostVa_ = malloc(sizeof(uint8_t));
            if (!notifyHostVa_) {
                HCCL_ERROR("malloc for notifyHostVa_ failed");
                return HCCL_E_RUNTIME;
            }
            aclError aclRet = aclrtHostRegister(notifyHostVa_, sizeof(uint8_t), ACL_HOST_REGISTER_MAPPED, &notifyDeviceVa_);
            if (aclRet != ACL_SUCCESS) {
                HCCL_ERROR("aclrtHostRegister failed, ret = %d", aclRet);
                free(notifyHostVa_);
                notifyHostVa_ = nullptr;
                return HCCL_E_INTERNAL;
            }
        }
        isInit_ = true;
        return HCCL_SUCCESS;
    };
    uint64_t GetIdentifier() {
        return reinterpret_cast<uint64_t>(notifyDeviceVa_);
    };
private:
    bool isInit_ = false;
    void* notifyHostVa_{};
    void* notifyDeviceVa_{};
};

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

    // Local Data Plane Functions
    HcclResult LocalNotifyRecord(uint32_t notifyId) const override {
        return HCCL_E_NOT_SUPPORT;
    };
    HcclResult LocalNotifyWait(uint32_t notifyId) const override {
        return HCCL_E_NOT_SUPPORT;
    };
    HcclResult LocalCopy(void *dst, const void *src, uint64_t sizeByte) const override {
        return HCCL_E_NOT_SUPPORT;
    };
    HcclResult LocalReduce(
        void *dst, const void *src, uint64_t sizeByte, HcommDataType dataType, HcommReduceOp reduceOp) const override {
        return HCCL_E_NOT_SUPPORT;
    };

    ~CpuThread();

    HcclResult ServiceRegister(ThreadService serviceCb, ThreadServiceHandle* serviceHandle);
    HcclResult ServiceUnregister(ThreadServiceHandle service);
    HcclResult KernelRun();
    HcclResult GetThreadEntity(void* &threadEntity) override;
    MemNotify* GetMemNotify(uint32_t notifyIndex);
    uint32_t GetNotifyNum() const override;

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
    bool isInit_{false};
    void* deviceHandle_;
};

}  // namespace hccl

#endif  // HCCL_CPU_THREAD_H
