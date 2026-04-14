/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_api.hpp"
#include "ccu_log.h"

// ======================== CCU IF Demo ========================

struct CcuIfDemoKernelArg {
    uint32_t value;
    uint64_t expected;
};

CcuResult CcuIfDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuIfDemoKernelArg *>(arg);

    ccu::Variable var;
    ccu::Alloc(&var);
    var = args->value;

    CCU_IF(var == args->expected) {
        ccu::Variable thenResult, thenAddend;
        ccu::Alloc(&thenResult);
        ccu::Alloc(&thenAddend);
        thenAddend = 100;
        thenResult = var + thenAddend;
    } CCU_ELSE {
        ccu::Variable elseResult, elseAddend;
        ccu::Alloc(&elseResult);
        ccu::Alloc(&elseAddend);
        elseAddend = 200;
        elseResult = var + elseAddend;
    }

    return CcuResult::CCU_SUCCESS;
}

// ======================== CCU IF_ONLY Demo ========================

struct CcuIfOnlyDemoKernelArg {
    uint32_t value;
    uint64_t threshold;
};

CcuResult CcuIfOnlyDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuIfOnlyDemoKernelArg *>(arg);

    ccu::Variable var;
    ccu::Alloc(&var);
    var = args->value;

    ccu::Variable result;
    ccu::Alloc(&result);
    result = 0;

    ccu::Variable addend;
    ccu::Alloc(&addend);
    addend = 100;

    CCU_IF_ONLY(var == args->threshold) {
        result = var + addend;
    }

    return CcuResult::CCU_SUCCESS;
}

// ======================== CCU WHILE Demo ========================

struct CcuWhileDemoKernelArg {
    uint32_t loopCount;
};

CcuResult CcuWhileDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuWhileDemoKernelArg *>(arg);

    ccu::Variable counter;
    ccu::Alloc(&counter);
    counter = 0;

    ccu::Variable limit;
    ccu::Alloc(&limit);
    limit = args->loopCount;

    ccu::Variable one;
    ccu::Alloc(&one);
    one = 1;

    ccu::Variable accumulator;
    ccu::Alloc(&accumulator);
    accumulator = 0;

    ccu::Variable step;
    ccu::Alloc(&step);
    step = 10;

    CCU_WHILE(counter != args->loopCount) {
        accumulator = accumulator + step;
        counter = counter + one;
    }

    return CcuResult::CCU_SUCCESS;
}

// ======================== CCU DO_WHILE Demo ========================

struct CcuDoWhileDemoKernelArg {
    uint32_t loopCount;
};

CcuResult CcuDoWhileDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuDoWhileDemoKernelArg *>(arg);

    ccu::Variable counter;
    ccu::Alloc(&counter);
    counter = 0;

    ccu::Variable one;
    ccu::Alloc(&one);
    one = 1;

    ccu::Variable accumulator;
    ccu::Alloc(&accumulator);
    accumulator = 0;

    ccu::Variable step;
    ccu::Alloc(&step);
    step = 10;

    CCU_DO_WHILE(counter != args->loopCount) {
        accumulator = accumulator + step;
        counter = counter + one;
    }

    return CcuResult::CCU_SUCCESS;
}
