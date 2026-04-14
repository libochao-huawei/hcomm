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

// // ======================== CCU IF Demo ========================

// struct CcuIfDemoKernelArg {
//     uint32_t value;
//     uint64_t expected;
// };

// CcuResult CcuIfDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuIfDemoKernelArg *>(arg);

//     CcuVariable var{};
//     CCU_CHK_RET(CcuVariableCreate(&var));
//     var = args->value;

//     // if (var == expected) { result = var + 100 } else { result = var + 200 }
//     CCU_IF(var == args->expected) {
//         CcuVariable thenResult{}, thenAddend{};
//         CCU_CHK_RET(CcuVariableCreate(&thenResult));
//         CCU_CHK_RET(CcuVariableCreate(&thenAddend));
//         thenAddend = 100;
//         thenResult = var + thenAddend;
//     } CCU_ELSE {
//         CcuVariable elseResult{}, elseAddend{};
//         CCU_CHK_RET(CcuVariableCreate(&elseResult));
//         CCU_CHK_RET(CcuVariableCreate(&elseAddend));
//         elseAddend = 200;
//         elseResult = var + elseAddend;
//     }

//     return CcuResult::CCU_SUCCESS;
// }

// // ======================== CCU IF_ONLY Demo ========================

// struct CcuIfOnlyDemoKernelArg {
//     uint32_t value;
//     uint64_t threshold;
// };

// CcuResult CcuIfOnlyDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuIfOnlyDemoKernelArg *>(arg);

//     CcuVariable var{};
//     CCU_CHK_RET(CcuVariableCreate(&var));
//     var = args->value;

//     CcuVariable result{};
//     CCU_CHK_RET(CcuVariableCreate(&result));
//     result = 0;

//     // if (var == threshold) { result = var + 100 }
//     CcuVariable addend{};
//     CCU_CHK_RET(CcuVariableCreate(&addend));
//     addend = 100;

//     CCU_IF_ONLY(var == args->threshold) {
//         result = var + addend;
//     }

//     return CcuResult::CCU_SUCCESS;
// }

// // ======================== CCU WHILE Demo ========================

// struct CcuWhileDemoKernelArg {
//     uint32_t loopCount;
// };

// CcuResult CcuWhileDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuWhileDemoKernelArg *>(arg);

//     CcuVariable counter{};
//     CCU_CHK_RET(CcuVariableCreate(&counter));
//     counter = 0;

//     CcuVariable limit{};
//     CCU_CHK_RET(CcuVariableCreate(&limit));
//     limit = args->loopCount;

//     CcuVariable one{};
//     CCU_CHK_RET(CcuVariableCreate(&one));
//     one = 1;

//     CcuVariable accumulator{};
//     CCU_CHK_RET(CcuVariableCreate(&accumulator));
//     accumulator = 0;

//     CcuVariable step{};
//     CCU_CHK_RET(CcuVariableCreate(&step));
//     step = 10;

//     // while (counter != limit) { accumulator += step; counter += 1; }
//     CCU_WHILE(counter != args->loopCount) {
//         accumulator = accumulator + step;
//         counter = counter + one;
//     }

//     return CcuResult::CCU_SUCCESS;
// }

// // ======================== CCU DO_WHILE Demo ========================

// struct CcuDoWhileDemoKernelArg {
//     uint32_t loopCount;
// };

// CcuResult CcuDoWhileDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuDoWhileDemoKernelArg *>(arg);

//     CcuVariable counter{};
//     CCU_CHK_RET(CcuVariableCreate(&counter));
//     counter = 0;

//     CcuVariable one{};
//     CCU_CHK_RET(CcuVariableCreate(&one));
//     one = 1;

//     CcuVariable accumulator{};
//     CCU_CHK_RET(CcuVariableCreate(&accumulator));
//     accumulator = 0;

//     CcuVariable step{};
//     CCU_CHK_RET(CcuVariableCreate(&step));
//     step = 10;

//     // do { accumulator += step; counter += 1; } while (counter != loopCount)
//     CCU_DO_WHILE(counter != args->loopCount) {
//         accumulator = accumulator + step;
//         counter = counter + one;
//     }

//     return CcuResult::CCU_SUCCESS;
// }
