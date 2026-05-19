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

#include <type_traits>
#include <utility>

#define private public
#define protected public
#include "coll_reduce_scatter_ring_layered_executor.h"
#include "reduce_scatter_layered.h"
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

std::unique_ptr<TopoMatcher> BuildReduceScatterLayeredTopoMatcher(bool withLayeredPlanes)
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

class ReduceScatterLayeredExecutorProbe : public CollReduceScatterRingLayeredExecutor {
public:
    explicit ReduceScatterLayeredExecutorProbe(std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollReduceScatterRingLayeredExecutor(nullptr, topoMatcher)
    {
    }

    using CollReduceScatterRingLayeredExecutor::CalcResRequest;
};

template <typename...>
struct MakeVoid { using type = void; };

template <typename... Ts>
using VoidT = typename MakeVoid<Ts...>::type;

template <typename T, typename = void>
struct HasTemplateCopyIn : std::false_type {};

template <typename T>
struct HasTemplateCopyIn<T, VoidT<decltype(std::declval<T &>().CopyIn(
    std::declval<const OpParam &>(), std::declval<hccl::ExecMem &>(), std::declval<const LayeredParam &>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct HasTemplateCopyOut : std::false_type {};

template <typename T>
struct HasTemplateCopyOut<T, VoidT<decltype(std::declval<T &>().CopyOut(
    std::declval<const OpParam &>(), std::declval<hccl::ExecMem &>(), std::declval<const LayeredParam &>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct HasExecutorCopyIn : std::false_type {};

template <typename T>
struct HasExecutorCopyIn<T, VoidT<decltype(std::declval<T &>().CopyIn(
    std::declval<const OpParam &>(), std::declval<const ExecMem &>(), std::declval<const HcomCollOpInfo *>(),
    std::declval<const std::vector<std::vector<Slice>> &>(), std::declval<const std::vector<std::vector<Slice>> &>(),
    std::declval<u32>(), std::declval<std::vector<std::vector<Slice>> &>()))>> : std::true_type {};

template <typename T, typename = void>
struct HasExecutorCopyOut : std::false_type {};

template <typename T>
struct HasExecutorCopyOut<T, VoidT<decltype(std::declval<T &>().CopyOut(
    std::declval<const OpParam &>(), std::declval<const ExecMem &>(), std::declval<u32>(), std::declval<u32>(),
    std::declval<u32>(), std::declval<const HcomCollOpInfo *>()))>> : std::true_type {};

class ReduceScatterLayeredProbe : public ReduceScatterLayered {
public:
    explicit ReduceScatterLayeredProbe() : ReduceScatterLayered(nullptr, 0)
    {
    }

    using ReduceScatterLayered::Prepare;
    using ReduceScatterLayered::SetOuterReduceScatterContext;
    using ReduceScatterLayered::CopyIn;
    using ReduceScatterLayered::CopyOut;
    using ReduceScatterLayered::GetOuterUserMemSlices;
    using ReduceScatterLayered::RunAsync;
    using ReduceScatterLayered::RunInnerReduceScatter;
    using ReduceScatterLayered::RunCrossReduceScatter;
};

class RecordingReduceScatterLayeredProbe : public ReduceScatterLayeredProbe {
public:
    std::vector<std::string> phaseOrder;

protected:
    HcclResult RunInnerReduceScatter() override
    {
        phaseOrder.push_back("Inner");
        return HCCL_SUCCESS;
    }

    HcclResult RunCrossReduceScatter() override
    {
        phaseOrder.push_back("Cross");
        return HCCL_SUCCESS;
    }
};

class ReduceScatterLayeredExecutorApiTest : public BaseInit {
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
        param.tag = "reduce_scatter_layered_executor_ut";
        param.opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
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
        layeredParam.outerCommInfo = SubCommInfo{0U, 2U};
        layeredParam.innerCommInfo = SubCommInfo{0U, 2U};
        layeredParam.crossCommInfo = SubCommInfo{0U, 2U};
        layeredParam.innerDataSlices = {{0U, 8U}, {8U, 8U}};
        layeredParam.crossDataSlices = {{8U, 4U}, {12U, 4U}};
        return layeredParam;
    }
};
} // namespace

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_CalcResRequest_When_LayeredReduceScatterSelected_Expect_RequestLevel0LayeredLevel1AndLayeredLevel2)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildReduceScatterLayeredTopoMatcher(true);
    ASSERT_NE(topoMatcher, nullptr);

    ReduceScatterLayeredExecutorProbe executor(topoMatcher);
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

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_CalcResRequest_When_LayeredPlanesAbsent_Expect_NoPhantomLayeredTransportRequests)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildReduceScatterLayeredTopoMatcher(false);
    ASSERT_NE(topoMatcher, nullptr);

    ReduceScatterLayeredExecutorProbe executor(topoMatcher);
    ASSERT_EQ(executor.SetAlgType(BuildLayeredExecutorAlgType()), HCCL_SUCCESS);
    ASSERT_EQ(executor.SetCCLInBuffer(TEST_LAYERED_CCL_BUFFER_SIZE), HCCL_SUCCESS);

    AlgResourceRequest resourceRequest;
    ASSERT_EQ(executor.CalcResRequest(BuildOpParam(), resourceRequest), HCCL_SUCCESS);

    EXPECT_FALSE(resourceRequest.opTransport[COMM_LEVEL0].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LAYERED_LEVEL1].empty());
    EXPECT_TRUE(resourceRequest.opTransport[COMM_LAYERED_LEVEL2].empty());
}

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_LayeredRunAsync_When_Invoked_Expect_InnerThenCrossPhaseOrder)
{
    RecordingReduceScatterLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    int input[4] = {0};
    int output[4] = {0};
    int scratch[4] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.count = 2U;

    LayeredParam layeredParam = BuildLayeredParam();
    ASSERT_EQ(layered.RunAsync(param, execMem, layeredParam), HCCL_SUCCESS);

    EXPECT_EQ(layered.phaseOrder, (std::vector<std::string>{"Inner", "Cross"}));
}

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_TemplateCopySeamShape_When_TemplateContractParityLands_Expect_TemplateOwnsCopyInCopyOut)
{
    EXPECT_FALSE(HasExecutorCopyIn<ReduceScatterLayeredExecutorProbe>::value);
    EXPECT_FALSE(HasExecutorCopyOut<ReduceScatterLayeredExecutorProbe>::value);
    EXPECT_TRUE(HasTemplateCopyIn<ReduceScatterLayeredProbe>::value);
    EXPECT_TRUE(HasTemplateCopyOut<ReduceScatterLayeredProbe>::value);
}

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_TemplateCopyIn_When_PathBContextMovedInward_Expect_TemplateProducesOuterUserMemSlices)
{
    ReduceScatterLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    int input[16] = {0};
    int output[4] = {0};
    int scratch[16] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.count = 4U;

    LayeredParam layeredParam = BuildLayeredParam();
    std::vector<std::vector<Slice>> multiStreamSlice = {{{0U, 16U}, {16U, 16U}}};
    std::vector<std::vector<Slice>> outerSlices = {{{0U, 16U}, {16U, 16U}, {32U, 16U}, {48U, 16U}}};
    layered.SetOuterReduceScatterContext(nullptr, multiStreamSlice, outerSlices, sizeof(int));

    ASSERT_EQ(layered.Prepare(param, execMem, layeredParam), HCCL_SUCCESS);
    ASSERT_EQ(layered.CopyIn(param, execMem, layeredParam), HCCL_SUCCESS);

    const auto &userSlices = layered.GetOuterUserMemSlices();
    ASSERT_EQ(userSlices.size(), 1U);
    ASSERT_EQ(userSlices[0].size(), outerSlices[0].size());
    EXPECT_EQ(userSlices[0][0].offset, outerSlices[0][0].offset);
    EXPECT_EQ(userSlices[0][1].offset, outerSlices[0][1].offset);
}

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_TemplateRunAsync_When_TemplateContractParityLands_Expect_NoImplicitPrepareRequired)
{
    RecordingReduceScatterLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    LayeredParam layeredParam = BuildLayeredParam();

    ASSERT_EQ(layered.RunAsync(param, execMem, layeredParam), HCCL_SUCCESS);
    EXPECT_EQ(layered.phaseOrder, (std::vector<std::string>{"Inner", "Cross"}));
}

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_LayeredRunInnerReduceScatter_When_CommSizeOne_Expect_Success)
{
    ReduceScatterLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    int input[4] = {0};
    int output[4] = {0};
    int scratch[4] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.count = 2U;

    LayeredParam layeredParam = BuildLayeredParam();
    layeredParam.innerCommInfo.localRankSize = 1U;
    ASSERT_EQ(layered.Prepare(param, execMem, layeredParam), HCCL_SUCCESS);
    EXPECT_EQ(layered.RunInnerReduceScatter(), HCCL_SUCCESS);
}

TEST_F(ReduceScatterLayeredExecutorApiTest,
    Ut_LayeredRunCrossReduceScatter_When_CommSizeOne_Expect_Success)
{
    ReduceScatterLayeredProbe layered;
    OpParam param = BuildOpParam();
    ExecMem execMem;
    int input[4] = {0};
    int output[4] = {0};
    int scratch[4] = {0};
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.count = 2U;

    LayeredParam layeredParam = BuildLayeredParam();
    layeredParam.crossCommInfo.localRankSize = 1U;
    ASSERT_EQ(layered.Prepare(param, execMem, layeredParam), HCCL_SUCCESS);
    EXPECT_EQ(layered.RunCrossReduceScatter(), HCCL_SUCCESS);
}
