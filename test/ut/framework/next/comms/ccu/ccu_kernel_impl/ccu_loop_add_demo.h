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

    Variable r1{}, r2{}, r3{}, r4{}, r5{}, r6{}, numA{}, numB{};

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
    std::vector<Loop> group1Loops{loop1, loop2};
    LoopGroup group1(grpCfg1, group1Loops);

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
    std::vector<Loop> group2Loops{loop2, loop3};
    LoopGroup group2(grpCfg2, group2Loops);

    // ========== LoopGroup 3 (var-based): variable group & repeated variable loop ==========
    Variable varLoopParam{}, varParallel{}, varOffset{};

    varLoopParam = 0x0001000200030000ULL;
    varParallel  = 0x0002000100020000ULL;
    varOffset    = 0x1000000100010000ULL;

    Func body4([&]() {
        r6 = numA + numB;
    });
    Loop loop4(varLoopParam, body4);

    std::vector<Loop> group3Loops{loop4, loop4};
    LoopGroup group3(varParallel, varOffset, group3Loops);

    return CcuResult::CCU_SUCCESS;
}
