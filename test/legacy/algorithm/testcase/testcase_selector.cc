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
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include <vector>
#include <iostream>
#include <string>

#include "types.h"
#define private public
#include "testcase_utils.h"
#include "virtual_topo_stub.h"
#include "dev_capability.h"
#include "virtual_topo.h"
#include "virtual_topo_stub.h"
#include "orion_adapter_rts.h"

#include "coll_alg_params.h"
#include "coll_operator.h"
#include "execute_selector.h"
#include "base_selector.h"
#undef private

using namespace Hccl;
using namespace checker;

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
    {}

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        // 这边每个case执行完成需要清理所有的环境变量，如果有新增的环境变量，需要在这个函数中进行清理
        ClearHcclEnv();
    }

};

TEST_F(SelectorTest, TestAutoSelectorOneTimesFour)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));

    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095OneTimesFour(rankTable);

    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::CCU_MS;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    opAllReduce.dataCount = 1024 * 1024;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    std::string allReduceAlgName;
    params.dataSize = opAllReduce.dataCount * DataTypeSizeGet(opAllReduce.dataType);
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).SetRankSize(4).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "CcuAllReduceMesh1D");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    opAllGather.dataCount = 1024 * 1024;
    std::string allGatherAlgName;
    params.dataSize = opAllGather.dataCount * DataTypeSizeGet(opAllGather.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "CcuAllGatherMesh1D");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    opReduceScatter.dataCount = 1024 * 1024;
    std::string reduceScatterAlgName;
    params.dataSize = opReduceScatter.dataCount * DataTypeSizeGet(opReduceScatter.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "CcuReduceScatterMesh1D");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;

    CollAlgOperator opBroadcast;
    opBroadcast.opType = OpType::BROADCAST;
    opBroadcast.dataType = DataType::FP32;
    opBroadcast.dataCount = 1024 * 1024;
    std::string broadcastAlgName;
    params.dataSize = opBroadcast.dataCount * DataTypeSizeGet(opBroadcast.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opBroadcast, params, broadcastAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(broadcastAlgName, "CcuBroadcastMesh1D");
    std::cout << "The setted broadcast insCollAlgName: " << broadcastAlgName << std::endl;

    CollAlgOperator opReduce;
    opReduce.opType = OpType::REDUCE;
    opReduce.dataType = DataType::FP32;
    opReduce.dataCount = 1024 * 1024;
    std::string reduceAlgName;
    params.dataSize = opReduce.dataCount * DataTypeSizeGet(opReduce.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduce, params, reduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceAlgName, "CcuReduceMesh1D");
    std::cout << "The setted reduce insCollAlgName: " << reduceAlgName << std::endl;

    CollAlgOperator opScatter;
    opScatter.opType = OpType::SCATTER;
    opScatter.dataType = DataType::FP32;
    std::string scatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opScatter, params, scatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(scatterAlgName, "CcuScatterMesh1D");
    std::cout << "The setted scatter insCollAlgName: " << scatterAlgName << std::endl;

    CollAlgOperator opAlltoall;
    opAlltoall.opType = OpType::ALLTOALL;
    opAlltoall.dataType = DataType::FP32;
    std::string alltoallAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoall, params, alltoallAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallAlgName, "CcuAlltoAllMesh1D");

    CollAlgOperator opAlltoallv;
    opAlltoallv.opType = OpType::ALLTOALLV;
    opAlltoallv.dataType = DataType::FP32;
    std::string alltoallvAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoallv, params, alltoallvAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallvAlgName, "CcuAlltoAllVMesh1D");
    std::cout << "The setted alltoallv insCollAlgName: " << alltoallvAlgName << std::endl;

    std::cout << "The setted alltoall insCollAlgName: " << alltoallAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorOneTimesFourCcuSchedule)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));

    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095OneTimesFour(rankTable);

    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::CCU_SCHED;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    std::string allReduceAlgName;
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "CcuAllReduceMeshMem2Mem1D");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "CcuAllGatherMeshMem2Mem1D");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "CcuReduceScatterMeshMem2Mem1D");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;

    CollAlgOperator opBroadcast;
    opBroadcast.opType = OpType::BROADCAST;
    opBroadcast.dataType = DataType::FP32;
    std::string broadcastAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opBroadcast, params, broadcastAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(broadcastAlgName, "CcuBroadcastMeshMem2Mem1D");
    std::cout << "The setted broadcast insCollAlgName: " << broadcastAlgName << std::endl;

    CollAlgOperator opAlltoall;
    opAlltoall.opType = OpType::ALLTOALL;
    opAlltoall.dataType = DataType::FP32;
    std::string alltoallAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoall, params, alltoallAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallAlgName, "CcuAlltoAllMesh1D");

    CollAlgOperator opAlltoallv;
    opAlltoallv.opType = OpType::ALLTOALLV;
    opAlltoallv.dataType = DataType::FP32;
    std::string alltoallvAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoallv, params, alltoallvAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallvAlgName, "CcuAlltoAllVMesh1D");
    std::cout << "The setted alltoallv insCollAlgName: " << alltoallvAlgName << std::endl;
    std::cout << "The setted alltoall insCollAlgName: " << alltoallAlgName << std::endl;

    CollAlgOperator opAllGatherV;
    opAllGatherV.opType = OpType::ALLGATHERV;
    opAllGatherV.dataType = DataType::FP32;
    opAllGatherV.dataCount = 1024 * 1024;
    std::string allGatherVAlgName;
    params.dataSize = opAllGatherV.dataCount * DataTypeSizeGet(opAllGatherV.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGatherV, params, allGatherVAlgName);
    std::string hcclAllgatherVAlgo = "level0:fullmesh;level1:NA";
    std::cout << hcclAllgatherVAlgo << std::endl;
    EnvConfig::GetInstance().algoCfg.hcclAlgoConfig.value = SetHcclAlgoConfig(hcclAllgatherVAlgo);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGatherV, params, allGatherVAlgName);
}

TEST_F(SelectorTest, TopoInit91095TwoTimesTwoALLGATHERV)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));

    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095TwoTimesTwo(rankTable);
    OpExecuteConfig opConfig;
    
    opConfig.accState = AcceleratorState::CCU_SCHED;
    CollAlgOperator opAllGatherV;
    opAllGatherV.opType = OpType::ALLGATHERV;
    opAllGatherV.dataType = DataType::FP32;
    opAllGatherV.dataCount = 1024 * 1024;
    std::string allGatherVAlgName;
    CollAlgParams params;
    params.dataSize = opAllGatherV.dataCount * DataTypeSizeGet(opAllGatherV.dataType);
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGatherV, params, allGatherVAlgName);
    std::string hcclAllgatherVAlgo = "level0:fullmesh;level1:NA";
    std::cout << hcclAllgatherVAlgo << std::endl;
    EnvConfig::GetInstance().algoCfg.hcclAlgoConfig.value = SetHcclAlgoConfig(hcclAllgatherVAlgo);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGatherV, params, allGatherVAlgName);
}

TEST_F(SelectorTest, TestAutoSelectorOneTimesFourAiv)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));

    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095OneTimesFour(rankTable);

    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AIV;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    CollAlgParams params;
    params.dataSize = 1000;
    params.opExecuteConfig = opConfig;
    std::string allReduceAlgName;
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "AivAllReduceMesh1DOneShot");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "AivAllGatherMesh1D");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "AivReduceScatterMesh1D");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;

    CollAlgOperator opBroadcast;
    opBroadcast.opType = OpType::BROADCAST;
    opBroadcast.dataType = DataType::FP32;
    std::string broadcastAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opBroadcast, params, broadcastAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(broadcastAlgName, "AivBroadcastMesh1D");
    std::cout << "The setted broadcast insCollAlgName: " << broadcastAlgName << std::endl;

    CollAlgOperator opAlltoall;
    opAlltoall.opType = OpType::ALLTOALL;
    opAlltoall.dataType = DataType::FP32;
    std::string alltoallAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoall, params, alltoallAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallAlgName, "AivAlltoAllMesh1D");
    std::cout << "The setted alltoall insCollAlgName: " << alltoallAlgName << std::endl;

    CollAlgOperator opScatter;
    opScatter.opType = OpType::SCATTER;
    opScatter.dataType = DataType::FP32;
    std::string scatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opScatter, params, scatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(scatterAlgName, "AivScatterMesh1D");
    std::cout << "The setted scatter insCollAlgName: " << scatterAlgName << std::endl;

    CollAlgOperator opReduce;
    opReduce.opType = OpType::REDUCE;
    opReduce.dataType = DataType::FP32;
    std::string reduceAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduce, params, reduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceAlgName, "AivReduceMesh1D");
    std::cout << "The setted reduce insCollAlgName: " << reduceAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoTimesTwo)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));

    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095TwoTimesTwo(rankTable);
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::CCU_MS;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    opAllReduce.dataCount = 255 * 1024;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    std::string allReduceAlgName;
    params.dataSize = opAllReduce.dataCount * DataTypeSizeGet(opAllReduce.dataType);
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "CcuAllReduceMesh2DTwoShot");
    std::cout << "The setted insCollAlgName: " << allReduceAlgName << std::endl;

    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    opAllReduce.dataCount = 256 * 1024;
    params.opExecuteConfig = opConfig;
    params.dataSize = opAllReduce.dataCount * DataTypeSizeGet(opAllReduce.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "CcuAllReduceMesh2DTwoShot");
    std::cout << "The setted insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "CcuAllGatherMesh2D");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "CcuReduceScatterMesh2D");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;

    CollAlgOperator opBroadcast;
    opBroadcast.opType = OpType::BROADCAST;
    opBroadcast.dataType = DataType::FP32;
    opBroadcast.dataCount = 1024*255;
    std::string broadcastAlgName;
    params.dataSize = opBroadcast.dataCount * DataTypeSizeGet(opBroadcast.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opBroadcast, params, broadcastAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(broadcastAlgName, "CcuBroadcastMesh2D");
    std::cout << "The setted broadcast insCollAlgName: " << broadcastAlgName << std::endl;

    CollAlgOperator opBroadcast2d;
    opBroadcast2d.opType = OpType::BROADCAST;
    opBroadcast2d.dataType = DataType::FP32;
    opBroadcast2d.dataCount = 1024*1024;
    params.dataSize = opBroadcast2d.dataCount * DataTypeSizeGet(opBroadcast2d.dataType);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opBroadcast2d, params, broadcastAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(broadcastAlgName, "CcuBroadcastMesh2D");
    std::cout << "The setted broadcast insCollAlgName: " << broadcastAlgName << std::endl;

    CollAlgOperator opReduce;
    opReduce.opType = OpType::REDUCE;
    opReduce.dataType = DataType::FP32;
    std::string reduceAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduce, params, reduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceAlgName, "CcuReduceMesh2D");
    std::cout << "The setted reduce insCollAlgName: " << reduceAlgName << std::endl;

    CollAlgOperator opAlltoallv;
    opAlltoallv.opType = OpType::ALLTOALLV;
    opAlltoallv.dataType = DataType::FP32;
    std::string alltoallvAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoallv, params, alltoallvAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallvAlgName, "CcuAlltoAllVMesh2D");
    std::cout << "The setted alltoallv insCollAlgName: " << alltoallvAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoTimesTwoCcuSchedule)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));

    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095TwoTimesTwo(rankTable);
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::CCU_SCHED;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    opAllReduce.dataCount = 100 * 1024 * 1024;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    std::string allReduceAlgName;
    params.dataSize = opAllReduce.dataCount * DataTypeSizeGet(opAllReduce.dataType);
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "CcuAllReduceMeshTwoShotMem2Mem2D");
    std::cout << "The setted insCollAlgName: " << allReduceAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoTimesTwoAicpu)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));

    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095TwoTimesTwo(rankTable);

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;

    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    std::string allReduceAlgName;
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceMesh2DTwoShot");
    std::cout << "The setted insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherMesh2D");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterMesh2D");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;

    CollAlgOperator opScatter;
    opScatter.opType = OpType::SCATTER;
    opScatter.dataType = DataType::FP32;
    std::string scatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opScatter, params, scatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(scatterAlgName, "InsScatterMesh2D");
    std::cout << "The setted scatter insCollAlgName: " << scatterAlgName << std::endl;

    CollAlgOperator opBroadcast;
    opBroadcast.opType = OpType::BROADCAST;
    opBroadcast.dataType = DataType::FP32;
    std::string broadcastAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opBroadcast, params, broadcastAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(broadcastAlgName, "InsBroadcastMesh2DTwoShot");
    std::cout << "The setted broadcast insCollAlgName: " << broadcastAlgName << std::endl;

    CollAlgOperator opReduce;
    opReduce.opType = OpType::REDUCE;
    opReduce.dataType = DataType::FP32;
    std::string reduceAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduce, params, reduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceAlgName, "InsReduceMesh2D");
    std::cout << "The setted reduce insCollAlgName: " << reduceAlgName << std::endl;

    CollAlgOperator opSend;
    opSend.opType = OpType::SEND;
    opSend.dataType = DataType::FP32;
    std::string sendAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opSend, params, sendAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(sendAlgName, "InsSend");
    std::cout << "The setted send insCollAlgName: " << sendAlgName << std::endl;

    CollAlgOperator opRecv;
    opRecv.opType = OpType::RECV;
    opRecv.dataType = DataType::FP32;
    std::string recvAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opRecv, params, recvAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(recvAlgName, "InsRecv");
    std::cout << "The setted recv insCollAlgName: " << recvAlgName << std::endl;

    CollAlgOperator opAlltoall;
    opAlltoall.opType = OpType::ALLTOALL;
    opAlltoall.dataType = DataType::FP32;
    std::string alltoallAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoall, params, alltoallAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallAlgName, "InsAlltoAllMesh2D");
    std::cout << "The setted alltoall insCollAlgName: " << alltoallAlgName << std::endl;

    CollAlgOperator opAlltoallv;
    opAlltoallv.opType = OpType::ALLTOALLV;
    opAlltoallv.dataType = DataType::FP32;
    std::string alltoallvAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoallv, params, alltoallvAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallvAlgName, "InsAlltoAllvMesh");
    std::cout << "The setted alltoallv insCollAlgName: " << alltoallvAlgName << std::endl;

    CollAlgOperator batchSendRecv;
    batchSendRecv.opType = OpType::BATCHSENDRECV;
    batchSendRecv.dataType = DataType::FP32;
    std::string batchSendRecvAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(batchSendRecv, params, batchSendRecvAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(batchSendRecvAlgName, "InsBatchSendRecv");
    std::cout << "The setted batchSendRecv insCollAlgName: " << batchSendRecvAlgName << std::endl;

    std::string hcclAlgo = "level0:fullmesh;level1:NA";
    std::cout << hcclAlgo << std::endl;
    EnvConfig::GetInstance().algoCfg.hcclAlgoConfig.value = SetHcclAlgoConfig(hcclAlgo);
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherMesh2D");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoServerTimesTwoDefault)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(0);
    string rankTable = "test";
    virtTopo.TopoInit91095TwoServerTimesTwo(rankTable);

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;

    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    std::string allReduceAlgName;
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceParallelMesh1DNHR");
    std::cout << "The setted insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherParallelMesh1DNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterParallelMesh1DNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;

    CollAlgOperator opScatter;
    opScatter.opType = OpType::SCATTER;
    opScatter.dataType = DataType::FP32;
    std::string scatterAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opScatter, params, scatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(scatterAlgName, "InsScatterParallelMesh1DNHR");
    std::cout << "The setted scatter insCollAlgName: " << scatterAlgName << std::endl;

    CollAlgOperator opBroadcast;
    opBroadcast.opType = OpType::BROADCAST;
    opBroadcast.dataType = DataType::FP32;
    std::string broadcastAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opBroadcast, params, broadcastAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    std::cout << "The setted broadcast insCollAlgName: " << broadcastAlgName << std::endl;

    CollAlgOperator opReduce;
    opReduce.opType = OpType::REDUCE;
    opReduce.dataType = DataType::FP32;
    std::string reduceAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opReduce, params, reduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceAlgName, "InsReduceParallelMesh1DNHR");
    std::cout << "The setted reduce insCollAlgName: " << reduceAlgName << std::endl;

    CollAlgOperator opAlltoall;
    opAlltoall.opType = OpType::ALLTOALL;
    opAlltoall.dataType = DataType::FP32;
    std::string alltoallAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoall, params, alltoallAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallAlgName, "InsAlltoAllMesh");
    std::cout << "The setted alltoall insCollAlgName: " << alltoallAlgName << std::endl;

    CollAlgOperator opAlltoallv;
    opAlltoallv.opType = OpType::ALLTOALLV;
    opAlltoallv.dataType = DataType::FP32;
    std::string alltoallvAlgName;
    status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAlltoallv, params, alltoallvAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(alltoallvAlgName, "InsAlltoAllvMesh");
    std::cout << "The setted alltoallv insCollAlgName: " << alltoallvAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodFourTwoAndTwoTwoFirstPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(0);
    u32 myRank = 0;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodFourTwoAndTwoTwo(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherParallelMesh2DNHR算法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherParallelMesh2DNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceParallelMesh2DNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterParallelMesh2DNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodFourTwoAndTwoTwoSecondPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(9);
    u32 myRank = 9;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodFourTwoAndTwoTwo(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherParallelMesh2DNHR算法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status =
        ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherParallelMesh2DNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP32;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceParallelMesh2DNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterParallelMesh2DNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodIrregularEightAndIrregularFourFirstPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(0);
    u32 myRank = 0;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodIrregularEightAndIrregularFour(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherNHR算法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::INT16;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodIrregularEightAndIrregularFourSecondPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(9);
    u32 myRank = 9;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodIrregularEightAndIrregularFour(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherNHR算法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;

    CollAlgOperator opAllGather;

    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::INT16;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodFourTwoAndThreeFirstPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(0);
    u32 myRank = 0;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodFourTwoAndThree(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherNHR算法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status = ExecuteSelector().SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::UINT8;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodFourTwoAndThreeSecondPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(9);
    u32 myRank = 9;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodFourTwoAndThree(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherNHR算法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status =
        ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP16;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodThreeTwoAndThreeFirstPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(0);
    u32 myRank = 0;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodThreeTwoAndThree(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherParallelNHRNHR法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;
    
    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status =
        ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherParallelNHRNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::FP16;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceParallelNHRNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterParallelNHRNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}

TEST_F(SelectorTest, TestAutoSelectorTwoPodThreeTwoAndThreeSecondPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(8);
    u32 myRank = 8;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodThreeTwoAndThree(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherParallelMesh1DNHR法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;

    params.opExecuteConfig = opConfig;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status =
        ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherParallelMesh1DNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::UINT32;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceParallelMesh1DNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;

    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterParallelMesh1DNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;
}


TEST_F(SelectorTest, TestAutoSelectorTwoPodTwoTwoAndTwoTwoFirstPod)
{
    MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_950));
    VirtualTopoStub virtTopo(0);
    u32 myRank = 0;
    string rankTable = "test";
    virtTopo.TopoInit91095TwoPodTwoTwoAndTwoTwo(rankTable);
    // 满足非对称分级topo条件下，正确选择到InsAllGatherParallelMesh1DNHR法
    OpExecuteConfig opConfig;
    opConfig.accState = AcceleratorState::AICPU_TS;
    CollAlgParams params;
    params.opExecuteConfig = opConfig;

    CollAlgOperator opAllGather;
    opAllGather.opType = OpType::ALLGATHER;
    opAllGather.dataType = DataType::FP32;
    std::string allGatherAlgName;
    HcclResult status =
        ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllGather, params, allGatherAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allGatherAlgName, "InsAllGatherParallelMesh2DNHR");
    std::cout << "The setted allgather insCollAlgName: " << allGatherAlgName << std::endl;

    CollAlgOperator opAllReduce;
    opAllReduce.opType = OpType::ALLREDUCE;
    opAllReduce.dataType = DataType::INT8;
    std::string allReduceAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opAllReduce, params, allReduceAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(allReduceAlgName, "InsAllReduceParallelMesh2DNHR");
    std::cout << "The setted allreduce insCollAlgName: " << allReduceAlgName << std::endl;
    
    CollAlgOperator opReduceScatter;
    opReduceScatter.opType = OpType::REDUCESCATTER;
    opReduceScatter.dataType = DataType::FP32;
    std::string reduceScatterAlgName;
    status = ExecuteSelector().SetMyRank(myRank).SetVirtualTopo(&virtTopo).Run(opReduceScatter, params, reduceScatterAlgName);
    EXPECT_EQ(status, HcclResult::HCCL_SUCCESS);
    EXPECT_EQ(reduceScatterAlgName, "InsReduceScatterParallelMesh2DNHR");
    std::cout << "The setted reducescatter insCollAlgName: " << reduceScatterAlgName << std::endl;

}
