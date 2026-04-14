/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_data_api.h"
#include "ccu_variable.hpp"

#include "ccu_log.h"
#include "ccu_data_api_impl.h"


CcuResult CcuIfBegin(CcuVariable *var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    CCU_CHK_RET(CcuIfBeginImpl(var->handle, immediate, condType, label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfElse(const char *label)
{
    CCU_CHK_RET(CcuIfElseImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfEnd(const char *label)
{
    CCU_CHK_RET(CcuIfEndImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWhileBegin(CcuVariable *var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    CCU_CHK_RET(CcuWhileBeginImpl(var->handle, immediate, condType, label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWhileEnd(const char *label)
{
    CCU_CHK_RET(CcuWhileEndImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuDoWhileBegin(const char *label)
{
    CCU_CHK_RET(CcuDoWhileBeginImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuDoWhileEnd(CcuVariable *var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    CCU_CHK_RET(CcuDoWhileEndImpl(var->handle, immediate, condType, label));
    return CcuResult::CCU_SUCCESS;
}