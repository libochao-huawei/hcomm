/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_MEM_NOTIFY_H
#define HCCL_MEM_NOTIFY_H
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include "thread.h"
#include "acl/acl_rt.h"
#include "ascend_hal.h"
#include "utils/service_scheduler.h"
#define SVM_REGISTER_FLAG_WITH_ACCESS_VA (1ULL << 0ULL)
#define SVM_MEM_READ (0x1 << 0)
#define SVM_MEM_WRITE (0x1 << 1)
constexpr u32 WAIT_TIMEOUT_MS = 30 * 1000; // 定义最大等待30秒
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
        if (connectType == HOST_DEVICE_CONNECT_TYPE_PCIE) { // PCIe
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
        if (connectType == HOST_DEVICE_CONNECT_TYPE_PCIE) { // PCIe
            // 轮询notifyDeviceVa_地址，等待被写入
            while (true) {
                uint8_t flag;
                // halSvmAccess(devId, reinterpret_cast<uint64_t>(notifyHostVa_), reinterpret_cast<uint64_t>(&flag), sizeof(uint8_t), SVM_MEM_READ);
                // flag = *(reinterpret_cast<uint8_t*>(notifyHostVa_));
                aclError err = aclrtMemcpy(&flag, sizeof(uint8_t), notifyDeviceVa_, sizeof(uint8_t), ACL_MEMCPY_DEVICE_TO_HOST);
                if (err != ACL_ERROR_NONE) {
                    return HCCL_E_RUNTIME;
                }
                if (flag != 0) {
                    // reset flag
                    uint8_t resetFlag = 0;
                    // halSvmAccess(devId, reinterpret_cast<uint64_t>(notifyDeviceVa_), reinterpret_cast<uint64_t>(&resetFlag), sizeof(uint8_t), SVM_MEM_WRITE);
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
        if (connectType == HOST_DEVICE_CONNECT_TYPE_PCIE) { // PCIe
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
} // namespace hccl
#endif // HCCL_MEM_NOTIFY_H