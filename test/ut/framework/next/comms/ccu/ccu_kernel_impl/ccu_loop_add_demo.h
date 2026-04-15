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

    CcuVariable r1{}, r2{}, r3{}, r4{}, r5{}, r6{}, numA{}, numB{};

    CCU_CHK_RET(Alloc(&r1));
    CCU_CHK_RET(Alloc(&r2));
    CCU_CHK_RET(Alloc(&r3));
    CCU_CHK_RET(Alloc(&r4));
    CCU_CHK_RET(Alloc(&r5));
    CCU_CHK_RET(Alloc(&r6));
    CCU_CHK_RET(Alloc(&numA));
    CCU_CHK_RET(Alloc(&numB));

    numA = args->numA;
    numB = args->numB;

    r1 = numA + numB;

    // group2 需要最多引擎: 1(非展开) + 1+3(展开3次) = 5
    CcuLoopExecutors enginePool;
    CCU_CHK_RET(CreateBlockExecutor(&enginePool, 5));

    // ========== Loop 1: param bindings ==========
    CcuLoop loop1;
    CCU_LOOP(loop1) {
        CcuVariable formalA{}, formalB{}, formalResult{};
        CCU_CHK_RET(Alloc(&formalA));
        CCU_CHK_RET(Alloc(&formalB));
        CCU_CHK_RET(Alloc(&formalResult));

        CCU_CHK_RET(LoopSetParam(loop1, &formalA, &numA));
        CCU_CHK_RET(LoopSetParam(loop1, &formalB, &numB));
        CCU_CHK_RET(LoopSetParam(loop1, &formalResult, &r2));

        formalResult = formalA + formalB;
    }

    // ========== Loop 2: external variables directly ==========
    CcuLoop loop2;
    CCU_LOOP(loop2) {
        r3 = numA + numB;
    }

    // ========== LoopGroup 1 (config-based): two loops, no unroll ==========
    CcuLoopGroup group1;
    CcuLoopGroupConfig grpCfg1 = {
        .addrOffset = 0, .bufferOffset = 0, .eventOffset = 0,
        .repeatNum = 0, .repeatLoopIdx = 0
    };
    CCU_CHK_RET(LoopGroupCreate(&group1, &grpCfg1, enginePool));

    CcuLoopConfig cfg1 = {.addrOffset = 0, .loopIterNum = 2};
    CCU_CHK_RET(LoopGroupAddLoop(group1, loop1, &cfg1));

    CcuLoopConfig cfg2 = {.addrOffset = 0, .loopIterNum = 2};
    CCU_CHK_RET(LoopGroupAddLoop(group1, loop2, &cfg2));

    r4 = numA + numB;

    // ========== Loop 3: with offsets and param bindings ==========
    CcuLoop loop3;
    CCU_LOOP(loop3) {
        CcuVariable formalX{}, formalY{}, formalOut{};
        CCU_CHK_RET(Alloc(&formalX));
        CCU_CHK_RET(Alloc(&formalY));
        CCU_CHK_RET(Alloc(&formalOut));

        CCU_CHK_RET(LoopSetParam(loop3, &formalX, &numA));
        CCU_CHK_RET(LoopSetParam(loop3, &formalY, &numB));
        CCU_CHK_RET(LoopSetParam(loop3, &formalOut, &r5));

        formalOut = formalX + formalY;
    }

    // ========== LoopGroup 2: loop2(no unroll) + loop3(unroll 3x) ==========
    CcuLoopGroup group2;
    CcuLoopGroupConfig grpCfg2 = {
        .addrOffset = 4096, .bufferOffset = 1, .eventOffset = 1,
        .repeatNum = 3, .repeatLoopIdx = 1
    };
    CCU_CHK_RET(LoopGroupCreate(&group2, &grpCfg2, enginePool));

    CcuLoopConfig cfg2_reuse = {.addrOffset = 0, .loopIterNum = 2};
    CCU_CHK_RET(LoopGroupAddLoop(group2, loop2, &cfg2_reuse));

    CcuLoopConfig cfg3 = {.addrOffset = 4096, .loopIterNum = 4};
    CCU_CHK_RET(LoopGroupAddLoop(group2, loop3, &cfg3));

    // ========== LoopGroup 3 (var-based): CcuVariable-based group & loop ==========
    CcuVariable varLoopParam{}, varParallel{}, varOffset{};
    CCU_CHK_RET(Alloc(&varLoopParam));
    CCU_CHK_RET(Alloc(&varParallel));
    CCU_CHK_RET(Alloc(&varOffset));

    varLoopParam = 0x0001000200030000ULL;
    varParallel  = 0x0002000100020000ULL;
    varOffset    = 0x1000000100010000ULL;

    CcuLoop loop4;
    CCU_LOOP(loop4) {
        r6 = numA + numB;
    }

    CcuLoopGroup group3;
    CCU_CHK_RET(LoopGroupCreate(&group3, &varParallel, &varOffset, enginePool));
    CCU_CHK_RET(LoopGroupAddLoop(group3, loop4, &varLoopParam));

    return CcuResult::CCU_SUCCESS;
}
