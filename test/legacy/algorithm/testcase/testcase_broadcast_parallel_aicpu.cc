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
        const ::testing::TestInfo *const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
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
};

TEST_F(BroadcastParallelAiCpuTest, broadcast_aicpu_case_test_2_mul_1_rank_ParallelMesh1DNHR)
{
    // 此算法有ERROR级别日志报错
    RankTable_For_LLT gen;
    TopoMeta topoMeta {{{0},{0}}};

    setenv("HCCL_IODIE_NUM", "2", 1);
    // buffersize: 200 * 1024 * 1024
    setenv("HCCL_BUFFSIZE", "200", 1);

    CheckerOpParam checkerOpParam;
    checkerOpParam.opType = CheckerOpType::BROADCAST;
    checkerOpParam.tag = "broadcast";
    checkerOpParam.opMode = CheckerOpMode::OPBASE;
    checkerOpParam.devtype = CheckerDevType::DEV_TYPE_910_95;
    checkerOpParam.DataDes.count = 100;
    checkerOpParam.DataDes.dataType = CheckerDataType::DATA_TYPE_INT32;
    checkerOpParam.root = 0;    
    checkerOpParam.algName = "AiCpuInsBroadcastParallelMesh1DNHR";

    Checker checker;
    HcclResult ret;
    ret = checker.CheckA5Aicpu(checkerOpParam, topoMeta);
   
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}