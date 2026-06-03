/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_diag.h"
#include "adapter_rts.h"
#include "dfx/taskException/host/hcclCommTaskException.h"

using namespace hccl;

HcclResult HcclTaskExceptionRegCallBack(HcclTaskExceptionCallback callback)
{
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    if (devType != DevType::DEV_TYPE_950) {
        return HCCL_E_NOT_SUPPORT;
    }
    HCCL_INFO("[%s] START, callback[%p].", __func__, callback);
    s32 devLogicId;
    CHK_RET(hrtGetDevice(&devLogicId));
    hcomm::TaskExceptionHost* handler = hcomm::TaskExceptionHostManager::GetHandler(
        static_cast<size_t>(devLogicId));
    CHK_PTR_NULL(handler);
    handler->SetTaskExceptionCallback(callback);
    HCCL_INFO("[%s] SUCCESS, deviceLogicId[%d].", __func__, devLogicId);
    return HCCL_SUCCESS;
}