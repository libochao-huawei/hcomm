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

#include <chrono>
#include <memory>
#include <vector>

#define private public
#define protected public
#include "coll_all_gather_ring_layered_executor.h"
#include "all_gather_layered.h"
#include "transport.h"
#include "transport_base.h"
#undef protected
#undef private

namespace {
using namespace hccl;

constexpr u32 TEST_RANK_SIZE = 4U;
constexpr u64 TEST_COUNT = 2U;
constexpr u32 TEST_SQ_HEAD = 0U;
constexpr u32 TEST_SQ_TAIL = 100U;
u8 g_sqAddr[HCCL_SQE_SIZE * HCCL_SQE_MAX_CNT] = {0};

std::unique_ptr<TopoMatcher> BuildTopoMatcher()
{
    std::vector<std::vector<std::vector<u32>>> commPlaneRanks(static_cast<u32>(COMM_LEVEL_RESERVED));
    commPlaneRanks[COMM_LEVEL0] = {{{0U, 1U, 2U, 3U}}};
    commPlaneRanks[COMM_LEVEL1] = {{{0U, 2U}, {1U, 3U}}};
    commPlaneRanks[COMM_LEVEL2] = {{{0U, 1U}}};
    commPlaneRanks[COMM_LAYERED_LEVEL1] = {{{0U, 2U}, {1U, 3U}}};
    commPlaneRanks[COMM_LAYERED_LEVEL2] = {{{0U, 1U}}};

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

std::unique_ptr<NotifyPool> &DummyNotifyPool()
{
    static std::unique_ptr<NotifyPool> notifyPool = nullptr;
    return notifyPool;
}

MachinePara &DummyMachinePara()
{
    static MachinePara machinePara{};
    return machinePara;
}

class DummyTransportBase final : public TransportBase {
public:
    DummyTransportBase() : TransportBase(nullptr, DummyNotifyPool(), DummyMachinePara(), std::chrono::milliseconds(0)) {}

    HcclResult TxAsync(std::vector<TxMemoryInfo>& txMems, Stream &stream) override
    {
        (void)txMems;
        (void)stream;
        return HCCL_SUCCESS;
    }

    HcclResult RxAsync(std::vector<RxMemoryInfo>& rxMems, Stream &stream) override
    {
        (void)rxMems;
        (void)stream;
        return HCCL_SUCCESS;
    }

    HcclResult TxAck(Stream &stream) override
    {
        (void)stream;
        return HCCL_SUCCESS;
    }

    HcclResult RxAck(Stream &stream) override
    {
        (void)stream;
        return HCCL_SUCCESS;
    }

    HcclResult TxWaitDone(Stream &stream) override
    {
        (void)stream;
        return HCCL_SUCCESS;
    }

    HcclResult RxWaitDone(Stream &stream) override
    {
        (void)stream;
        return HCCL_SUCCESS;
    }
};

LINK BuildDummyLink()
{
    return std::make_shared<Transport>(new (std::nothrow) DummyTransportBase());
}

SingleSubCommTransport BuildSingleSubCommTransport(const std::vector<u32> &userRanks, u32 selfUserRank)
{
    SingleSubCommTransport transport;
    transport.transportRequests.resize(userRanks.size());
    transport.links.resize(userRanks.size());
    transport.status.resize(userRanks.size(), TransportStatus::READY);
    for (u32 i = 0; i < userRanks.size(); ++i) {
        transport.userRank2subCommRank[userRanks[i]] = i;
        transport.subCommRank2UserRank[i] = userRanks[i];
        transport.links[i] = BuildDummyLink();
        transport.transportRequests[i].isValid = true;
        transport.transportRequests[i].localUserRank = selfUserRank;
        transport.transportRequests[i].remoteUserRank = userRanks[i];
    }
    return transport;
}

AlgResourceResponse BuildAlgResourceResponse()
{
    AlgResourceResponse algRes;
    algRes.opTransportResponse.resize(static_cast<u32>(COMM_LEVEL_RESERVED));
    algRes.opTransportResponse[COMM_LEVEL0].push_back(BuildSingleSubCommTransport({0U, 1U, 2U, 3U}, 1U));
    algRes.opTransportResponse[COMM_LAYERED_LEVEL1].push_back(BuildSingleSubCommTransport({0U, 2U}, 1U));
    algRes.opTransportResponse[COMM_LAYERED_LEVEL1].push_back(BuildSingleSubCommTransport({1U, 3U}, 1U));
    algRes.opTransportResponse[COMM_LAYERED_LEVEL2].push_back(BuildSingleSubCommTransport({0U, 1U}, 1U));
    return algRes;
}

HcclResult HcclD2DMemcpyAsyncStub(HcclDispatcher, DeviceMem &dst, const DeviceMem &src, Stream &)
{
    CHK_PTR_NULL(dst.ptr());
    CHK_PTR_NULL(src.ptr());
    const size_t copySize = static_cast<size_t>(std::min(dst.size(), src.size()));
    const errno_t ret = memcpy_s(dst.ptr(), copySize, src.ptr(), copySize);
    return (ret == EOK) ? HCCL_SUCCESS : HCCL_E_INTERNAL;
}

class FakeDispatcher final : public DispatcherPub {
public:
    explicit FakeDispatcher() : DispatcherPub(0) {}

    HcclResult Init() override
    {
        return HCCL_SUCCESS;
    }

    HcclResult MemcpyAsync(DeviceMem &dst, const DeviceMem &src, Stream &stream,
        u32 remoteUserRank = INVALID_VALUE_RANKID,
        hccl::LinkType inLinkType = hccl::LinkType::LINK_ONCHIP) override
    {
        (void)stream;
        (void)remoteUserRank;
        (void)inLinkType;
        return HcclD2DMemcpyAsyncStub(nullptr, dst, src, stream);
    }
};

Stream BuildTestStream()
{
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 128;
    streamInfo.sqBaseAddr = static_cast<void *>(g_sqAddr);
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, true);
    static SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(TEST_SQ_HEAD, TEST_SQ_TAIL, &sqeCqeCtx);
    return stream;
}

class AllGatherCorrectnessExecutorProbe : public CollAllGatherRingLayeredExecutor {
public:
    explicit AllGatherCorrectnessExecutorProbe(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollAllGatherRingLayeredExecutor(dispatcher, topoMatcher) {}

    using CollAllGatherRingLayeredExecutor::KernelRun;
    using CollAllGatherRingLayeredExecutor::algResResp_;
    using CollAllGatherRingLayeredExecutor::workflowMode_;
    using CollAllGatherRingLayeredExecutor::tag_;

    std::vector<std::string> evidence;

protected:
    HcclResult ActiveSlaveStreamsForTest(const Stream &stream) override
    {
        (void)stream;
        evidence.push_back("active-slave-streams");
        return HCCL_SUCCESS;
    }

    HcclResult MultiRingAllGatherForTest(const std::string &, DeviceMem, DeviceMem outputMem, u64,
        HcclDataType, const std::vector<std::vector<Slice>> &, Stream, s32) override
    {
        evidence.push_back("outer-ag");
        int *buffer = static_cast<int *>(outputMem.ptr());
        int values[32] = {
            21,22,31,32,41,42,51,52,
            61,62,71,72,81,82,91,92,
            101,102,111,112,121,122,131,132,
            141,142,151,152,161,162,171,172
        };
        for (int i = 0; i < 32; ++i) {
            buffer[i] = values[i];
        }
        return HCCL_SUCCESS;
    }
};

class AllGatherLayeredCorrectnessApiTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        ASSERT_EQ(SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE), HCCL_SUCCESS);
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
        MOCKER(HcclD2DMemcpyAsync).stubs().with(any(), any(), any(), any()).will(invoke(HcclD2DMemcpyAsyncStub));
    }

    void TearDown() override
    {
        ASSERT_EQ(SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE), HCCL_SUCCESS);
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }

protected:
    OpParam BuildOpParam() const
    {
        OpParam param;
        param.tag = "all_gather_layered_correctness_ut";
        param.opType = HcclCMDType::HCCL_CMD_ALLGATHER;
        param.DataDes.count = TEST_COUNT;
        param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
        param.DataDes.strideCount = 0U;
        param.supportZeroCopy = false;
        param.aicpuUnfoldMode = false;
        return param;
    }
};
} // namespace

TEST_F(AllGatherLayeredCorrectnessApiTest,
    Ut_KernelRun_When_CanonicalNonDegenerateCase_Expect_FinalOutputPtrPermutationVector)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildTopoMatcher();
    ASSERT_NE(topoMatcher, nullptr);

    FakeDispatcher dispatcher;
    ASSERT_EQ(dispatcher.Init(), HCCL_SUCCESS);
    AllGatherCorrectnessExecutorProbe executor(reinterpret_cast<HcclDispatcher>(&dispatcher), topoMatcher);
    ASSERT_EQ(executor.SetAlgType(AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING,
        AlgTypeLevel1::ALG_LEVEL1_RING, AlgTypeLevel2::ALG_LEVEL2_RING)), HCCL_SUCCESS);
    executor.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    executor.tag_ = "ag_correctness";

    AlgResourceResponse algRes = BuildAlgResourceResponse();
    executor.algResResp_ = &algRes;

    int input[2] = {21,22};
    int output[32] = {0};
    int scratch[2] = {0};
    int userOutput[32] = {0};

    ExecMem execMem;
    execMem.count = TEST_COUNT;
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.inputPtr = input;
    execMem.outputPtr = userOutput;

    OpParam param = BuildOpParam();
    param.stream = BuildTestStream();
    ASSERT_EQ(executor.KernelRun(param, execMem), HCCL_SUCCESS);

    const SubCommInfo outer = executor.GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);
    const SubCommInfo inner = executor.GetSubCommInfo(COMM_LAYERED_LEVEL1, outer.localRank);
    const SubCommInfo cross = executor.GetSubCommInfo(COMM_LAYERED_LEVEL2, COMM_INDEX_0);
    EXPECT_EQ(outer.localRankSize, 4U);
    EXPECT_EQ(inner.localRankSize, 2U);
    EXPECT_EQ(cross.localRankSize, 2U);

    EXPECT_EQ(executor.evidence, (std::vector<std::string>{"active-slave-streams", "outer-ag"}));

    const int expected[32] = {
        21,22,61,62,101,102,141,142,
        41,42,81,82,121,122,161,162,
        31,32,71,72,111,112,151,152,
        51,52,91,92,131,132,171,172
    };
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(userOutput[i], expected[i]);
    }
}
