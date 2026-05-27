/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char **argv) {
    // testcase调试代码，只跑特定的用例
    // testing::GTEST_FLAG(filter) = "AllGatherAICPUMesh2dNHRTest.Aicpu_AllGather_Parallel_executor_4x2_2x2";
    testing::InitGoogleTest(&argc, argv);
    // 用来控制各个LLT任务的topoPath.json的名字，不能和其他任务的名字一样，可能会冲突
    setenv("TOPO_PATH_NAME", "testcase", 1);
    return RUN_ALL_TESTS();
}

// [----------] Global test environment tear-down
// [==========] 289 tests from 18 test suites ran. (44642 ms total)
// [  PASSED  ] 258 tests.
// [  FAILED  ] 31 tests, listed below:



// AllReduceTest.allreduce_aicpu_case_test_AllReduceNHR_2pod_2mul2_2mul2
// AllReduceTest.allreduce_aicpu_case_test_AllReduceNHR_3pod_2mul1_2mul2_3mul1
// ReduceScatterTest.reduceScatter_aicpu_case_test_ReduceScatterNHR_2pod_2mul2_2mul2
// ReduceScatterTest.reduceScatter_aicpu_case_test_ReduceScatterNHR_3pod_2mul1_2mul2_3mul1
