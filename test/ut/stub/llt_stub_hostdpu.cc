/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>
#include "hccl_types.h"

HcclResult RecordService(void *args, uint64_t argsSizeByte)
{
    return HCCL_SUCCESS;
}

HcclResult WaitService(void *args, uint64_t argsSizeByte)
{
    return HCCL_SUCCESS;
}

// HrtMemcpy: declaration uses rtMemcpyKind_t (enum tagRtMemcpyKind) inside Hccl namespace
namespace Hccl {

typedef enum tagRtMemcpyKind {
    RT_MEMCPY_HOST_TO_HOST_STUB = 0,
    RT_MEMCPY_HOST_TO_DEVICE_STUB,
    RT_MEMCPY_DEVICE_TO_HOST_STUB,
    RT_MEMCPY_DEVICE_TO_DEVICE_STUB,
} rtMemcpyKind_t;

void HrtMemcpy(void *dst, uint64_t destMax, const void *src, uint64_t count, rtMemcpyKind_t kind)
{
    return;
}

} // namespace Hccl

// rtSetXpuDevice / rtResetXpuDevice: declared extern "C" in orion_adapter_rts.h
typedef enum {
    RT_DEV_TYPE_DPU_STUB = 0,
    RT_DEV_TYPE_REV_STUB
} rtXpuDevType_stub;

extern "C" {

int32_t rtResetXpuDevice(rtXpuDevType_stub devType, const uint32_t devId)
{
    return 0;
}

int32_t rtSetXpuDevice(rtXpuDevType_stub devType, const uint32_t devId)
{
    return 0;
}

}
