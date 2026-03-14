/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_data_api_impl.h"

#include "hcom_common.h"

#include "ccu_kernel_mgr_.h"

CcuResult CcuVariableCreateImpl(CcuVariableHandle *resVar)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = CcuKernelMgr_::GetInstance(devLogicId).GetCurrentKernel();
    CHK_PTR_NULL(kernel);
    CHK_CCU_RET(kernel->VariableCreate(resVar));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableAssignImpl(CcuVariableHandle resVar, uint64_t immediate)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = CcuKernelMgr_::GetInstance(devLogicId).GetCurrentKernel();
    CHK_PTR_NULL(kernel);
    CHK_CCU_RET(kernel->VariableAssign(resVar, immediate));
    
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableAddVarToVarImpl(CcuVariableHandle resVar, CcuVariableHandle varA, CcuVariableHandle varB)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = CcuKernelMgr_::GetInstance(devLogicId).GetCurrentKernel();
    CHK_PTR_NULL(kernel);
    CHK_CCU_RET(kernel->VariableAddVarToVar(resVar, varA, varB));
    
    return CcuResult::CCU_SUCCESS;
}

