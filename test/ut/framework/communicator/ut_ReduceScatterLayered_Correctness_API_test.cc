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
#include "coll_reduce_scatter_ring_layered_executor.h"
#include "dispatcher_aicpu.h"
#include "reduce_scatter_layered.h"
#include "transport.h"
#include "transport_base.h"
#undef protected
#undef private

namespace {
using namespace hccl;

constexpr u32 TEST_RANK_SIZE = 4U;
constexpr u64 TEST_CCL_BUFFER_SIZE = 64U * 1024U * 1024U;
constexpr u64 TEST_COUNT = 8U;
constexpr u32 TEST_SQ_HEAD = 0U;
constexpr u32 TEST_SQ_TAIL = 100U;
u8 g_sqAddr[HCCL_SQE_SIZE * HCCL_SQE_MAX_CNT] = {0};

AlgType BuildLayeredExecutorAlgType()
{
    return AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING,
        AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel2::ALG_LEVEL2_RING);
}

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
    DummyTransportBase() : TransportBase(nullptr, DummyNotifyPool(), DummyMachinePara(), std::chrono::milliseconds(0))
    {
    }

    HcclResult TxAsync(std::vector<TxMemoryInfo>& txMems, Stream &stream) override
    {
        (void)stream;
        (void)txMems;
        return HCCL_SUCCESS;
    }

    HcclResult RxAsync(std::vector<RxMemoryInfo>& rxMems, Stream &stream) override
    {
        (void)stream;
        (void)rxMems;
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

    HcclResult InlineReduceAsync(const void *src, u64 count, const HcclDataType datatype, HcclReduceOp redOp,
        Stream &stream, void *dst, u32 remoteUserRank = INVALID_VALUE_RANKID,
        hccl::LinkType inLinkType = hccl::LinkType::LINK_ONCHIP) override
    {
        (void)remoteUserRank;
        (void)inLinkType;
        return ReduceIntoDst(src, dst, count, datatype, redOp, stream);
    }

    HcclResult ReduceAsync(const void *src, void *dst, u64 dataCount, const HcclDataType datatype,
        HcclReduceOp redOp, Stream &stream, HcclReduceType reduceType = HcclReduceType::HCCL_TBE_REDUCE) override
    {
        (void)reduceType;
        return ReduceIntoDst(src, dst, dataCount, datatype, redOp, stream);
    }

private:
    HcclResult ReduceIntoDst(const void *src, void *dst, u64 dataCount, const HcclDataType datatype,
        HcclReduceOp redOp, Stream &stream)
    {
        (void)stream;
        if (datatype != HCCL_DATA_TYPE_INT32 || redOp != HCCL_REDUCE_SUM) {
            return HCCL_E_NOT_SUPPORT;
        }
        auto *srcPtr = static_cast<const int *>(src);
        auto *dstPtr = static_cast<int *>(dst);
        CHK_PTR_NULL(srcPtr);
        CHK_PTR_NULL(dstPtr);
        for (u64 i = 0; i < dataCount; ++i) {
            dstPtr[i] += srcPtr[i];
        }
        return HCCL_SUCCESS;
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

class ReduceScatterCorrectnessExecutorProbe : public CollReduceScatterRingLayeredExecutor {
public:
    explicit ReduceScatterCorrectnessExecutorProbe(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollReduceScatterRingLayeredExecutor(dispatcher, topoMatcher)
    {
    }

    using CollReduceScatterRingLayeredExecutor::KernelRun;
    using CollReduceScatterRingLayeredExecutor::algResResp_;
    using CollReduceScatterRingLayeredExecutor::workflowMode_;
    using CollReduceScatterRingLayeredExecutor::tag_;

    std::vector<std::string> evidence;

protected:
    HcclResult ActiveSlaveStreamsForTest(const Stream &stream) override
    {
        (void)stream;
        evidence.push_back("active-slave-streams");
        return HCCL_SUCCESS;
    }

    HcclResult MultiRingReduceScatterForTest(const std::string &tag, DeviceMem inputMem, DeviceMem outputMem,
        u64 count, HcclDataType dataType, HcclReduceOp reductionOp,
        const std::vector<std::vector<Slice>> &multRingsSliceZero, Stream stream, s32 profStage,
        u64 baseOffset, const HcomCollOpInfo *opInfo,
        const std::vector<std::vector<Slice>> &multRingsUserMemSlice) override
    {
        (void)tag;
        (void)inputMem;
        (void)count;
        (void)dataType;
        (void)reductionOp;
        (void)multRingsSliceZero;
        (void)stream;
        (void)profStage;
        (void)baseOffset;
        (void)opInfo;
        (void)multRingsUserMemSlice;
        evidence.push_back("outer-rs");
        (void)outputMem;
        int *inputSeed = static_cast<int *>(inputMem.ptr());
        const int values[8] = {1111,1112,1113,1114,1115,1116,1117,1118};
        for (int i = 0; i < 8; ++i) {
            inputSeed[8 + i] = values[i];
        }
        return HCCL_SUCCESS;
    }
};

class ReduceScatterLayeredCorrectnessApiTest : public BaseInit {
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
        param.tag = "reduce_scatter_layered_correctness_ut";
        param.opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
        param.DataDes.count = TEST_COUNT;
        param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
        param.reduceType = HCCL_REDUCE_SUM;
        return param;
    }
};
} // namespace

TEST_F(ReduceScatterLayeredCorrectnessApiTest,
    Ut_KernelRun_When_CanonicalNonDegenerateCase_Expect_FinalOutputPtr1111And1112)
{
    std::unique_ptr<TopoMatcher> topoMatcher = BuildTopoMatcher();
    ASSERT_NE(topoMatcher, nullptr);

    FakeDispatcher dispatcher;
    ASSERT_EQ(dispatcher.Init(), HCCL_SUCCESS);
    ReduceScatterCorrectnessExecutorProbe executor(reinterpret_cast<HcclDispatcher>(&dispatcher), topoMatcher);
    ASSERT_EQ(executor.SetAlgType(BuildLayeredExecutorAlgType()), HCCL_SUCCESS);
    executor.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    executor.tag_ = "rs_correctness";

    AlgResourceResponse algRes = BuildAlgResourceResponse();
    executor.algResResp_ = &algRes;
    EXPECT_EQ(algRes.opTransportResponse.size(), static_cast<u32>(COMM_LEVEL_RESERVED));
    EXPECT_EQ(algRes.opTransportResponse[COMM_LEVEL0].size(), 1U);
    EXPECT_EQ(algRes.opTransportResponse[COMM_LAYERED_LEVEL1].size(), 2U);
    EXPECT_EQ(algRes.opTransportResponse[COMM_LAYERED_LEVEL2].size(), 1U);

    int input[TEST_COUNT * TEST_RANK_SIZE] = {0};
    for (int i = 0; i < static_cast<int>(TEST_COUNT); ++i) {
        input[i] = 100 + i;
    }
    int output[TEST_COUNT] = {0};
    int scratch[TEST_COUNT * TEST_RANK_SIZE] = {0};
    int userOutput[TEST_COUNT] = {0};

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

    EXPECT_EQ(executor.evidence.size(), 2U);
    EXPECT_EQ(executor.evidence[0], "active-slave-streams");
    EXPECT_EQ(executor.evidence[1], "outer-rs");

    const SubCommInfo outer = executor.GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);
    const SubCommInfo inner = executor.GetSubCommInfo(COMM_LAYERED_LEVEL1, outer.localRank);
    const SubCommInfo cross = executor.GetSubCommInfo(COMM_LAYERED_LEVEL2, COMM_INDEX_0);
    EXPECT_EQ(outer.localRankSize, 4U);
    EXPECT_EQ(inner.localRankSize, 2U);
    EXPECT_EQ(cross.localRankSize, 2U);

    const int expected[8] = {1111,1112,1113,1114,1115,1116,1117,1118};
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(userOutput[i], expected[i]);
    }
}
