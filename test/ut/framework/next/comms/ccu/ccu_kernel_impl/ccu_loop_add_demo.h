// /**
//  * Copyright (c) 2026 Huawei Technologies Co., Ltd.
//  * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
//  * CANN Open Software License Agreement Version 2.0 (the "License").
//  * Please refer to the License for details. You may not use this file except in compliance with the License.
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
//  * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
//  * See LICENSE in the root of the software repository for the full text of the License.
//  */

// #include "ccu_data_api.h"
// #include "ccu_log.h"

// struct CcuLoopAddKernelArg {
//     uint32_t numA{0};
//     uint32_t numB{0};
// };

// CcuResult CcuLoopAddDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuLoopAddKernelArg *>(arg);

//     CcuVariable r1{}, r2{}, r3{}, r4{}, r5{}, r6{}, numA{}, numB{};

//     CCU_CHK_RET(ccu::Variable(&r1));
//     CCU_CHK_RET(ccu::Variable(&r2));
//     CCU_CHK_RET(ccu::Variable(&r3));
//     CCU_CHK_RET(ccu::Variable(&r4));
//     CCU_CHK_RET(ccu::Variable(&r5));
//     CCU_CHK_RET(ccu::Variable(&r6));
//     CCU_CHK_RET(ccu::Variable(&numA));
//     CCU_CHK_RET(ccu::Variable(&numB));

//     numA = args->numA;
//     numB = args->numB;

//     r1 = numA + numB;

//     // ========== Loop 1: param bindings ==========
//     CcuLoopHandle loop1;
//     CCU_LOOPBODY(loop1) {
//         CcuVariable formalA{}, formalB{}, formalResult{};
//         CCU_CHK_RET(ccu::Variable(&formalA));
//         CCU_CHK_RET(ccu::Variable(&formalB));
//         CCU_CHK_RET(ccu::Variable(&formalResult));

//         CCU_CHK_RET(CcuLoopSetParam(loop1, &formalA, &numA));
//         CCU_CHK_RET(CcuLoopSetParam(loop1, &formalB, &numB));
//         CCU_CHK_RET(CcuLoopSetParam(loop1, &formalResult, &r2));

//         formalResult = formalA + formalB;
//     }

//     // ========== Loop 2: external variables directly ==========
//     CcuLoopHandle loop2;
//     CCU_LOOPBODY(loop2) {
//         r3 = numA + numB;
//     }

//     // ========== LoopGroup 1 (config-based): two non-unroll loops ==========
//     CcuLoopGroupHandle group1;
//     CcuLoopGroupConfig grpCfg1 = {
//         .addrOffset = 0, .bufferOffset = 0, .eventOffset = 0,
//         .repeatNum = 1
//     };
//     CCU_CHK_RET(CcuLoopGroupCreate(&group1, &grpCfg1));

//     CcuLoopConfig cfg1 = {.addrOffset = 0, .loopIterNum = 2};
//     CCU_CHK_RET(CcuLoopGroupAddLoop(group1, loop1, &cfg1, false));

//     CcuLoopConfig cfg2 = {.addrOffset = 0, .loopIterNum = 2};
//     CCU_CHK_RET(CcuLoopGroupAddLoop(group1, loop2, &cfg2, false));

//     r4 = numA + numB;

//     // ========== Loop 3: unroll with offsets and param bindings ==========
//     CcuLoopHandle loop3;
//     CCU_LOOPBODY(loop3) {
//         CcuVariable formalX{}, formalY{}, formalOut{};
//         CCU_CHK_RET(ccu::Variable(&formalX));
//         CCU_CHK_RET(ccu::Variable(&formalY));
//         CCU_CHK_RET(ccu::Variable(&formalOut));

//         CCU_CHK_RET(CcuLoopSetParam(loop3, &formalX, &numA));
//         CCU_CHK_RET(CcuLoopSetParam(loop3, &formalY, &numB));
//         CCU_CHK_RET(CcuLoopSetParam(loop3, &formalOut, &r5));

//         formalOut = formalX + formalY;
//     }

//     // ========== LoopGroup 2: non-unroll (reuse loop2) + unroll with offsets ==========
//     CcuLoopGroupHandle group2;
//     CcuLoopGroupConfig grpCfg2 = {
//         .addrOffset = 4096, .bufferOffset = 1, .eventOffset = 1,
//         .repeatNum = 3
//     };
//     CCU_CHK_RET(CcuLoopGroupCreate(&group2, &grpCfg2));

//     CcuLoopConfig cfg2_reuse = {.addrOffset = 0, .loopIterNum = 2};
//     CCU_CHK_RET(CcuLoopGroupAddLoop(group2, loop2, &cfg2_reuse, false));

//     CcuLoopConfig cfg3 = {.addrOffset = 4096, .loopIterNum = 4};
//     CCU_CHK_RET(CcuLoopGroupAddLoop(group2, loop3, &cfg3, true));

//     // ========== LoopGroup 3 (var-based): CcuVariable-based group & loop ==========
//     CcuVariable varLoopParam{}, varParallel{}, varOffset{};
//     CCU_CHK_RET(ccu::Variable(&varLoopParam));
//     CCU_CHK_RET(ccu::Variable(&varParallel));
//     CCU_CHK_RET(ccu::Variable(&varOffset));

//     varLoopParam = 0x0001000200030000ULL;
//     varParallel  = 0x0002000100020000ULL;
//     varOffset    = 0x1000000100010000ULL;

//     CcuLoopHandle loop4;
//     CCU_LOOPBODY(loop4) {
//         r6 = numA + numB;
//     }

//     CcuLoopGroupHandle group3;
//     CCU_CHK_RET(CcuLoopGroupCreateFromVar(&group3, &varParallel, &varOffset));
//     CCU_CHK_RET(CcuLoopGroupAddLoopFromVar(group3, loop4, &varLoopParam, false));

//     return CcuResult::CCU_SUCCESS;
// }
