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
#include "coll_all_gather_ring_layered_executor.h"
#include "all_gather_layered.h"
#include "all_gather_operator.h"
#undef protected
#undef private

namespace {
using namespace hccl;
constexpr u32 TEST_LAYERED_RANK_SIZE = 4U;
constexpr u64 TEST_LAYERED_CCL_BUFFER_SIZE = 64U * 1024U * 1024U;
constexpr u64 TEST_LAYERED_COUNT = 2U * 1024U * 1024U;

AlgType BuildLayeredExecutorAlgType(AlgTypeLevel1 level1 = AlgTypeLevel1::ALG_LEVEL1_RING,
    AlgTypeLevel2 level2 = AlgTypeLevel2::ALG_LEVEL2_RING)
{
    return AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING, level1, level2);
}

std::unique_ptr<TopoMatcher> BuildAllGatherLayeredTopoMatcher(bool withLayeredPlanes)
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

class AllGatherLayeredExecutorProbe : public CollAllGatherRingLayeredExecutor {
public:
    explicit AllGatherLayeredExecutorProbe(std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollAllGatherRingLayeredExecutor(nullptr, topoMatcher)
    {
    }

    using CollAllGatherRingLayeredExecutor::CalcResRequest;
};

class RecordingAllGatherLayeredProbe : public AllGatherLayered {
public:
    RecordingAllGatherLayeredProbe() : AllGatherLayered(nullptr)
    {
    }

    using AllGatherLayered::CopyIn;
    using AllGatherLayered::CopyOut;
    using AllGatherLayered::Prepare;
    using AllGatherLayered::RunAsync;
    using AllGatherLayered::localCopyOffset_;

    std::vector<std::string> phaseOrder;

protected:
    HcclResult RunCrossAllGather() override
    {
        phaseOrder.push_back("Cross");
        return HCCL_SUCCESS;
    }

    HcclResult RunInnerAllGather() override
    {
        phaseOrder.push_back("Inner");
        return HCCL_SUCCESS;
    }
};

class AllGatherLayeredExecutorApiTest : public testing::Test {
public:
    void SetUp() override
    {
        ASSERT_EQ(SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE), HCCL_SUCCESS);
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        ASSERT_EQ(SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE), HCCL_SUCCESS);
        GlobalMockObject::verify();
    }

protected:
    OpParam BuildOpParam() const
    {
        OpParam param;
        param.tag = "all_gather_layered_executor_ut";
        param.opType = HcclCMDType::HCCL_CMD_ALLGATHER;
        param.DataDes.count = TEST_LAYERED_COUNT;
        param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
        param.supportZeroCopy = false;
        param.aicpuUnfoldMode = false;
        return param;
    }

    std::unique_ptr<AllGatherOperator> BuildOperator(std::unique_ptr<TopoMatcher> &topoMatcher, bool isOxcMode)
    {
        topoAttr_.serverNum = 1U;
        topoAttr_.superPodNum = 1U;
        topoAttr_.moduleNum = 1U;
        topoAttr_.deviceNumPerServer = TEST_LAYERED_RANK_SIZE;
        topoAttr_.deviceNumPerAggregation = 1U;
        topoAttr_.userRankSize = TEST_LAYERED_RANK_SIZE;
        topoAttr_.deviceType = DevType::DEV_TYPE_910_93;
        topoAttr_.isOxcMode = isOxcMode;

        algConfigurator_.reset(new (std::nothrow) AlgConfigurator(algoAttr_, topoAttr_));
        EXPECT_NE(algConfigurator_, nullptr);
        EXPECT_EQ(cclBufferManager_.InitCCLbuffer(TEST_LAYERED_CCL_BUFFER_SIZE, TEST_LAYERED_CCL_BUFFER_SIZE), HCCL_SUCCESS);

        std::unique_ptr<AllGatherOperator> op(
            new (std::nothrow) AllGatherOperator(algConfigurator_.get(), cclBufferManager_, nullptr, topoMatcher));
        EXPECT_NE(op, nullptr);
        if (op == nullptr) {
            return nullptr;
        }
        op->algType_ = BuildLayeredExecutorAlgType();
        op->topoType_ = TopoType::TOPO_TYPE_NP_SINGLE_RING;
        op->workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
        op->userRankSize_ = TEST_LAYERED_RANK_SIZE;
        op->serverNum_ = 1U;
        op->superPodNum_ = 1U;
        op->deviceNumPerAggregation_ = 1U;
        op->multiModuleDiffDeviceNumMode_ = false;
        op->retryEnable_ = false;
        return op;
    }

    LayeredParam BuildLayeredParam() const
    {
        LayeredParam layeredParam;
        layeredParam.count = 1U;
        layeredParam.algType = BuildLayeredExecutorAlgType();
        layeredParam.topoAttr.userRank = 1U;
        layeredParam.topoAttr.userRankSize = TEST_LAYERED_RANK_SIZE;
        layeredParam.interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_RING;
        layeredParam.outerCommInfo = SubCommInfo{1U, 2U};
        layeredParam.innerCommInfo = SubCommInfo{0U, 2U};
        layeredParam.crossCommInfo = SubCommInfo{1U, 2U};
        layeredParam.outerDataSlices = {{0U, 16U}, {16U, 16U}};
        return layeredParam;
    }

private:
    HcclAlgoAttr algoAttr_;
    HcclTopoAttr topoAttr_;
    std::unique_ptr<AlgConfigurator> algConfigurator_;
    CCLBufferManager cclBufferManager_;
};
} // namespace

TEST_F(AllGatherLayeredExecutorApiTest,
    Ut_CalcResRequest_When_LayeredAllGatherSelected_Expect_RequestLevel0LayeredLevel1AndLayeredLevel2)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildAllGatherLayeredTopoMatcher(true);
    ASSERT_NE(topoMatcher, nullptr);

    AllGatherLayeredExecutorProbe executor(topoMatcher);
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

TEST_F(AllGatherLayeredExecutorApiTest,
    Ut_CalcResRequest_When_LayeredPlanesAbsent_Expect_NoPhantomLayeredTransportRequests)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildAllGatherLayeredTopoMatcher(false);
    ASSERT_NE(topoMatcher, nullptr);

    AllGatherLayeredExecutorProbe executor(topoMatcher);
    ASSERT_EQ(executor.SetAlgType(BuildLayeredExecutorAlgType()), HCCL_SUCCESS);
    ASSERT_EQ(executor.SetCCLInBuffer(TEST_LAYERED_CCL_BUFFER_SIZE), HCCL_SUCCESS);

    AlgResourceRequest resourceRequest;
    ASSERT_EQ(executor.CalcResRequest(BuildOpParam(), resourceRequest), HCCL_SUCCESS);

    ASSERT_EQ(resourceRequest.opTransport.size(), static_cast<u32>(COMM_LEVEL_RESERVED));
    EXPECT_FALSE(resourceRequest.opTransport[COMM_LEVEL0].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LEVEL1].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LEVEL2].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LAYERED_LEVEL1].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LAYERED_LEVEL2].empty());
}

TEST_F(AllGatherLayeredExecutorApiTest,
    Ut_LayeredRunAsync_When_Invoked_Expect_CrossThenInnerPhaseOrder)
{
    RecordingAllGatherLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    int input[1] = {7};
    int output[8] = {0};
    int scratch[1] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.inputPtr = input;
    execMem.outputPtr = output;
    execMem.count = 1U;

    LayeredParam layeredParam = BuildLayeredParam();
    ASSERT_EQ(layered.Prepare(param, execMem, layeredParam), HCCL_SUCCESS);
    ASSERT_EQ(layered.RunAsync(param, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(layered.phaseOrder, (std::vector<std::string>{"Cross", "Inner"}));
}

TEST_F(AllGatherLayeredExecutorApiTest,
    Ut_LayeredCopyContractAndRunAsync_When_UsingLayeredBuffers_Expect_CopyInCopyOutAndCrossThenInnerOrder)
{
    RecordingAllGatherLayeredProbe layered;

    OpParam param = BuildOpParam();
    param.DataDes.count = 1U;

    ExecMem execMem;
    int input[1] = {99};
    int output[8] = {10, 11, 12, 13, 14, 15, 16, 17};
    int scratch[1] = {0};
    int userOutput[8] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.inputPtr = input;
    execMem.outputPtr = userOutput;
    execMem.count = 1U;

    LayeredParam layeredParam = BuildLayeredParam();
    ASSERT_EQ(layered.Prepare(param, execMem, layeredParam), HCCL_SUCCESS);

    constexpr size_t kOutputCount = sizeof(output) / sizeof(output[0]);
    const u64 localIndex = layered.localCopyOffset_ / sizeof(int);
    ASSERT_LT(localIndex, static_cast<u64>(kOutputCount));
    EXPECT_EQ(output[localIndex], 15);

    MOCKER(HcclD2DMemcpyAsync).stubs().with(any(), any(), any(), any()).will(returnValue(HCCL_SUCCESS));

    ASSERT_EQ(layered.CopyIn(param, execMem, layeredParam), HCCL_SUCCESS);
    ASSERT_EQ(layered.RunAsync(param, execMem, layeredParam), HCCL_SUCCESS);
    ASSERT_EQ(layered.CopyOut(param, execMem, layeredParam), HCCL_SUCCESS);
    EXPECT_EQ(layered.phaseOrder, (std::vector<std::string>{"Cross", "Inner"}));
    GlobalMockObject::verify();
}
