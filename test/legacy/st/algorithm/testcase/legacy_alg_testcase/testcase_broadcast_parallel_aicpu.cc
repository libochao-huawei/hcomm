/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include <vector>
#include <iostream>
#include <string>

#include "coll_service_stub.h"
#include "checker.h"
#include "testcase_utils.h"
#include "topo_meta.h"

class BroadcastParallelAiCpuTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "Broadcast AiCpuInsBroadcastParallelMesh1DNHR test set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "Broadcast AiCpuInsBroadcastParallelMesh1DNHR test tear down" << std::endl;
    }

    virtual void SetUp()
    {
        const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string caseName =
            "analysis_result_" + std::string(test_info->test_case_name()) + "_" + std::string(test_info->name());
        Checker::SetDumpFileName(caseName);
    }

    virtual void TearDown()
    {
        Checker::SetDumpFileName("analysis_result");
        GlobalMockObject::verify();
        // 这边每个case执行完成需要清理所有的环境变量，如果有新增的环境变量，需要在这个函数中进行清理
        ClearHcclEnv();
    }
    void RunBroadcastTest(
        int root, int supNum, int sevNum, int rankNum, CheckerOpMode opMode, int dataCount, string algName,
        int maxTmpMemSize)
    {
        RankTable_For_LLT gen;
        TopoMeta topoMeta;
        gen.GenTopoMeta(topoMeta, supNum, sevNum, rankNum);
        setenv("HCCL_BUFFSIZE", std::to_string(maxTmpMemSize).c_str(), 1);
        CheckerOpParam checkerOpParam;
        checkerOpParam.opType = CheckerOpType::BROADCAST;
        checkerOpParam.tag = "broadcast";
        checkerOpParam.opMode = opMode;
        checkerOpParam.devtype = CheckerDevType::DEV_TYPE_950;
        checkerOpParam.DataDes.count = dataCount;
        checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT32;
        checkerOpParam.root = root;
        checkerOpParam.algName = algName;

        Checker checker;
        HcclResult ret;
        //checker.EnableTaskPrint();
        ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
        EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
    }
};

TEST_F(BroadcastParallelAiCpuTest, broadcast_aicpu_case_test_2x2_rank_ParallelMesh1DNHR)
{
    RunBroadcastTest(0, 1, 2, 2, CheckerOpMode::OPBASE, 100, "AiCpuInsBroadcastParallelMesh1DNHR", 200);
}

TEST_F(BroadcastParallelAiCpuTest, broadcast_aicpu_case_test_2x8_rank_ParallelMesh1DNHR_root_1)
{
    RunBroadcastTest(1, 1, 2, 8, CheckerOpMode::OPBASE, 800, "AiCpuInsBroadcastParallelMesh1DNHR", 200);
}

TEST_F(BroadcastParallelAiCpuTest, broadcast_aicpu_case_test_2x8_rank_ParallelMesh1DNHR_root_2)
{
    RunBroadcastTest(2, 1, 2, 8, CheckerOpMode::OPBASE, 800, "AiCpuInsBroadcastParallelMesh1DNHR", 200);
}

TEST_F(BroadcastParallelAiCpuTest, broadcast_aicpu_case_test_2x8_rank_ParallelMesh1DNHR_root_7)
{
    RunBroadcastTest(7, 1, 2, 8, CheckerOpMode::OPBASE, 800, "AiCpuInsBroadcastParallelMesh1DNHR", 200);
}

TEST_F(BroadcastParallelAiCpuTest, broadcast_aicpu_case_test_2x8_rank_ParallelMesh1DNHR_root_15)
{
    RunBroadcastTest(15, 1, 2, 8, CheckerOpMode::OPBASE, 800, "AiCpuInsBroadcastParallelMesh1DNHR", 200);
}