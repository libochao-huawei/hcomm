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
#include "all_reduce_operator.h"
#undef protected
#undef private

namespace {
using namespace hccl;
constexpr u32 TEST_RANK_SIZE = 4U;
constexpr u64 TEST_CCL_BUFFER_SIZE = 64U * 1024U * 1024U;
constexpr u64 TEST_AR_COUNT = 1024U;

std::unique_ptr<TopoMatcher> BuildTopoMatcher(bool withLayeredPlanes)
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
    topoInfo.userRankSize = TEST_RANK_SIZE;
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

class AllReduceLayeredCommunicatorApiTest : public testing::Test {
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
};

TEST_F(AllReduceLayeredCommunicatorApiTest,
    Ut_CommunicatorLayeredRoute_When_OxcOrdinaryRingReady_Expect_LayeredExecutorAndLayeredCommRequests)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    topoAttr.serverNum = 1;
    topoAttr.superPodNum = 1;
    topoAttr.moduleNum = 1;
    topoAttr.deviceNumPerServer = TEST_RANK_SIZE;
    topoAttr.deviceNumPerAggregation = 1;
    topoAttr.userRankSize = TEST_RANK_SIZE;
    topoAttr.deviceType = DevType::DEV_TYPE_910_93;
    topoAttr.isOxcMode = true;

    AlgConfigurator algConfigurator(algoAttr, topoAttr);
    CCLBufferManager cclBufferManager;
    ASSERT_EQ(cclBufferManager.InitCCLbuffer(TEST_CCL_BUFFER_SIZE, TEST_CCL_BUFFER_SIZE), HCCL_SUCCESS);

    std::unique_ptr<TopoMatcher> topoMatcher = BuildTopoMatcher(true);
    ASSERT_NE(topoMatcher, nullptr);

    AllReduceOperator op(&algConfigurator, cclBufferManager, nullptr, topoMatcher);
    op.algType_ = AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING, AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel2::ALG_LEVEL2_RING);
    op.topoType_ = TopoType::TOPO_TYPE_NP_SINGLE_RING;
    op.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    op.userRankSize_ = TEST_RANK_SIZE;
    op.serverNum_ = 1;
    op.superPodNum_ = 1;
    op.deviceNumPerAggregation_ = 1;
    op.multiModuleDiffDeviceNumMode_ = false;
    op.retryEnable_ = false;

    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.tag = "allreduce_layered_comm_ut";
    param.DataDes.count = TEST_AR_COUNT;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
    param.reduceType = HCCL_REDUCE_SUM;

    std::string algName;
    ASSERT_EQ(op.SelectAlgfor91093(param, algName), HCCL_SUCCESS);
    ASSERT_EQ(algName, "AllReduceRingLayeredExecutor");

    AlgResourceRequest resourceRequest;
    ASSERT_EQ(op.CalcResRequest(algName, param, resourceRequest), HCCL_SUCCESS);
    EXPECT_FALSE(resourceRequest.opTransport[COMM_LEVEL0].empty());
    EXPECT_FALSE(resourceRequest.opTransport[COMM_LAYERED_LEVEL1].empty());
    EXPECT_FALSE(resourceRequest.opTransport[COMM_LAYERED_LEVEL2].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LEVEL1].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LEVEL2].empty());
}

TEST_F(AllReduceLayeredCommunicatorApiTest,
    Ut_CommunicatorLayeredRoute_When_LayeredPlanesMissing_Expect_OrdinaryRingExecutor)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    topoAttr.serverNum = 1;
    topoAttr.superPodNum = 1;
    topoAttr.moduleNum = 1;
    topoAttr.deviceNumPerServer = TEST_RANK_SIZE;
    topoAttr.deviceNumPerAggregation = 1;
    topoAttr.userRankSize = TEST_RANK_SIZE;
    topoAttr.deviceType = DevType::DEV_TYPE_910_93;
    topoAttr.isOxcMode = true;

    AlgConfigurator algConfigurator(algoAttr, topoAttr);
    CCLBufferManager cclBufferManager;
    ASSERT_EQ(cclBufferManager.InitCCLbuffer(TEST_CCL_BUFFER_SIZE, TEST_CCL_BUFFER_SIZE), HCCL_SUCCESS);

    std::unique_ptr<TopoMatcher> topoMatcher = BuildTopoMatcher(false);
    ASSERT_NE(topoMatcher, nullptr);

    AllReduceOperator op(&algConfigurator, cclBufferManager, nullptr, topoMatcher);
    op.algType_ = AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING, AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel2::ALG_LEVEL2_RING);
    op.topoType_ = TopoType::TOPO_TYPE_NP_SINGLE_RING;
    op.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    op.userRankSize_ = TEST_RANK_SIZE;
    op.serverNum_ = 1;
    op.superPodNum_ = 1;
    op.deviceNumPerAggregation_ = 1;
    op.multiModuleDiffDeviceNumMode_ = false;
    op.retryEnable_ = false;

    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.tag = "allreduce_non_layered_comm_ut";
    param.DataDes.count = TEST_AR_COUNT;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
    param.reduceType = HCCL_REDUCE_SUM;

    std::string algName;
    ASSERT_EQ(op.SelectAlgfor91093(param, algName), HCCL_SUCCESS);
    EXPECT_EQ(algName, "AllReduceRingFor91093Executor");
}
} // namespace
