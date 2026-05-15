/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_FUNC_CALL_DEMO_H
#define CCU_FUNC_CALL_DEMO_H

#include "ccu_api.hpp"

ccu::Func CcuFuncCallBasicFunc([](ccu::Variable x) {
    ccu::Variable tmp{};
    tmp = x + x;
});

ccu::Func CcuFuncCallNestedInnerFunc([](ccu::Variable x) {
    ccu::Variable tmp{};
    tmp = x + x;
});

ccu::Func CcuFuncCallNestedOuterFunc([](ccu::Variable x) {
    (void)ccu::CallFunc<CcuFuncCallNestedInnerFunc>(x);
});

inline CcuResult CcuFuncCallBasicDemoKernel(CcuKernelArg arg)
{
    (void)arg;
    ccu::Variable x{};
    x = 1;
    return ccu::CallFunc<CcuFuncCallBasicFunc>(x);
}

inline CcuResult CcuFuncCallReuseDemoKernel(CcuKernelArg arg)
{
    (void)arg;
    ccu::Variable x{};
    x = 2;
    CCU_CHK_RET(ccu::CallFunc<CcuFuncCallBasicFunc>(x));
    return ccu::CallFunc<CcuFuncCallBasicFunc>(x);
}

inline CcuResult CcuFuncCallInLoopInvalidDemoKernel(CcuKernelArg arg)
{
    (void)arg;
    ccu::Variable x{};
    x = 3;

    CcuResult bodyRet = CcuResult::CCU_SUCCESS;
    ccu::Func body([&]() {
        bodyRet = ccu::CallFunc<CcuFuncCallBasicFunc>(x);
    });
    CcuLoopConfig dummyCfg{};
    ccu::Loop loop(dummyCfg, body);
    return bodyRet;
}

inline CcuResult CcuFuncCallNestedInvalidDemoKernel(CcuKernelArg arg)
{
    (void)arg;
    ccu::Variable x{};
    x = 4;
    return ccu::CallFunc<CcuFuncCallNestedOuterFunc>(x);
}

// 多入参 Func：lambda 形参个数由 ccu::Func 在编译期推导（FunctorTraits<Lambda>::Arity），
// CallFunc 时若实参个数不匹配会返回 CCU_E_PARA。本 demo 演示 3 个 Variable 入参。
ccu::Func CcuFuncCallMultiArgFunc([](ccu::Variable a, ccu::Variable b, ccu::Variable c) {
    ccu::Variable tmp{};
    tmp = a + b;
    tmp = tmp + c;
});

inline CcuResult CcuFuncCallMultiArgDemoKernel(CcuKernelArg arg)
{
    (void)arg;
    ccu::Variable a{}, b{}, c{};
    a = 5;
    b = 6;
    c = 7;
    CCU_CHK_RET(ccu::CallFunc<CcuFuncCallMultiArgFunc>(a, b, c));
    return ccu::CallFunc<CcuFuncCallMultiArgFunc>(a, b, c);
}

#endif // CCU_FUNC_CALL_DEMO_H
