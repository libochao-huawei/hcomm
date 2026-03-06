/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include <vector>
#include <iostream>
#include <string>

#include "testcase_utils.h"
#include "coll_alg_component_builder.h"
#include "virtual_topo.h"
#include "virtual_topo_stub.h"
#include "coll_alg_params.h"
#include "coll_operator.h"
#include "execute_selector.h"
#include "base_selector.h"
#include "dev_buffer.h"

using namespace Hccl;
using namespace std;

class CollAlgComponentTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CalcResTest set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CalcResTest tear down" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "CalcResTest set up" << std::endl;
    }

    virtual void TearDown()
    {
        ClearHcclEnv();
    }
};

TEST_F(CollAlgComponentTest, CalcResTest)
{
    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095OneTimesFour(rankTable);

    RankId myRank = 0;
    u32 rankSize = 4;

    CollAlgComponentBuilder collAlgComponentBuilder;
    std::shared_ptr<CollAlgComponent> collAlgComponent = collAlgComponentBuilder.SetRankGraph(&virtTopo)
                                                             .SetDevType(DevType::DEV_TYPE_950)
                                                             .SetMyRank(myRank)
                                                             .SetRankSize(rankSize)
                                                             .Build();
    OpExecuteConfig opExecuteConfig;
    AcceleratorState accState = AcceleratorState::CCU_MS;
    setenv("PRIM_QUEUE_GEN_NAME", "CcuAllReduceMesh1D", 1);

    OpType opType = OpType::ALLREDUCE;
    u64 dataSize = 100;
    CollOffloadOpResReq resReq1;

    EXPECT_NO_THROW(collAlgComponent->CalcResOffload(opType, dataSize, HcclDataType::HCCL_DATA_TYPE_INT16, opExecuteConfig, resReq1));
    EXPECT_EQ(16, resReq1.requiredSubQueNum);
    EXPECT_EQ(256 * 1024 * 1024, resReq1.requiredScratchMemSize);
    unsetenv("PRIM_QUEUE_GEN_NAME");
}


TEST_F(CollAlgComponentTest, SelectorOffloadTest)
{
    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095OneTimesFour(rankTable);

    RankId myRank = 0;
    u32 rankSize = 4;

    CollAlgComponentBuilder collAlgComponentBuilder;
    std::shared_ptr<CollAlgComponent> collAlgComponent = collAlgComponentBuilder.SetRankGraph(&virtTopo)
                                                             .SetDevType(DevType::DEV_TYPE_950)
                                                             .SetMyRank(myRank)
                                                             .SetRankSize(rankSize)
                                                             .Build();
    OpExecuteConfig opExecuteConfig;
    opExecuteConfig.accState = AcceleratorState::CCU_FALLBACK;

    CollOffloadOpResReq resReq1;
    OpType opType = OpType::ALLGATHER;
    CollAlgOperator collAlgOp;
    collAlgOp.opType = opType;
    collAlgOp.dataType = DataType::FP16;;
    collAlgOp.dataCount = 100;
    uint64_t dataSize = collAlgOp.dataCount * 2;
    collAlgOp.inputMem = DevBuffer::Create(0x1000000, dataSize);
    collAlgOp.outputMem = DevBuffer::Create(0x2000000, dataSize);
    collAlgOp.scratchMem = DevBuffer::Create(0x3000000, dataSize);

    CollAlgParams collAlgParams;
    collAlgParams.opMode = OpMode::OFFLOAD;
    collAlgParams.opExecuteConfig = opExecuteConfig;
    std::string algName = "";


    EXPECT_NO_THROW(collAlgComponent->ExecAlgSelect(collAlgOp, collAlgParams, algName, opExecuteConfig));
}

TEST_F(CollAlgComponentTest, SelectorMc2Test)
{
    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095OneTimesFour(rankTable);

    RankId myRank = 0;
    u32 rankSize = 4;

    CollAlgComponentBuilder collAlgComponentBuilder;
    std::shared_ptr<CollAlgComponent> collAlgComponent = collAlgComponentBuilder.SetRankGraph(&virtTopo)
                                                             .SetDevType(DevType::DEV_TYPE_950)
                                                             .SetMyRank(myRank)
                                                             .SetRankSize(rankSize)
                                                             .Build();
    OpExecuteConfig opExecuteConfig;
    opExecuteConfig.accState = AcceleratorState::AICPU_TS;

    CollOffloadOpResReq resReq1;
    OpType opType = OpType::ALLGATHER;
    CollAlgOperator collAlgOp;
    collAlgOp.opType = opType;
    collAlgOp.dataType = DataType::FP16;;
    collAlgOp.dataCount = 100;
    uint64_t dataSize = collAlgOp.dataCount * 2;
    collAlgOp.inputMem = DevBuffer::Create(0x1000000, dataSize);
    collAlgOp.outputMem = DevBuffer::Create(0x2000000, dataSize);
    collAlgOp.scratchMem = DevBuffer::Create(0x3000000, dataSize);

    CollAlgParams collAlgParams;
    collAlgParams.opMode = OpMode::OFFLOAD;
    collAlgParams.opExecuteConfig = opExecuteConfig;
    collAlgParams.isMc2 = true;
    std::string algName = "";

    EXPECT_NO_THROW(collAlgComponent->ExecAlgSelect(collAlgOp, collAlgParams, algName, opExecuteConfig));

    opExecuteConfig.accState = AcceleratorState::CCU_MS;
    collAlgParams.opExecuteConfig = opExecuteConfig;
    EXPECT_NO_THROW(collAlgComponent->ExecAlgSelect(collAlgOp, collAlgParams, algName, opExecuteConfig));
}

TEST_F(CollAlgComponentTest, SelectorMc2Test2D)
{
    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095TwoTimesThree(rankTable);

    RankId myRank = 0;
    u32 rankSize = 6;

    CollAlgComponentBuilder collAlgComponentBuilder;
    std::shared_ptr<CollAlgComponent> collAlgComponent = collAlgComponentBuilder.SetRankGraph(&virtTopo)
                                                             .SetDevType(DevType::DEV_TYPE_950)
                                                             .SetMyRank(myRank)
                                                             .SetRankSize(rankSize)
                                                             .Build();
    OpExecuteConfig opExecuteConfig;
    opExecuteConfig.accState = AcceleratorState::AICPU_TS;

    CollOffloadOpResReq resReq1;
    OpType opType = OpType::ALLGATHER;
    CollAlgOperator collAlgOp;
    collAlgOp.opType = opType;
    collAlgOp.dataType = DataType::FP16;;
    collAlgOp.dataCount = 100;
    uint64_t dataSize = collAlgOp.dataCount * 2;
    collAlgOp.inputMem = DevBuffer::Create(0x1000000, dataSize);
    collAlgOp.outputMem = DevBuffer::Create(0x2000000, dataSize);
    collAlgOp.scratchMem = DevBuffer::Create(0x3000000, dataSize);

    CollAlgParams collAlgParams;
    collAlgParams.opMode = OpMode::OFFLOAD;
    collAlgParams.opExecuteConfig = opExecuteConfig;
    collAlgParams.isMc2 = true;
    std::string algName = "";

    EXPECT_NO_THROW(collAlgComponent->ExecAlgSelect(collAlgOp, collAlgParams, algName, opExecuteConfig));

    opExecuteConfig.accState = AcceleratorState::CCU_MS;
    collAlgParams.opExecuteConfig = opExecuteConfig;
    EXPECT_NO_THROW(collAlgComponent->ExecAlgSelect(collAlgOp, collAlgParams, algName, opExecuteConfig));
}