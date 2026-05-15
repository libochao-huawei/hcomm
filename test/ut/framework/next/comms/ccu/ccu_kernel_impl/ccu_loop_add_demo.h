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

struct CcuLoopAddKernelArg {
    uint32_t numA{0};
    uint32_t numB{0};
};

CcuResult CcuLoopAddDemoKernel(CcuKernelArg arg)
{
    using namespace ccu;
    auto *args = static_cast<CcuLoopAddKernelArg *>(arg);

    // 申请 LoopEngine 池，大小 = max(各 LoopGroup loop 数) = 2；
    // 三个 LoopGroup 都从该池按 local loopIdx 取 executorId，跨组复用 0、1。
    CCU_CHK_RET(SetLoopNum(2));

    Variable r1{}, r2{}, r3{}, r4{}, r5{}, r6{}, r7{}, numA{}, numB{};

    numA = args->numA;
    numB = args->numB;

    r1 = numA + numB;

    // ========== LoopGroup 1 (config-based): two config loops, no unroll ==========
    Func body1([&]() {
        r2 = numA + numB;
    });
    Func body2([&]() {
        r3 = numA + numB;
    });

    CcuLoopConfig cfg1 = {.addrOffset = 0, .loopIterNum = 2};
    CcuLoopConfig cfg2 = {.addrOffset = 0, .loopIterNum = 2};
    Loop loop1(cfg1, body1);
    Loop loop2(cfg2, body2);

    CcuLoopGroupConfig grpCfg1 = {
        .addrOffset = 0, .bufferOffset = 0, .eventOffset = 0,
        .repeatNum = 0, .repeatLoopIdx = 0
    };
    LoopGroup group1(grpCfg1, {loop1, loop2});

    r4 = numA + numB;

    // ========== LoopGroup 2: reuse loop2 + offset loop3 ==========
    Func body3([&]() {
        r5 = numA + numB;
    });
    CcuLoopConfig cfg3 = {.addrOffset = 4096, .loopIterNum = 4};
    Loop loop3(cfg3, body3);

    CcuLoopGroupConfig grpCfg2 = {
        .addrOffset = 4096, .bufferOffset = 1, .eventOffset = 1,
        .repeatNum = 3, .repeatLoopIdx = 1
    };
    LoopGroup group2(grpCfg2, {loop2, loop3});

    // ========== LoopGroup 3 (var-based): variable group with two distinct var-loops ==========
    Variable varLoopParam4{}, varLoopParam5{}, varParallel{}, varOffset{};

    varLoopParam4 = 0x0001000200030000ULL;
    varLoopParam5 = 0x0002000300040000ULL;
    varParallel   = 0x0002000100020000ULL;
    varOffset     = 0x1000000100010000ULL;

    Func body4([&]() {
        r6 = numA + numB;
    });
    Func body5([&]() {
        r7 = numA + numB;
    });
    Loop loop4(varLoopParam4, body4);
    Loop loop5(varLoopParam5, body5);

    LoopGroup group3(varParallel, varOffset, {loop4, loop5});

    return CcuResult::CCU_SUCCESS;
}
