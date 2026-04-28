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

// ======================== CCU IF (without else) Demo ========================

struct CcuIfNoElseDemoKernelArg {
    uint32_t value;
    uint64_t threshold;
};

CcuResult CcuIfNoElseDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuIfNoElseDemoKernelArg *>(arg);

    ccu::Variable var;
    ccu::Alloc(&var);
    var = args->value;

    ccu::Variable result;
    ccu::Alloc(&result);
    result = 0;

    ccu::Variable addend;
    ccu::Alloc(&addend);
    addend = 100;

    CCU_IF(var == args->threshold) {
        result = var + addend;
    }

    return CcuResult::CCU_SUCCESS;
}

// ======================== Nested IF: if{if{}}else{} ========================

struct CcuNestedIfOuterElseDemoKernelArg {
    uint32_t outerVal;
    uint64_t outerExpected;
    uint32_t innerVal;
    uint64_t innerExpected;
};

CcuResult CcuNestedIfOuterElseDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuNestedIfOuterElseDemoKernelArg *>(arg);

    ccu::Variable outerVar;
    ccu::Alloc(&outerVar);
    outerVar = args->outerVal;

    ccu::Variable innerVar;
    ccu::Alloc(&innerVar);
    innerVar = args->innerVal;

    ccu::Variable result;
    ccu::Alloc(&result);
    result = 0;

    ccu::Variable addend;
    ccu::Alloc(&addend);

    CCU_IF(outerVar == args->outerExpected) {
        CCU_IF(innerVar == args->innerExpected) {
            addend = 10;
            result = result + addend;
        }
        addend = 20;
        result = result + addend;
    } CCU_ELSE {
        addend = 30;
        result = result + addend;
    }

    return CcuResult::CCU_SUCCESS;
}

// ======================== Nested IF: if{if{}else{}} ========================

struct CcuNestedIfInnerElseDemoKernelArg {
    uint32_t outerVal;
    uint64_t outerExpected;
    uint32_t innerVal;
    uint64_t innerExpected;
};

CcuResult CcuNestedIfInnerElseDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuNestedIfInnerElseDemoKernelArg *>(arg);

    ccu::Variable outerVar;
    ccu::Alloc(&outerVar);
    outerVar = args->outerVal;

    ccu::Variable innerVar;
    ccu::Alloc(&innerVar);
    innerVar = args->innerVal;

    ccu::Variable result;
    ccu::Alloc(&result);
    result = 0;

    ccu::Variable addend;
    ccu::Alloc(&addend);

    CCU_IF(outerVar == args->outerExpected) {
        CCU_IF(innerVar == args->innerExpected) {
            addend = 10;
            result = result + addend;
        } CCU_ELSE {
            addend = 20;
            result = result + addend;
        }
        addend = 30;
        result = result + addend;
    }

    return CcuResult::CCU_SUCCESS;
}

// ======================== Nested IF: if{}if{} ========================

struct CcuNestedIfIfDemoKernelArg {
    uint32_t outerVal;
    uint64_t outerExpected;
    uint32_t innerVal;
    uint64_t innerExpected;
};

CcuResult CcuNestedIfIfDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuNestedIfIfDemoKernelArg *>(arg);

    ccu::Variable outerVar;
    ccu::Alloc(&outerVar);
    outerVar = args->outerVal;

    ccu::Variable innerVar;
    ccu::Alloc(&innerVar);
    innerVar = args->innerVal;

    ccu::Variable result;
    ccu::Alloc(&result);
    result = 0;

    ccu::Variable addend;
    ccu::Alloc(&addend);

    CCU_IF(outerVar == args->outerExpected) {
        addend = 10;
        result = result + addend;
         
    }
    CCU_IF(innerVar == args->innerExpected) {
        addend = 20;
        result = result + addend;
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

// ======================== CCU DO_WHILE_WHILE Demo (do{while{}}while) ========================

struct CcuDoWhileWhileDemoKernelArg {
    uint32_t loopCount;
};

CcuResult CcuDoWhileWhileDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuDoWhileWhileDemoKernelArg *>(arg);

    ccu::Variable counter_1;
    ccu::Alloc(&counter_1);
    counter_1 = 0;
    ccu::Variable counter_2;
    ccu::Alloc(&counter_2);
    counter_2 = 0;

    ccu::Variable one;
    ccu::Alloc(&one);
    one = 1;

    ccu::Variable accumulator;
    ccu::Alloc(&accumulator);
    accumulator = 0;

    ccu::Variable step;
    ccu::Alloc(&step);
    step = 10;

    CCU_DO {
        CCU_WHILE(counter_2 != args->loopCount) {
            accumulator = accumulator + step;
            counter_2 = counter_2 + one;
        }
        accumulator = accumulator + step;
        counter_1 = counter_1 + one;
    } CCU_WHILE(counter_1 != args->loopCount);
    return CcuResult::CCU_SUCCESS;
}

// ======================== CCU DO {} CCU_WHILE Demo (unified syntax) ========================

struct CcuDoWhileUnifiedDemoKernelArg {
    uint32_t loopCount;
};

CcuResult CcuDoWhileUnifiedDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuDoWhileUnifiedDemoKernelArg *>(arg);

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

    CCU_DO {
        accumulator = accumulator + step;
        counter = counter + one;
    } CCU_WHILE(counter != args->loopCount);

    return CcuResult::CCU_SUCCESS;
}
