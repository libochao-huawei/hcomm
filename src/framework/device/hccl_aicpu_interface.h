/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __MC2_AICPU_INTERFACE_H__
#define __MC2_AICPU_INTERFACE_H__

#include <cstdint>
#include "hcomm_res_defs.h"
#include "hccl_group_utils.h"

struct ThreadNotifyRecordParam {
    ThreadHandle thread;
    ThreadHandle dstThread;
    uint32_t dstNotifyIdx;
};
struct ThreadNotifyWaitParam {
    ThreadHandle thread;
    uint32_t notifyIdx;
    uint32_t timeOut;
};

struct P2pAicpuKernelParam {
    ThreadNotifyWaitParam waitParam;    // NotifyWait 参数
    hccl::HcclKernelFuncInfo funcInfo;         // dlopen/func 参数
    ThreadNotifyRecordParam recordParam; // NotifyRecord 参数
    void* funcArgs;                      // func执行的参数(OpParam*)
    ThreadHandle sendRecvStream;         // P2P stream参数
};

struct P2pGroupAicpuKernelParam {
    hccl::HcclKernelFuncInfo funcInfo;         // dlopen/func 参数
    void* funcArgs;                      // func执行的参数(OpParam*)
    ThreadHandle sendRecvStream;         // P2P stream参数
};

extern "C" {
__attribute__((visibility("default"))) uint32_t RunAicpuKfcResInitV2(void *args);
__attribute__((visibility("default"))) uint32_t RunAicpuRpcSrvLaunchV2(void *args);
__attribute__((visibility("default"))) uint32_t RunAicpuNotifyRecord(void *args);
__attribute__((visibility("default"))) uint32_t RunAicpuNotifyWait(void *args);
__attribute__((visibility("default"))) uint32_t HcclP2pLaunchNonGroupSynAicpuKernel(void *args);
__attribute__((visibility("default"))) uint32_t HcclP2pLaunchGroupAicpuKernel(void *args);

}

#endif // __MC2_AICPU_INTERFACE_HPP__