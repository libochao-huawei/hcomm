/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "acl/acl_base.h"
#include "acl/acl_rt.h"
#include "sim_log.h"
#include "db_sim_runner_ops.h"

#define EXC_STUB_TRACE(format, ...)  HCCL_VM_TRACE("[EXCEPTION_STUB]" format, ##__VA_ARGS__)
#define EXC_STUB_DEBUG(format, ...)  HCCL_VM_DEBUG("[EXCEPTION_STUB]" format, ##__VA_ARGS__)
#define EXC_STUB_INFO(format, ...)   HCCL_VM_INFO("[EXCEPTION_STUB]" format, ##__VA_ARGS__)
#define EXC_STUB_WARN(format, ...)   HCCL_VM_WARN("[EXCEPTION_STUB]" format, ##__VA_ARGS__)
#define EXC_STUB_ERROR(format, ...)  HCCL_VM_ERROR("[EXCEPTION_STUB]" format, ##__VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

aclError aclrtSetExceptionInfoCallback(aclrtExceptionInfoCallback callback)
{
    (void) callback;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

uint32_t aclrtGetTaskIdFromExceptionInfo(const aclrtExceptionInfo *info)
{
    (void) info;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

uint32_t aclrtGetStreamIdFromExceptionInfo(const aclrtExceptionInfo *info)
{
    (void) info;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

uint32_t aclrtGetThreadIdFromExceptionInfo(const aclrtExceptionInfo *info)
{
    (void) info;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

uint32_t aclrtGetDeviceIdFromExceptionInfo(const aclrtExceptionInfo *info)
{
    (void) info;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

uint32_t aclrtGetErrorCodeFromExceptionInfo(const aclrtExceptionInfo *info)
{
    (void) info;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclrtPeekAtLastError(aclrtLastErrLevel level)
{
    (void) level;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclrtGetLastError(aclrtLastErrLevel level)
{
    (void) level;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclrtGetMemUceInfo(int32_t deviceId, aclrtMemUceInfo *memUceInfoArray, size_t arraySize, size_t *retSize)
{
    (void) deviceId;
    (void) memUceInfoArray;
    (void) arraySize;
    (void) retSize;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclrtMemUceRepair(int32_t deviceId, aclrtMemUceInfo *memUceInfoArray, size_t arraySize)
{
    (void) deviceId;
    (void) memUceInfoArray;
    (void) arraySize;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclrtDeviceTaskAbort(int32_t deviceId, uint32_t timeout)
{
    (void) deviceId;
    (void) timeout;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclRecoverAllHcclTasks(int32_t deviceId)
{
    (void) deviceId;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclrtGetErrorVerbose(int32_t deviceId, aclrtErrorInfo *errorInfo)
{
    (void) deviceId;
    (void) errorInfo;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}

aclError aclrtRepairError(int32_t deviceId, const aclrtErrorInfo *errorInfo)
{
    (void) deviceId;
    (void) errorInfo;
    EXC_STUB_TRACE("not supported");
    return ACL_SUCCESS;
}
#ifdef __cplusplus
}
#endif  // __cplusplus
