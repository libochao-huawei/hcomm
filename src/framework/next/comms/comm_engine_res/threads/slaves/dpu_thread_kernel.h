/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DPU_THREAD_KERNEL_H
#define DPU_THREAD_KERNEL_H

#include <cstdint>
#include <unordered_map>
#include <string>
#include <memory>
#include "cpu_thread.h"

extern "C" {
__attribute__((visibility("default"))) uint32_t RunDpuRpcSrvLaunchNew(const uint64_t args)
{
    struct DpuKernelLaunchParam {
        void*       cpuThread;
        int32_t    deviceId;
    };

    HCCL_INFO("[%s] Launch Dpu Kernel: 0x%lx", __func__, args);
    if (args == 0) {
        HCCL_ERROR("[%s] args is null.", __func__);
        return HCCL_E_PARA;
    }

    // 解析参数信息
    DpuKernelLaunchParam *params = reinterpret_cast<DpuKernelLaunchParam *>(args);

    // 实例化
    CpuThread* cpuThread = static_cast<CpuThread*>(params->cpuThread);

    aclError ret = aclrtSetDevice(params->deviceId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[%s] set device fail. DeviceId: %d.", __func__, params->deviceId);
        return HCCL_E_RUNTIME;
    }

    // Run
    HCCL_INFO("[%s] start to TaskRun", __func__);
    CHK_RET(cpuThread->KernelRun());
    return HCCL_SUCCESS;
}
}

#endif // DPU_THREAD_KERNEL_H