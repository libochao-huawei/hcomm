/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "orion_adapter_hal.h"
#include "log.h"

namespace Hccl
{
HcclResult HrtHalHostRegister(void *srcPtr, uint64_t size, uint32_t flag, uint32_t devid, void **dstPtr)
{
    return HCCL_E_NOT_SUPPORT;
}

HcclResult HrtHalHostUnregister(void *ptr, uint32_t devid)
{
    return HCCL_E_NOT_SUPPORT;
}

}