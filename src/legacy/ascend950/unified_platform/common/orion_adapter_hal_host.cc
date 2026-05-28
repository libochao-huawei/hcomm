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
#include "dlhal_function_v2.h"
#include "ascend_hal_error.h"
#include "acl/acl_rt.h"
#include "log.h"

namespace Hccl
{
HcclResult HrtHalHostRegister(void *srcPtr, uint64_t size, uint32_t flag, uint32_t devid, void **dstPtr)
{
    CHK_PTR_NULL(srcPtr);
    CHK_PTR_NULL(dstPtr);
    int32_t logicId = 0;
    aclError aclRet = aclrtGetLogicDevIdByUserDevId(devid, &logicId);
    if (aclRet != ACL_SUCCESS) {
        HCCL_ERROR("[%s]aclrtGetLogicDevIdByUserDevId failed, devid: %d, ret: %d", __func__, devid, aclRet);
        return HCCL_E_RUNTIME;
    }

    drvError_t ret = halHostRegister(srcPtr, size, flag, logicId, dstPtr);
    if (ret != DRV_ERROR_NONE) {
        HCCL_ERROR("halHostRegister failed, ret: %d", ret);
        return HCCL_E_DRV;
    }
    return HCCL_SUCCESS;
}

HcclResult HrtHalHostUnregister(void *ptr, uint32_t devid)
{
    CHK_PTR_NULL(ptr);
    int32_t logicId = 0;
    aclError aclRet = aclrtGetLogicDevIdByUserDevId(devid, &logicId);
    if (aclRet != ACL_SUCCESS) {
        HCCL_ERROR("[%s]aclrtGetLogicDevIdByUserDevId failed, devid: %d, ret: %d", __func__, devid, aclRet);
        return HCCL_E_RUNTIME;
    }

    drvError_t ret = halHostUnregister(ptr, logicId);
    if (ret != DRV_ERROR_NONE) {
        HCCL_ERROR("halHostUnregister failed, ret: %d", ret);
        return HCCL_E_DRV;
    }
    return HCCL_SUCCESS;
}
}