/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"

#define private public
#define protected public
#include "coll_all_reduce_ring_layered_executor.h"
#include "all_reduce_layered.h"
#undef protected
#undef private

namespace {
using namespace hccl;
constexpr u32 TEST_LAYERED_RANK_SIZE = 4U;
constexpr u64 TEST_LAYERED_CCL_BUFFER_SIZE = 64U * 1024U * 1024U;

AlgType BuildLayeredExecutorAlgType(AlgTypeLevel1 level1 = AlgTypeLevel1::ALG_LEVEL1_RING,
    AlgTypeLevel2 level2 = AlgTypeLevel2::ALG_LEVEL2_RING)
{
    return AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING, level1, level2);
}

std::unique_ptr<TopoMatcher> BuildAllReduceLayeredTopoMatcher(bool withLayeredPlanes)
{
    std::vector<std::vector<std::vector<u32>>> commPlaneRanks(static_cast<u32>(COMM_LEVEL_RESERVED));
    commPlaneRanks[COMM_LEVEL0] = {{{0U, 1U, 2U, 3U}}};
    commPlaneRanks[COMM_LEVEL1] = {{{0U, 2U}, {1U, 3U}}};
    commPlaneRanks[COMM_LEVEL2] = {{{1U, 3U}}};
    if (withLayeredPlanes) {
        commPlaneRanks[COMM_LAYERED_LEVEL1] = {{{0U, 2U}, {1U, 3U}}};
        commPlaneRanks[COMM_LAYERED_LEVEL2] = {{{1U, 3U}}};
    }

    std::vector<bool> isBridgeVector;
    HcclTopoInfo topoInfo;
    topoInfo.userRank = 1U;
    topoInfo.realUserRank = 1U;
    topoInfo.userRankSize = TEST_LAYERED_RANK_SIZE;
    topoInfo.serverNum = 1U;
    topoInfo.superPodNum = 1U;
    topoInfo.deviceNumPerAggregation = 1U;
    topoInfo.deviceType = DevType::DEV_TYPE_910_93;
    topoInfo.topoType = TopoType::TOPO_TYPE_NP_SINGLE_RING;
    HcclAlgoInfo algoInfo;
    HcclExternalEnable externalEnable;
    std::vector<std::vector<std::vector<u32>>> serverAndSuperPodToRank;
    return std::unique_ptr<TopoMatcher>(new (std::nothrow) TopoMatcher(commPlaneRanks, isBridgeVector,
        topoInfo, algoInfo, externalEnable, serverAndSuperPodToRank));
}

class AllReduceLayeredExecutorProbe : public CollAllReduceRingLayeredExecutor {
public:
    explicit AllReduceLayeredExecutorProbe(std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollAllReduceRingLayeredExecutor(nullptr, topoMatcher)
    {
    }

    using CollAllReduceRingLayeredExecutor::CalcResRequest;
};

class AllReduceLayeredProbe : public AllReduceLayered {
public:
    explicit AllReduceLayeredProbe() : AllReduceLayered(nullptr, 0)
    {
    }

    using AllReduceLayered::Prepare;
    using AllReduceLayered::RunCrossAllReduce;
    using AllReduceLayered::RunInnerAllGather;
    using AllReduceLayered::RunInnerReduceScatter;
};

class AllReduceLayeredExecutorApiTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        ASSERT_EQ(SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE), HCCL_SUCCESS);
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        ASSERT_EQ(SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE), HCCL_SUCCESS);
        BaseInit::TearDown();
    }

protected:
    OpParam BuildOpParam() const
    {
        OpParam param;
        param.tag = "all_reduce_layered_executor_ut";
        param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
        param.DataDes.count = 1024U;
        param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
        param.reduceType = HCCL_REDUCE_SUM;
        return param;
    }

    static LayeredParam BuildLayeredParam()
    {
        LayeredParam layeredParam;
        layeredParam.count = 2U;
        layeredParam.algType = BuildLayeredExecutorAlgType();
        layeredParam.topoAttr.userRank = 1U;
        layeredParam.topoAttr.userRankSize = TEST_LAYERED_RANK_SIZE;
        layeredParam.interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_RING;
        layeredParam.outerCommInfo = SubCommInfo{0U, 4U};
        layeredParam.innerCommInfo = SubCommInfo{0U, 2U};
        layeredParam.crossCommInfo = SubCommInfo{0U, 2U};
        layeredParam.outerDataSlices = {{0U, 8U}, {8U, 8U}, {16U, 8U}, {24U, 8U}};
        return layeredParam;
    }
};
} // namespace

TEST_F(AllReduceLayeredExecutorApiTest,
    Ut_CalcResRequest_When_LayeredAllReduceSelected_Expect_RequestLevel0LayeredLevel1AndLayeredLevel2)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildAllReduceLayeredTopoMatcher(true);
    ASSERT_NE(topoMatcher, nullptr);

    AllReduceLayeredExecutorProbe executor(topoMatcher);
    ASSERT_EQ(executor.SetAlgType(BuildLayeredExecutorAlgType()), HCCL_SUCCESS);
    ASSERT_EQ(executor.SetCCLInBuffer(TEST_LAYERED_CCL_BUFFER_SIZE), HCCL_SUCCESS);

    AlgResourceRequest resourceRequest;
    ASSERT_EQ(executor.CalcResRequest(BuildOpParam(), resourceRequest), HCCL_SUCCESS);

    ASSERT_EQ(resourceRequest.opTransport.size(), static_cast<u32>(COMM_LEVEL_RESERVED));
    EXPECT_FALSE(resourceRequest.opTransport[COMM_LEVEL0].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LEVEL1].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LEVEL2].empty());
    EXPECT_FALSE(resourceRequest.opTransport[COMM_LAYERED_LEVEL1].empty());
    EXPECT_FALSE(resourceRequest.opTransport[COMM_LAYERED_LEVEL2].empty());
}

TEST_F(AllReduceLayeredExecutorApiTest,
    Ut_CalcResRequest_When_LayeredPlanesAbsent_Expect_NoPhantomLayeredTransportRequests)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildAllReduceLayeredTopoMatcher(false);
    ASSERT_NE(topoMatcher, nullptr);

    AllReduceLayeredExecutorProbe executor(topoMatcher);
    ASSERT_EQ(executor.SetAlgType(BuildLayeredExecutorAlgType()), HCCL_SUCCESS);
    ASSERT_EQ(executor.SetCCLInBuffer(TEST_LAYERED_CCL_BUFFER_SIZE), HCCL_SUCCESS);

    AlgResourceRequest resourceRequest;
    ASSERT_EQ(executor.CalcResRequest(BuildOpParam(), resourceRequest), HCCL_SUCCESS);

    EXPECT_FALSE(resourceRequest.opTransport[COMM_LEVEL0].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LAYERED_LEVEL1].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LAYERED_LEVEL2].empty());
}

TEST_F(AllReduceLayeredExecutorApiTest,
    Ut_LayeredRunInnerReduceScatter_When_CommSizeOne_Expect_Success)
{
    AllReduceLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    int input[8] = {0};
    int output[8] = {0};
    int scratch[8] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.count = 2U;

    LayeredParam layeredParam = BuildLayeredParam();
    layeredParam.innerCommInfo.localRankSize = 1U;
    ASSERT_EQ(layered.Prepare(param, execMem, layeredParam), HCCL_SUCCESS);
    EXPECT_EQ(layered.RunInnerReduceScatter(), HCCL_SUCCESS);
}

TEST_F(AllReduceLayeredExecutorApiTest,
    Ut_LayeredRunCrossAllReduce_When_CommSizeOne_Expect_Success)
{
    AllReduceLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    int input[8] = {0};
    int output[8] = {0};
    int scratch[8] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.count = 2U;

    LayeredParam layeredParam = BuildLayeredParam();
    layeredParam.crossCommInfo.localRankSize = 1U;
    ASSERT_EQ(layered.Prepare(param, execMem, layeredParam), HCCL_SUCCESS);
    EXPECT_EQ(layered.RunCrossAllReduce(), HCCL_SUCCESS);
}
