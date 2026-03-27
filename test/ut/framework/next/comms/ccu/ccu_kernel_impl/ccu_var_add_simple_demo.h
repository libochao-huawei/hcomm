/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_VAR_ADD_SIMPLE_DEMO_H
#define CCU_VAR_ADD_SIMPLE_DEMO_H

#include "ccu_data_api.h"
#include "ccu_log.h" // demo演示使用，hccl仓需要另外实现

struct CcuVarAddKernelArg {
    uint32_t numA{0};
    uint32_t numB{0};
};

struct CcuVarAddTaskArg {
};

CcuResult CcuVarAddDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);

    // 定义空的资源对象
    CcuVariable result{}, numA{}, numB{};

    // 为资源对象赋资源实例（可以是创建或借用）
    CCU_CHK_RET(CcuVariableCreate(&result));
    CCU_CHK_RET(CcuVariableCreate(&numA));
    CCU_CHK_RET(CcuVariableCreate(&numB));

    // 算法具体实现
    numA = args->numA;
    numB = args->numB;
    result = numA + numB;

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVarAddDemoGenTaskArgs(CcuVarAddTaskArg arg, uint64_t *vars, uint32_t num)
{
    // vars
    return CcuResult::CCU_SUCCESS;
}

#endif // CCU_VAR_ADD_SIMPLE_DEMO_H