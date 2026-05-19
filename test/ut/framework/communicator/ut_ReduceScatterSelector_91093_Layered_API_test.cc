/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "reduce_scatter_operator.h"
#undef protected
#undef private
#include "workflow_pub.h"

namespace {
using namespace hccl;

constexpr u32 TEST_RANK_SIZE = 4;
constexpr u64 TEST_CCL_BUFFER_SIZE = 64 * 1024 * 1024;
constexpr u64 TEST_RS_COUNT = 2 * 1024 * 1024;

AlgType BuildRsAlgType(AlgTypeLevel1 level1)
{
    return AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING, level1);
}

AlgType BuildWholeRingAlgType()
{
    return AlgType(AlgTypeLevel0::ALG_LEVEL0_WHOLE_RING, AlgTypeLevel1::ALG_LEVEL1_WHOLE_RING);
}

class ReduceScatterSelector91093LayeredTest : public testing::Test {
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
    std::string Select91093(bool isOxcMode, u32 groupSize, AlgType algType,
        TopoType topoType = TopoType::TOPO_TYPE_NP_SINGLE_RING)
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
        topoAttr.isOxcMode = isOxcMode;
        topoAttr.groupSize = groupSize;

        AlgConfigurator algConfigurator(algoAttr, topoAttr);
        CCLBufferManager cclBufferManager;
        EXPECT_EQ(cclBufferManager.InitCCLbuffer(TEST_CCL_BUFFER_SIZE, TEST_CCL_BUFFER_SIZE), HCCL_SUCCESS);

        std::vector<std::vector<std::vector<u32>>> commPlaneRanks(static_cast<u32>(COMM_LEVEL_RESERVED));
        commPlaneRanks[COMM_LEVEL0] = {{{0, 1, 2, 3}}};
        commPlaneRanks[COMM_LEVEL1] = {{{0, 2}, {1, 3}}};
        commPlaneRanks[COMM_LEVEL2] = {{{1, 3}}};
        if (isOxcMode && groupSize > 1) {
            commPlaneRanks[COMM_LAYERED_LEVEL1] = {{{0, 2}, {1, 3}}};
            commPlaneRanks[COMM_LAYERED_LEVEL2] = {{{1, 3}}};
        }
        std::vector<bool> isBridgeVector;
        HcclTopoInfo topoInfo;
        topoInfo.userRank = 1U;
        topoInfo.realUserRank = 1U;
        topoInfo.userRankSize = TEST_RANK_SIZE;
        topoInfo.deviceType = DevType::DEV_TYPE_910_93;
        HcclAlgoInfo algoInfo;
        HcclExternalEnable externalEnable;
        std::vector<std::vector<std::vector<u32>>> serverAndSuperPodToRank;
        std::unique_ptr<TopoMatcher> topoMatcher(new (std::nothrow) TopoMatcher(commPlaneRanks, isBridgeVector,
            topoInfo, algoInfo, externalEnable, serverAndSuperPodToRank));
        EXPECT_NE(topoMatcher, nullptr);
        if (topoMatcher == nullptr) {
            return "";
        }

        ReduceScatterOperator op(&algConfigurator, cclBufferManager, nullptr, topoMatcher);
        op.algType_ = algType;
        op.topoType_ = topoType;
        op.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
        op.userRankSize_ = TEST_RANK_SIZE;
        op.serverNum_ = 1;
        op.superPodNum_ = 1;
        op.deviceNumPerAggregation_ = 1;
        op.multiModuleDiffDeviceNumMode_ = false;
        op.retryEnable_ = false;

        OpParam param;
        param.opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
        param.DataDes.count = TEST_RS_COUNT;
        param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
        param.reduceType = HCCL_REDUCE_RESERVED;

        std::string algName;
        ResourceLimit limit;
        EXPECT_EQ(op.SelectAlgfor91093(param, algName, limit), HCCL_SUCCESS);
        return algName;
    }
};

TEST_F(ReduceScatterSelector91093LayeredTest,
    Ut_SelectAlgfor91093_When_OxcGroupReadyAndOrdinaryRing_Expect_LayeredExecutor)
{
    EXPECT_EQ(Select91093(true, 2, BuildRsAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)),
        "ReduceScatterRingLayeredExecutor");
}

TEST_F(ReduceScatterSelector91093LayeredTest,
    Ut_SelectAlgfor91093_When_NonOxcOrdinaryRing_Expect_OriginalRingExecutor)
{
    EXPECT_EQ(Select91093(false, 2, BuildRsAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)),
        "ReduceScatterRingFor91093Executor");
}

TEST_F(ReduceScatterSelector91093LayeredTest,
    Ut_SelectAlgfor91093_When_DefaultOrSingleGroupSize_Expect_OriginalRingExecutor)
{
    EXPECT_EQ(Select91093(true, 0, BuildRsAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)),
        "ReduceScatterRingFor91093Executor");
    EXPECT_EQ(Select91093(true, 1, BuildRsAlgType(AlgTypeLevel1::ALG_LEVEL1_RING)),
        "ReduceScatterRingFor91093Executor");
}

TEST_F(ReduceScatterSelector91093LayeredTest,
    Ut_SelectAlgfor91093_When_LayeredPlanesMissing_Expect_OriginalRingExecutor)
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
    topoAttr.groupSize = 2;

    AlgConfigurator algConfigurator(algoAttr, topoAttr);
    CCLBufferManager cclBufferManager;
    ASSERT_EQ(cclBufferManager.InitCCLbuffer(TEST_CCL_BUFFER_SIZE, TEST_CCL_BUFFER_SIZE), HCCL_SUCCESS);

    std::vector<std::vector<std::vector<u32>>> commPlaneRanks(static_cast<u32>(COMM_LEVEL_RESERVED));
    commPlaneRanks[COMM_LEVEL0] = {{{0, 1, 2, 3}}};
    std::vector<bool> isBridgeVector;
    HcclTopoInfo topoInfo;
    topoInfo.userRank = 1U;
    topoInfo.realUserRank = 1U;
    topoInfo.userRankSize = TEST_RANK_SIZE;
    topoInfo.deviceType = DevType::DEV_TYPE_910_93;
    HcclAlgoInfo algoInfo;
    HcclExternalEnable externalEnable;
    std::vector<std::vector<std::vector<u32>>> serverAndSuperPodToRank;
    std::unique_ptr<TopoMatcher> topoMatcher(new (std::nothrow) TopoMatcher(commPlaneRanks, isBridgeVector,
        topoInfo, algoInfo, externalEnable, serverAndSuperPodToRank));
    ASSERT_NE(topoMatcher, nullptr);

    ReduceScatterOperator op(&algConfigurator, cclBufferManager, nullptr, topoMatcher);
    op.algType_ = BuildRsAlgType(AlgTypeLevel1::ALG_LEVEL1_RING);
    op.topoType_ = TopoType::TOPO_TYPE_NP_SINGLE_RING;
    op.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    op.userRankSize_ = TEST_RANK_SIZE;
    op.serverNum_ = 1;
    op.superPodNum_ = 1;
    op.deviceNumPerAggregation_ = 1;
    op.multiModuleDiffDeviceNumMode_ = false;
    op.retryEnable_ = false;

    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    param.DataDes.count = TEST_RS_COUNT;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
    param.reduceType = HCCL_REDUCE_RESERVED;

    std::string algName;
    ResourceLimit limit;
    ASSERT_EQ(op.SelectAlgfor91093(param, algName, limit), HCCL_SUCCESS);
    EXPECT_EQ(algName, "ReduceScatterRingFor91093Executor");
}

TEST_F(ReduceScatterSelector91093LayeredTest,
    Ut_SelectAlgfor91093_When_UnsupportedLevel1_Expect_OriginalRingExecutor)
{
    EXPECT_EQ(Select91093(true, 2, BuildWholeRingAlgType()), "ReduceScatterRingFor91093Executor");
}

TEST_F(ReduceScatterSelector91093LayeredTest,
    Ut_SelectAlgfor91093_When_DoubleRingCandidate_Expect_NoLayeredReplacement)
{
    EXPECT_EQ(Select91093(true, 2, BuildRsAlgType(AlgTypeLevel1::ALG_LEVEL1_RING),
        TopoType::TOPO_TYPE_NP_DOUBLE_RING), "ReduceScatterFastDoubleRingFor91093Executor");
}
} // namespace
