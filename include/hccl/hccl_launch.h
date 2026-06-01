/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_LAUNCH_H_
#define HCCL_LAUNCH_H_

#include <stdint.h>
#include <hccl/hccl_types.h>
#include <acl/acl_rt.h>

#ifndef THREAD_HANDLE_DEFINED
#define THREAD_HANDLE_DEFINED
typedef uint64_t ThreadHandle;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Launch AICPU kernel for P2P task
 *
 * @param comm Communication domain handle
 * @param opInfo Operator descriptor parameters
 * @param funcInfo Kernel function info (dynamic library name and kernel function name)
 * @param args Kernel function input parameters
 * @param argSize Kernel function input parameter size in bytes
 * @param aicpuThreadHandle AICPU communication main stream thread handle
 * @param userStream User host main stream
 * @return HcclResult
 *   HCCL_SUCCESS: success
 *   HCCL_E_NOT_SUPPORT: HcclGroupStart not called
 *   HCCL_E_INTERNAL: internal error (async task failure, communication task failure)
 */
extern HcclResult HcclAicpuKernelLaunch(HcclComm comm, HcclOpDesc opInfo, HcclKernelFuncInfo funcInfo,
    void *args, uint32_t argSize, ThreadHandle aicpuThreadHandle, aclrtStream userStream);

#ifdef __cplusplus
}
#endif

#endif