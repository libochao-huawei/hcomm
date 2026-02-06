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
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "dispatcher_graph.h"
#include "op_json_info.h"
#include "op_tiling.h"
#include "vector_tiling.h"
#include "auto_tiling_rt2.h"
#include "vector_op_info.h"
#include "auto_tiling_register.h"
#include "vector_tiling_handle.h"
#include "vector_tiling_rt2.h"
#include "fusion.h"
#include "auto_tiling_context.h"
#include "profiling_manager.h"
#include "dlprof_function.h"
#include "profiler_manager.h"
#include "externalinput.h"
#include "adapter_rts.h"
#include "ffts_common_pub.h"
#include "heartbeat.h"
#include "graph_ctx_mgr.h"
#include "ffts_common.h"
#include "legacy_common.h"
#include "ffts_ctx_provider.h"
#include "graph_ctx_mgr_common.h"
#include "dispatcher_graph_pub.h"
#include "ffts_ctx_provider.h"
#undef private
#undef protected

using namespace hccl;
using namespace std;

HcclResult InitTask_stub(HcclDispatcher dispatcherPtr, hccl::Stream &stream, const bool enableCache,
    const std::string &key, bool useGraphConstructorV2)
{
    CHK_PTR_NULL(dispatcherPtr);
    CHK_RET(reinterpret_cast<DispatcherPub *>(dispatcherPtr)->ResetGraphCtx(enableCache, key, false));
    return HCCL_SUCCESS;
}

class DispatcherFFTSTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "DispatcherGraph test SetUP" << std::endl;
        MOCKER_CPP(&Heartbeat::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::RegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::UnRegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    }
    static void TearDownTestCase()
    {
        std::cout << "DispatcherGraph test TearDown" << std::endl;
        GlobalMockObject::verify();
    }
    virtual void SetUp()
    {
        std::cout << "DispatcherGraph test caseSetUP" << std::endl;
        MOCKER(InitTask).stubs().will(invoke(InitTask_stub));
        MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));    
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        fftsDispatcher = reinterpret_cast<DispatcherGraph*>(dispatcherPtr);
    }
    virtual void TearDown()
    {
        std::cout << "DispatcherGraph test case TearDown" << std::endl;
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            fftsDispatcher = nullptr;
        }
        GlobalMockObject::verify();
    }
public:
    HcclDispatcher dispatcherPtr;
    DispatcherGraph *fftsDispatcher;
};

TEST_F(DispatcherFFTSTest, ut_launch_task_before_reset_context) {
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> subStreams;
    auto ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    EXPECT_EQ(nullptr, fftsCtxsPtr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(DispatcherFFTSTest, ut_signalRecord_should_related_with_a_signal_wait_given_in_chip) {
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    u32 *notifyIdOut = new u32(0);
    MOCKER(GetNotifyID)
    .stubs()
    .with(any(), outBound(notifyIdOut))
    .will(returnValue(HCCL_SUCCESS));

    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    auto meta = HcclOpMetaInfo::GetOneForAllGather();

    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "should_related_with_a_signal");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    hccl::Stream stream;
    u32 userRank = 1;
    u64 offset = 0;
    s32 stage = 0;
    u64 signalAddr = INVALID_U64;
    auto ret = fftsDispatcher->SignalRecord(signal, stream, userRank, offset, stage, true, signalAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    auto notifyId = fftsCtxsPtr->ignoredSuccessorStart.back();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    EXPECT_EQ(0, notifyId);
    GlobalMockObject::verify();
    delete notifyIdOut;
}

TEST_F(DispatcherFFTSTest, ut_signalRecord_should_create_an_ctx_given_not_in_chip) {
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    u64 *notifyAddr = new u64(0);
    MOCKER(NotifyGetAddr)
    .stubs()
    .with(any(), outBound(notifyAddr))
    .will(returnValue(HCCL_SUCCESS));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "should_create_an_ctx_given_not_in_chip");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    Stream stream;
    u32 userRank = 1;
    u64 offset = 0;
    s32 stage = 0;
    u64 signalAddr = INVALID_U64;
    auto ret = fftsDispatcher->SignalRecord(signal, stream, userRank, offset, stage, false, signalAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    auto refreshIndex = fftsCtxsPtr->refreshIndex;
    auto ctxType = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex - 1].contextType;

    EXPECT_EQ(1, refreshIndex);
    EXPECT_EQ(RT_CTX_TYPE_WRITE_VALUE, ctxType);
    delete notifyAddr;
}

TEST_F(DispatcherFFTSTest, ut_signalWait_should_add_into_unassigned_successor_liut_given_in_chip) {
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    MOCKER_CPP(&DispatcherGraph::GetNotifyDfxInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    u32 *notifyIdOut = new u32(0);
    MOCKER(GetNotifyID)
    .stubs()
    .with(any(), outBound(notifyIdOut))
    .will(returnValue(HCCL_SUCCESS));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "successor_liut_given_in_chip");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    Stream stream;

    auto ret = fftsDispatcher->SignalWait(signal, stream, 0, 0, 0, true);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    auto notifyId = fftsCtxsPtr->unassignedSuccessorEnd[0].back();

    EXPECT_EQ(0, notifyId);
    delete notifyIdOut;
}

TEST_F(DispatcherFFTSTest, ut_signalWait_should_create_an_ctx_given_not_in_chip) {
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    u32 *notifyIdOut = new u32(0); 
    MOCKER(GetNotifyID)
    .stubs()
    .with(any(), outBound(notifyIdOut))
    .will(returnValue(HCCL_SUCCESS));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "create_an_ctx_given_not_in_chip");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    Stream stream;

    auto ret = fftsDispatcher->SignalWait(signal, stream, 0, 0, 0, false);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    auto refreshIndex = fftsCtxsPtr->refreshIndex;
    auto ctxType = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex - 1].contextType;

    EXPECT_EQ(1, refreshIndex);
    EXPECT_EQ(RT_CTX_TYPE_NOTIFY_WAIT, ctxType);
    delete notifyIdOut;
}

TEST_F(DispatcherFFTSTest, ut_memcpyAsync_should_create_an_ctx_given_init_case) {
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "should_create_an_ctx_given_init_case");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    u64 *ptr = new u64(1);
    DeviceMem dst(ptr, 1, false);
    DeviceMem src(ptr, 1, false);
    Stream stream;
    auto ret = fftsDispatcher->MemcpyAsync(dst, src, stream);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    auto ctxType = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex - 1].contextType;

    EXPECT_EQ(HCCL_SUCCESS, ret);
    EXPECT_EQ(1280, ctxType);
    delete ptr;
}

TEST_F(DispatcherFFTSTest, should_increase_one_for_refresh_index_given_skip_ffts_refresh) {
    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "given_skip_ffts_refresh");
    EXPECT_EQ(HCCL_SUCCESS, retCode);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    auto refreshIndexBefore = fftsCtxsPtr->refreshIndex;
    s32 streamId = 0;
    GraphMgr::GraphCtxMgr graphCtxMgr;
    graphCtxMgr.SkipFftsRefresh(streamId, fftsCtxsPtr);
    auto refreshIndexAfter = fftsCtxsPtr->refreshIndex;
    EXPECT_EQ(refreshIndexBefore + 1, refreshIndexAfter);
}

TEST_F(DispatcherFFTSTest, should_return_success_given_the_ffts_task_is_complete_when_launch_task) {
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    MOCKER(HcclCheckLogLevel)
    .stubs()
    .will(returnValue(true));

    MOCKER(rtGeneralCtrl)
    .stubs()
    .with(any(), any(), eq(RT_GNL_CTRL_TYPE_FFTS_PLUS))
    .will(returnValue(RT_ERROR_NONE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "complete_when_launch_task");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    DeviceMem dst = DeviceMem::alloc(80);
    DeviceMem src = DeviceMem::alloc(80);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->MemcpyAsync(dst, src, stream);

    std::vector<Stream> subStreams;
    auto ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->completed = true;
    fftsCtxsPtr->refreshIndex = 1;
    fftsCtxsPtr->ctxNum = 2;
    ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    fftsCtxsPtr->completed = true;
    fftsCtxsPtr->refreshIndex = 0;
    fftsCtxsPtr->ctxNum = 0;
    ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    EXPECT_EQ(HCCL_SUCCESS, ret); 

    rtFftsPlusSqe_t fftsPlusSqe = {};
    fftsPlusSqe.fftsType = 4;
    fftsPlusSqe.totalContextNum = 0;
    fftsPlusSqe.readyContextNum = 1;
    fftsPlusSqe.preloadContextNum = 128;
    u64 debug = 0;
    MOCKER(GetExterInputDebugConfigLegacy)
    .stubs()
    .will(returnValue(debug));
    GraphMgr::GraphCtxMgr graphCtxMgr;
    ret = graphCtxMgr.ConstructFftsSqe(fftsPlusSqe, fftsCtxsPtr);
    EXPECT_EQ(HCCL_SUCCESS, ret);   
}

TEST_F(DispatcherFFTSTest, should_return_error_given_the_ffts_task_ctxnum_exceeds_the_limit) {
    DlProfFunction::GetInstance().DlProfFunctionStubInit();
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    MOCKER(HcclCheckLogLevel)
    .stubs()
    .will(returnValue(true));

    MOCKER(rtGeneralCtrl)
    .stubs()
    .with(any(), any(), eq(RT_GNL_CTRL_TYPE_FFTS_PLUS))
    .will(returnValue(RT_ERROR_NONE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "ctxnum_exceeds_the_limit");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    DeviceMem dst = DeviceMem::alloc(80);
    DeviceMem src = DeviceMem::alloc(80);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->completed = true;
    fftsCtxsPtr->refreshIndex = 65536;
    fftsCtxsPtr->ctxNum = 65536;
    std::vector<Stream> subStreams;
    HcclResult ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    EXPECT_EQ(HCCL_E_NOT_SUPPORT, ret);
}

TEST_F(DispatcherFFTSTest, should_return_success_given_init_and_refresh_rdma_send_context)
{
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "refresh_rdma_send_context");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    u32 dbIndex = 0;
    u64 dbInfo = 0;
    u32 userRank;
    u64 offset = 0;
    struct send_wr wr;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank);
    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank, offset);

    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->completed = true;
    fftsCtxsPtr->refreshIndex = 0;

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank);
    auto ret = fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank, offset);   
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(DispatcherFFTSTest, should_return_success_given_init_and_refresh_rdma_send_context_capture)
{
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "rdma_send_context_capture");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    u32 dbIndex = 0;
    u64 dbInfo = 0;
    u32 userRank;
    u64 offset = 0;
    struct send_wr wr;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank, true);
    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank, offset, true);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->completed = true;
    fftsCtxsPtr->refreshIndex = 0;

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank, true);
    auto ret = fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank, offset, true);   
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(DispatcherFFTSTest, should_return_success_given_init_sdma_reduce_context)
{
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "init_sdma_reduce_context");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    DeviceMem dst = DeviceMem::alloc(80);
    DeviceMem src = DeviceMem::alloc(80);

    u64 dataCount = 20;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    auto ret = fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream, HcclReduceType::HCCL_INLINE_REDUCE);

    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(DispatcherFFTSTest, should_construct_label_ctx_given_the_dbindex_is_invalid_when_init_rdma_ctx)
{
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "invalid_when_init_rdma_ctx");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    u32 dbIndex = INVALID_UINT;
    u64 dbInfo = INVALID_U64;
    u32 userRank;
    u64 offset = 0;
    struct send_wr wr;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank);
    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank, offset);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    auto ctxType1 = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex - 1].contextType;
    auto ctxType2 = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex - 2].contextType;

    EXPECT_EQ(RT_CTX_TYPE_LABEL, ctxType1);
    EXPECT_EQ(RT_CTX_TYPE_LABEL, ctxType2);
}

TEST_F(DispatcherFFTSTest, should_change_ctx_to_label_given_the_dbinex_is_invalid_when_refresh_rdma_ctx)
{
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "invalid_when_refresh_rdma_ctx");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    u32 dbIndex = 2;
    u64 dbInfo = 2;
    u32 userRank;
    struct send_wr wr;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->completed = true;
    fftsCtxsPtr->refreshIndex = 0;
    dbIndex = INVALID_UINT;
    dbInfo = INVALID_U64;

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank);
    auto ctxType1 = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex - 1].contextType;
    EXPECT_EQ(RT_CTX_TYPE_LABEL, ctxType1);
}

TEST_F(DispatcherFFTSTest, should_refresh_ctx_to_rdma_given_the_dbindex_is_valid_when_refresh_rdma_ctx)
{
    MOCKER(GetWorkflowMode)
    .stubs()   
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "valid_when_refresh_rdma_ctx");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    u32 dbIndex = INVALID_UINT;
    u64 dbInfo = INVALID_U64;
    u32 userRank;
    struct send_wr wr;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->completed = true;
    fftsCtxsPtr->refreshIndex = 0;
    dbIndex = 1;
    dbInfo = 2;

    fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream, userRank);
    auto ctxType1 = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex - 1].contextType;
    EXPECT_EQ(RT_CTX_TYPE_WRITE_VALUE, ctxType1);
}

TbeReduce::AutoTilingCompileInfo* stub_AutoTilingCompileInfo_TilingParseContext()
{
    static TbeReduce::AutoTilingCompileInfo CompileInfo;
    CompileInfo.soc_info.core_num = 2;
    CompileInfo.int64_mode = 0;
    return &CompileInfo;
}

gert::TilingData *stub_GetRawTilingData() {
    return nullptr;
}

TEST_F(DispatcherFFTSTest, ffts_profiling_001) {
    DlProfFunction::GetInstance().DlProfFunctionStubInit();
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    MOCKER(HcclCheckLogLevel)
    .stubs()
    .will(returnValue(true));

    MOCKER(rtGeneralCtrl)
    .stubs()
    .with(any(), any(), eq(RT_GNL_CTRL_TYPE_FFTS_PLUS))
    .will(returnValue(RT_ERROR_NONE));

    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "ffts_profiling_001");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    DeviceMem dst = DeviceMem::alloc(80);
    DeviceMem src = DeviceMem::alloc(80);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->MemcpyAsync(dst, src, stream);
    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartTaskApiSubscribe();
    profilingManager.StartHostHcclOpSubscribe();
    profilingManager.StartAddtionInfoSubscribe();
    std::vector<Stream> subStreams;
    auto ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    u64 profConfigL0 = 0x84000985;
    profilingManager.StopSubscribe(profConfigL0);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    u64 dataCount = 20;
    fftsDispatcher->InlineReduceAsync(src.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream, dst.ptr());
    ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(DispatcherFFTSTest, ffts_profiling_reportaddtioninfo) {
    DlProfFunction::GetInstance().DlProfFunctionStubInit();
    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    MOCKER(HcclCheckLogLevel)
    .stubs()
    .will(returnValue(true));

    MOCKER(rtGeneralCtrl)
    .stubs()
    .with(any(), any(), eq(RT_GNL_CTRL_TYPE_FFTS_PLUS))
    .will(returnValue(RT_ERROR_NONE));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(true));
    auto meta = HcclOpMetaInfo::GetOneForAllGather();
    HcclResult retCode = fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "ffts_profiling_reportaddtioninfo");
    EXPECT_EQ(HCCL_SUCCESS, retCode);

    DeviceMem dst = DeviceMem::alloc(80);
    DeviceMem src = DeviceMem::alloc(80);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    fftsDispatcher->MemcpyAsync(dst, src, stream);
    auto &profilingManager = hccl::ProfilingManager::Instance();
    profilingManager.StartTaskApiSubscribe();
    profilingManager.StartHostHcclOpSubscribe();
    profilingManager.StartHostApiSubscribe();
    profilingManager.StartAddtionInfoSubscribe();
    profilingManager.StartFftsLaunchSubscribe();
    std::vector<Stream> subStreams;
    auto ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    u64 profConfigL0 = 0x84000985;
    profilingManager.StopSubscribe(profConfigL0);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    u64 dataCount = 20;
    fftsDispatcher->InlineReduceAsync(src.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream, dst.ptr());
    ret = fftsDispatcher->LaunchTasksEx(stream, subStreams);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(DispatcherFFTSTest, ut_init_dispatcher_failed) {
    HcclDispatcher dispatcher = nullptr;
    DispatcherPub dispatcherPub(0);
    MOCKER_CPP_VIRTUAL(dispatcherPub, &DispatcherPub::Init)
    .stubs()
    .will(returnValue(HCCL_E_PTR));
    HcclResult ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, -1, &dispatcher);
    EXPECT_EQ(HCCL_E_PTR, ret);
    EXPECT_EQ(dispatcher, nullptr);
    GlobalMockObject::verify();
}

TEST_F(DispatcherFFTSTest, ut_set_normalmode) {
    fftsDispatcher->SetNormalMode();
}

TEST_F(DispatcherFFTSTest, ut_profiler_task_handler_when_param_null) {
    hccl::ProfilerManager profilerManager(0, 0, 0);
    void *param = nullptr;
    profilerManager.TaskProfilerHandle(param, 0);
    EXPECT_EQ(param, nullptr);
}

TEST_F(DispatcherFFTSTest, ut_ffts_reduce_datatype_not_support)
{
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    GraphMgr::GraphCtxMgr graphCtxMgr;
    // 无效reduce类型
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    HcclResult ret = graphCtxMgr.InitFftsDescSdmaReduce(fftsCtxsPtr, stream.id(), nullptr, nullptr, 0,
        HCCL_DATA_TYPE_INT8, HCCL_REDUCE_RESERVED);
    EXPECT_EQ(HCCL_E_PARA, ret);
    // 无效数据类型
    ret = graphCtxMgr.InitFftsDescSdmaReduce(fftsCtxsPtr, stream.id(), nullptr, nullptr, 0,
        HCCL_DATA_TYPE_RESERVED, HCCL_REDUCE_MAX);
    EXPECT_EQ(HCCL_E_PARA, ret);
    // 不支持的reduce类型
    ret = graphCtxMgr.InitFftsDescSdmaReduce(fftsCtxsPtr, stream.id(), nullptr, nullptr, 0,
        HCCL_DATA_TYPE_INT8, HCCL_REDUCE_PROD);
    EXPECT_EQ(HCCL_E_PARA, ret);
    // 不支持的数据类型
    ret = graphCtxMgr.InitFftsDescSdmaReduce(fftsCtxsPtr, stream.id(), nullptr, nullptr, 0,
        HCCL_DATA_TYPE_INT64, HCCL_REDUCE_MAX);
    EXPECT_EQ(HCCL_E_PARA, ret);
}

void TaskProfilerCallBackFfts(void *userPtr, void *param, u32 length)
{
    HCCL_INFO("TaskProfilerCallBackFfts");
    return;
}

TEST_F(DispatcherFFTSTest, ut_dispatcherGraph_test)
{
    u64 config = 16;
    MOCKER(GetProfConfig)
    .stubs()
    .will(returnValue(config));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(true));

    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    u32 taskExceptionSwitch = 1;
    MOCKER(GetExternalInputTaskExceptionSwitch)
    .stubs()
    .will(returnValue(taskExceptionSwitch));

    MOCKER_CPP(&DispatcherGraph::GetNotifyDfxInfo)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(&DispatcherPub::IsProfSubscribeAdditionInfo)
    .stubs()
    .will(returnValue(true));

    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    void *srcAddr = nullptr;
    void *dstAddr = nullptr;
    HcclResult ret = Malloc(&srcAddr, 10);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = Malloc(&dstAddr, 10);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    fftsDispatcher->RegLoadTaskCallBack(srcAddr, TaskProfilerCallBackFfts);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    u32 userRank = 0; 
    u32 remoteUserRank = 1;
    s32 stage = 3;
    u32 ctxIdx = 2;
    u32 timeOut = 10;

    GraphMgr::HcclFftsContextsInfo fftsContex;
    fftsDispatcher->fftsCtxsPtr = &fftsContex;
    fftsDispatcher->disableFfts_ = false;
    MOCKER(GraphAddWaitTask)
    .stubs()
    .with(any(), any(), any(), any(), any(), outBoundP(&ctxIdx))
    .will(returnValue(HCCL_SUCCESS));

    ret = fftsDispatcher->SignalWait(signal, stream, userRank, remoteUserRank,
        stage, false, notifyId, timeOut);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(GraphAddMemcpyTask)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBoundP(&ctxIdx))
    .will(returnValue(HCCL_SUCCESS));
    DeviceMem dst = DeviceMem::alloc(1024);
    DeviceMem src = DeviceMem::alloc(1024);
    hccl::LinkType inLinkType = hccl::LinkType::LINK_HCCS;
    ret = fftsDispatcher->MemcpyAsync(dst, src, stream, remoteUserRank, inLinkType);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(GraphAddReduceTask)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), any(), any(), outBoundP(&ctxIdx))
    .will(returnValue(HCCL_SUCCESS));
    u64 dataCount = 10;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_INT8;
    HcclReduceOp reduceOp = HcclReduceOp::HCCL_REDUCE_SUM;
    HcclReduceType reduceType = HcclReduceType::HCCL_INLINE_REDUCE;
    ret = fftsDispatcher->ReduceAsync(dstAddr, srcAddr, dataCount, datatype, reduceOp,
        stream, reduceType);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(GraphAddInlineReduceTask)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), any(), any(), outBoundP(&ctxIdx))
    .will(returnValue(HCCL_SUCCESS));
    ret = fftsDispatcher->InlineReduceAsync(srcAddr, dataCount, datatype, reduceOp,
        stream, dstAddr, remoteUserRank, inLinkType);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(GraphAddTailVectorReduceTask)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBoundP(&ctxIdx))
    .will(returnValue(HCCL_SUCCESS));
    ret = fftsDispatcher->SetGraphTailVectorReduceDescSdma(dstAddr, srcAddr, dataCount, datatype,
        reduceOp, stream);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(GraphAddVectorReduceTask)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), any(), outBoundP(&ctxIdx))
    .will(returnValue(HCCL_SUCCESS));

    int count = 10;
     ret = fftsDispatcher->SetGraphTailVectorReduceDescSdma(dstAddr, srcAddr, dataCount, datatype,
        reduceOp, stream);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(GraphAddRdmaSendTask)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBoundP(&ctxIdx))
    .will(returnValue(HCCL_SUCCESS));
    u32 dbindex = 0;
    u64 dbinfo = 0;
    struct sg_list list = {0};
    struct send_wr wr = {nullptr};
    // 构造wr信息
    list.addr = static_cast<u64>(reinterpret_cast<uintptr_t>(srcAddr));
    list.len = 10;
    wr.buf_list = &list;
    wr.buf_num = 1; /* 此处list只有一个，设置为1 */
    wr.dst_addr = static_cast<u64>(reinterpret_cast<uintptr_t>(dstAddr));
    wr.op = 0; /* RDMA_WRITE: 0 */
    wr.send_flag = RA_SEND_SIGNALED;
    bool isCapture = true;
    ret = fftsDispatcher->RdmaSend(dbindex, dbinfo, wr, stream, remoteUserRank, isCapture);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    u64 offset = 0;
    ret = fftsDispatcher->RdmaSend(dbindex, dbinfo, wr, stream, userRank, offset, isCapture);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = Free(srcAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = Free(dstAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(DispatcherFFTSTest, ut_legacy_common_test)
{
    void *srcAddr = nullptr;
    void *dstAddr = nullptr;
    HcclResult ret = Malloc(&srcAddr, 10);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = Malloc(&dstAddr, 10);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = TbeMemcpy(dstAddr, 10, srcAddr, 10, ACL_MEMCPY_HOST_TO_DEVICE);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = Free(srcAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = Free(dstAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    int i = 0;
    aclrtFuncHandle funcHandle = &i;
    aclrtArgsHandle argsHandle;
    ret = KernelArgsInit(funcHandle, &argsHandle);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = KernelArgsFinalize(funcHandle);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    size_t paramSize = 1;
    aclrtParamHandle paramHandle;
    ret = KernelArgsAppend(funcHandle, &i, paramSize, &paramHandle);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    void *aicAddr = nullptr;
    ret = GetFunctionAddr(funcHandle, &aicAddr, &aicAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    uint32_t numBlocks = 10;
    aclrtStream stream = &i;
    aclrtLaunchKernelCfg cfg;
    argsHandle = &i;

    ret = LaunchKernelWithConfig(funcHandle, numBlocks, stream, &cfg, argsHandle, nullptr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    rtArgsEx_t argsInfo;
    void *devArgsAddr = nullptr;
    void *argsHandle1 = nullptr;
    ret = GetDevArgsAddr(stream, &argsInfo, &devArgsAddr, &argsHandle1);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    u32 dbIndex = 0;
    u64 dbAddr = 0;
    LegacyDevType type91093 = LegacyDevType::DEV_TYPE_910_93;
    MOCKER(GetDeviceType)
    .stubs()
    .with(outBound(type91093))
    .will(returnValue(HCCL_SUCCESS));

    ret = GetRdmaDoorbellAddr(dbIndex, dbAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    aclrtBinHandle binHandle = &i;
    std::string kernelName="test";
    ret = BinaryGetFunction(binHandle, kernelName.c_str(), &funcHandle);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(rtGetNotifyAddress)
    .stubs()
    .will(returnValue(RT_ERROR_NONE));

    u64 notifyAddr = 0;
    ret = NotifyGetAddr(dstAddr, &notifyAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    MOCKER(aclrtGetNotifyId)
    .stubs()
    .with(any(), any())
    .will(returnValue(ACL_SUCCESS));
    u32 notifyId = 0;
    ret = GetNotifyID(dstAddr, &notifyId);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    setenv("HCCL_DEBUG_CONFIG", "alg", 1);
    ret = LegacyParseDebugConfig();
    EXPECT_EQ(HCCL_SUCCESS, ret);

    GlobalMockObject::verify();
}

TEST_F(DispatcherFFTSTest, ut_graph_ctx_mgr_common_test)
{
    uint32_t  streamId = 10;
    uint32_t dataCount = 10;
    uint32_t ctxIdx;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_INT8;
    HcclReduceOp reduceOp = HcclReduceOp::HCCL_REDUCE_SUM;
    void *srcAddr = nullptr;
    void *dstAddr = nullptr;
    HcclResult ret = Malloc(&srcAddr, 128);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = Malloc(&dstAddr, 128);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    void *graphMgr = GraphMgrInit();
    std::string key = "graph_ctx_mgr_common";
    void *fftsCtxsPtr = GetGraphCtx(graphMgr, key.c_str(), key.length());

    ret = GraphAddInlineReduceTask(graphMgr, fftsCtxsPtr, streamId, dstAddr, srcAddr, dataCount, datatype, reduceOp, &ctxIdx);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    uint32_t numBlocks = 1;
    ret = GraphAddVectorReduceTask(graphMgr, fftsCtxsPtr, streamId, 1, srcAddr, dstAddr, numBlocks, &ctxIdx);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddTailVectorReduceTask(graphMgr, fftsCtxsPtr, streamId, dstAddr, srcAddr, 10, &ctxIdx);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddVectorReduceArgs(graphMgr, srcAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddRecordTaskById(graphMgr, fftsCtxsPtr, streamId, streamId);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddWaitTaskById(graphMgr, fftsCtxsPtr, streamId, streamId);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = LaunchGraph(graphMgr, srcAddr, fftsCtxsPtr, 20, &ctxIdx);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddInlineReduceTask(graphMgr, fftsCtxsPtr, streamId, dstAddr, srcAddr, dataCount, datatype, reduceOp, &ctxIdx);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddVectorReduceTask(graphMgr, fftsCtxsPtr, streamId, 1, srcAddr, dstAddr, numBlocks, &ctxIdx);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddTailVectorReduceTask(graphMgr, fftsCtxsPtr, streamId, dstAddr, srcAddr, 10, &ctxIdx);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddVectorReduceArgs(graphMgr, srcAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddRecordTaskById(graphMgr, fftsCtxsPtr, streamId, streamId);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = GraphAddWaitTaskById(graphMgr, fftsCtxsPtr, streamId, streamId);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    GraphDump(graphMgr, fftsCtxsPtr);

    GraphMgrDeInit(graphMgr);

    ret = Free(srcAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = Free(dstAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GlobalMockObject::verify();
}