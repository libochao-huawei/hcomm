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
#include "env_config.h"

using namespace Hccl;

class SelectorUnusedParamTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "SelectorUnusedParamTest set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "SelectorUnusedParamTest tear down" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in SelectorUnusedParamTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in SelectorUnusedParamTest TearDown" << std::endl;
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

    std::map<OpType, std::vector<HcclAlgoType>> CreateConfigAlgMap()
    {
        std::map<OpType, std::vector<HcclAlgoType>> configAlgMap;
        std::vector<HcclAlgoType> algos;
        algos.push_back(HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT);
        configAlgMap[OpType::ALLGATHER] = algos;
        return configAlgMap;
    }
};

TEST_F(SelectorUnusedParamTest, CollAlgComponentBuilder_SetMainboardId)
{
    CollAlgComponentBuilder builder;
    u64 mainBoardId = 0x1234567890;
    
    auto& result = builder.SetMainboardId(mainBoardId);
    
    EXPECT_EQ(&result, &builder);
}

TEST_F(SelectorUnusedParamTest, AllGatherAutoSelector_SelectCcuMsAlgo_UnusedParams)
{
    AllGatherAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuMsAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllGatherAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    AllGatherAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllGatherAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    AllGatherAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllGatherAutoSelector_SelectAivAlgo_UnusedParams)
{
    AllGatherAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllGatherVAutoSelector_SelectCcuMsAlgo_UnusedParams)
{
    AllGatherVAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuMsAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllGatherVAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    AllGatherVAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllReduceAutoSelector_SelectCcuMsAlgo_UnusedParams)
{
    AllReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLREDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuMsAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllReduceAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    AllReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLREDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllReduceAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    AllReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLREDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AllReduceAutoSelector_SelectAivAlgo_UnusedParams)
{
    AllReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLREDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AlltoAllAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    AlltoAllAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLTOALL;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AlltoAllAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    AlltoAllAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLTOALL;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AlltoAllAutoSelector_SelectAivAlgo_UnusedParams)
{
    AlltoAllAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLTOALL;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AlltoAllVAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    AlltoAllVAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLTOALLV;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AlltoAllVAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    AlltoAllVAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLTOALLV;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AlltoAllVAutoSelector_SelectAivAlgo_UnusedParams)
{
    AlltoAllVAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLTOALLV;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, AlltoAllVCAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    AlltoAllVCAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::ALLTOALLVC;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, BroadcastAutoSelector_SelectCcuMsAlgo_UnusedParams)
{
    BroadcastAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::BROADCAST;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuMsAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, BroadcastAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    BroadcastAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::BROADCAST;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, BroadcastAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    BroadcastAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::BROADCAST;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, BroadcastAutoSelector_SelectAivAlgo_UnusedParams)
{
    BroadcastAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::BROADCAST;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceAutoSelector_SelectCcuMsAlgo_UnusedParams)
{
    ReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuMsAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    ReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    ReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceAutoSelector_SelectAivAlgo_UnusedParams)
{
    ReduceAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCE;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceScatterAutoSelector_SelectCcuMsAlgo_UnusedParams)
{
    ReduceScatterAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCESCATTER;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuMsAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceScatterAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    ReduceScatterAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCESCATTER;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceScatterAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    ReduceScatterAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCESCATTER;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceScatterAutoSelector_SelectAivAlgo_UnusedParams)
{
    ReduceScatterAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCESCATTER;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceScatterVAutoSelector_SelectCcuMsAlgo_UnusedParams)
{
    ReduceScatterVAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCESCATTERV;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuMsAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ReduceScatterVAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    ReduceScatterVAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::REDUCESCATTERV;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ScatterAutoSelector_SelectCcuScheduleAlgo_UnusedParams)
{
    ScatterAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::SCATTER;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectCcuScheduleAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ScatterAutoSelector_SelectAicpuAlgo_UnusedParams)
{
    ScatterAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::SCATTER;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAicpuAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}

TEST_F(SelectorUnusedParamTest, ScatterAutoSelector_SelectAivAlgo_UnusedParams)
{
    ScatterAutoSelector selector;
    BaseSelector::TopoInfo topoInfo = CreateBasicTopoInfo();
    CollAlgOperator op = CreateBasicCollAlgOperator();
    op.opType = OpType::SCATTER;
    std::map<OpType, std::vector<HcclAlgoType>> configAlgMap = CreateConfigAlgMap();
    std::string primQueueGenName;
    
    auto status = selector.SelectAivAlgo(topoInfo, op, configAlgMap, primQueueGenName);
    
    EXPECT_TRUE(status == SelectorStatus::MATCH || status == SelectorStatus::NOT_MATCH);
}