/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "coll_alg_component_builder.h"
#include "virtual_topo.h"
#include "dev_type.h"
#include "coll_operator.h"
#include "coll_alg_params.h"
#include "all_gather_auto_selector.h"
#include "all_gather_v_auto_selector.h"
#include "all_reduce_auto_selector.h"
#include "alltoall_auto_selector.h"
#include "alltoallv_auto_selector.h"
#include "alltoallvc_auto_selector.h"
#include "broadcast_auto_selector.h"
#include "reduce_auto_selector.h"
#include "reduce_scatter_auto_selector.h"
#include "reduce_scatter_v_auto_selector.h"
#include "scatter_auto_selector.h"

using namespace Hccl;

class SelectorTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "SelectorTest set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "SelectorTest tear down" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in SelectorTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in SelectorTest TearDown" << std::endl;
    }

    BaseSelector::TopoInfo CreateBasicTopoInfo()
    {
        BaseSelector::TopoInfo topoInfo;
        topoInfo.levelNum = 1;
        topoInfo.level0Shape = Level0Shape::MESH_1D;
        return topoInfo;
    }

    CollAlgOperator CreateBasicCollAlgOperator()
    {
        CollAlgOperator op;
        op.opType = OpType::ALLGATHER;
        op.dataType = DataType::FP32;
        op.dataCount = 1024;
        op.reduceOp = ReduceOp::SUM;
        return op;
    }

    CollAlgParams CreateBasicCollAlgParams()
    {
        CollAlgParams params;
        params.opMode = OpMode::OPBASE;
        params.maxTmpMemSize = 1024 * 1024;
        params.maxQueue = 4;
        params.maxLink = 8;
        params.maxDepQueuePairs = 2;
        params.dataSize = 1024 * 4;
        return params;
    }

    std::map<OpType, std::vector<HcclAlgoType>> CreateConfigAlgMap()
    {
        std::map<OpType, std::vector<HcclAlgoType>> configAlgMap;
        std::vector<HcclAlgoType> algos;
        algos.push_back(HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
        configAlgMap[OpType::ALLGATHER] = algos;
        return configAlgMap;
    }
};

TEST_F(SelectorTest, CollAlgComponentBuilder_SetMainboardId)
{
    CollAlgComponentBuilder builder;
    u64 mainBoardId = 0x1234567879;
    
    auto& result = builder.SetMainboardId(mainBoardId);
    
    EXPECT_EQ(&result, &builder);
}

TEST_F(SelectorTest, AllGatherAutoSelector_Creation)
{
    AllGatherAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, AllGatherVAutoSelector_Creation)
{
    AllGatherVAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, AllReduceAutoSelector_Creation)
{
    AllReduceAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, AlltoAllAutoSelector_Creation)
{
    AlltoAllAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, AlltoAllVAutoSelector_Creation)
{
    AlltoAllVAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, AlltoAllVCAutoSelector_Creation)
{
    AlltoAllVCAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, BroadcastAutoSelector_Creation)
{
    BroadcastAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, ReduceAutoSelector_Creation)
{
    ReduceAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, ReduceScatterAutoSelector_Creation)
{
    ReduceScatterAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, ReduceScatterVAutoSelector_Creation)
{
    ReduceScatterVAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, ScatterAutoSelector_Creation)
{
    ScatterAutoSelector selector;
    (void)selector;
}

TEST_F(SelectorTest, CreateBasicTopoInfo_Valid)
{
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    
    EXPECT_EQ(topoInfo.levelNum, 1);
    EXPECT_EQ(topoInfo.level0Shape, Level0Shape::MESH_1D);
}

TEST_F(SelectorTest, CreateBasicCollAlgOperator_Valid)
{
    CollAlgOperator op = CreateBasicCollAlgOperator();
    
    EXPECT_EQ(op.opType, OpType::ALLGATHER);
    EXPECT_EQ(op.dataType, DataType::FP32);
    EXPECT_EQ(op.dataCount, 1024);
    EXPECT_EQ(op.reduceOp, ReduceOp::SUM);
}

TEST_F(SelectorTest, CreateBasicCollAlgParams_Valid)
{
    CollAlgParams params = CreateBasicCollAlgParams();
    
    EXPECT_EQ(params.opMode, OpMode::OPBASE);
    EXPECT_EQ(params.maxTmpMemSize, 1024 * 1024);
    EXPECT_EQ(params.maxQueue, 4);
    EXPECT_EQ(params.maxLink, 8);
    EXPECT_EQ(params.maxDepQueuePairs, 2);
    EXPECT_EQ(params.dataSize, 1024 * 4);
}

TEST_F(SelectorTest, CreateConfigAlgMap_Valid)
{
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    
    EXPECT_EQ(configAlgMap.count(OpType::ALLGATHER), 1);
    EXPECT_EQ(configAlgMap[OpType::ALLGATHER].size(), 1);
    EXPECT_EQ(configAlgMap[OpType::ALLGATHER][0], HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
}