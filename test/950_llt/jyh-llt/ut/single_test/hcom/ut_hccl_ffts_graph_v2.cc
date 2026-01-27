#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "externalinput.h"

#include <nlohmann/json.hpp>

#define private public
#define protected public
#include "topoinfo_detect.h"
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "coll_batch_send_recv_executor.h"
#include "coll_reduce_scatter_v_executor.h"
#include "coll_all_gather_v_executor.h"
#include "hccl_network.h"
#include "preempt_port_manager.h"
#include "dispatcher_pub.h"
#include "dispatcher_graph_pub.h"
#include "dispatcher_virtural_pub.h"
#include "dispatcher_aicpu_pub.h"
#undef protected
#undef private

#include "common/src/topo/hccl_whitelist.h"
#include "profiling_manager.h"
#include <hccl/hccl.h>
#include "llt_hccl_stub_pub.h"
#include <iostream>
#include <fstream>
#include "../inc/hccl/base.h"
#include "../inc/hccl/hccl_ex.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "op_base.h"
#include "adapter_rts.h"
#include "param_check_pub.h"
#include "hcom_pub.h"
#include "comm_config_pub.h"
#include "exception_handler.h"
#include "plugin_runner_pub.h"
#include "task_exception_handler_pub.h"
#include "adapter_rts.h"
#include "env_config.h"
#include "ffts_common.h"
#include "legacy_common.h"

using namespace std;
using namespace hccl;

HcclResult InitTask_stub(HcclDispatcher dispatcherPtr, hccl::Stream &stream, const bool enableCache,
    const std::string &key, bool useGraphConstructorV2)
{
    CHK_PTR_NULL(dispatcherPtr);
    CHK_RET(reinterpret_cast<DispatcherPub *>(dispatcherPtr)->ResetGraphCtx(enableCache, key, true));
    return HCCL_SUCCESS;
}

class FftsGraphV2Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--OpbaseUT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--OpbaseUT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        InitEnvParam();
        std::cout << "SetUp" << std::endl;
        ResetInitState();
        SetFftsSwitch(true);
        MOCKER(InitTask).stubs().will(invoke(InitTask_stub));
        MOCKER(GetWorkflowMode).stubs().will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));    
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        fftsDispatcher = reinterpret_cast<DispatcherGraph*>(dispatcherPtr); 
        
    }
    virtual void TearDown()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            fftsDispatcher = nullptr;
        }
        dst.free();
        src.free();
        ResetInitState();
        GlobalMockObject::verify();
        std::cout << "TearDown" << std::endl;
    }
public:
    HcclDispatcher dispatcherPtr;
    DispatcherGraph *fftsDispatcher;
    HcclOpMetaInfoDef meta = HcclOpMetaInfo::GetOneForAllGather();
    std::vector<Stream> subStreams;
    DeviceMem dst = DeviceMem::alloc(80);
    DeviceMem src = DeviceMem::alloc(80);
};

// 保证覆盖率最基本的用例
// stream1          stream2
// memcpy           inchip wait
// inchip record    memcpy
TEST_F(FftsGraphV2Test, FFTSGRAPHV2_base)
{
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);

    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, meta.GetCacheKey(),true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    EXPECT_EQ(fftsCtxsPtr->refreshIndex, 2);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 4, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyId, 1, 1, 2, 0),
        GraphMgr::HcclFfftsTaskInfo(2, 1, 1, stream2.id(), notifyId, 1, 1, 3, 0),
        GraphMgr::HcclFfftsTaskInfo(3, 4, 1, stream2.id(), 4294967295, 1, 0, 0, 0),
    };
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }
    EXPECT_EQ(fftsCtxsPtr->taskInfos.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->refreshTaskIndex, 4);    

    EXPECT_EQ(fftsCtxsPtr->contexts.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)0);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    rtFftsPlusSdmaCtx_t* ctx = reinterpret_cast<rtFftsPlusSdmaCtx_t*>(&comCtx);
    HCCL_ERROR("FFTSGRAPHV2_base fftsCtxsPtr[%p] refreshIndex[%u] contextType[0x%x]",
        fftsDispatcher->fftsCtxsPtr, fftsCtxsPtr->refreshIndex, ctx->contextType);
}

// stream1 stream2 stream3
// placeholder0
// record  wait
// record          wait
// memcpy1  memcpy2  memcpy4
// wait    record
// wait            record
// placeholder3
TEST_F(FftsGraphV2Test, FFTSGRAPHV2_inchipwait_no_afterCtx_repeat)
{   
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);

    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream3(StreamType::STREAM_TYPE_OFFLINE);
    
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "inchipwait_no_afterCtx_repeat1",true), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    rtFftsPlusSdmaCtx_t* ctx = reinterpret_cast<rtFftsPlusSdmaCtx_t*>(&comCtx);
    HCCL_ERROR("FFTSGRAPHV2_base fftsCtxsPtr[%p] refreshIndex[%u] contextType[0x%x]",
        fftsDispatcher->fftsCtxsPtr, fftsCtxsPtr->refreshIndex, ctx->contextType);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 8, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyId, 1, 2, 2, 6),
        GraphMgr::HcclFfftsTaskInfo(2, 0, 0, stream1.id(), notifyId, 1, 2, 3, 9),
        GraphMgr::HcclFfftsTaskInfo(3, 4, 1, stream1.id(), 4294967295, 1, 1, 4, 0),
        GraphMgr::HcclFfftsTaskInfo(4, 1, 5, stream1.id(), notifyId, 2, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(5, 1, 5, stream1.id(), notifyId, 2, 1, 13, 0),
        GraphMgr::HcclFfftsTaskInfo(6, 1, 2, stream2.id(), notifyId, 1, 1, 7, 0),
        GraphMgr::HcclFfftsTaskInfo(7, 4, 2, stream2.id(), 4294967295, 1, 1, 8, 0),
        GraphMgr::HcclFfftsTaskInfo(8, 0, 2, stream2.id(), notifyId, 1, 2, 10, 4),
        GraphMgr::HcclFfftsTaskInfo(9, 1, 4, stream3.id(), notifyId, 1, 1, 11, 0),
        GraphMgr::HcclFfftsTaskInfo(10, 4, 3, stream2.id(), 4294967295, 1, 0, 0, 0),
        GraphMgr::HcclFfftsTaskInfo(11, 8, 4, stream3.id(), 4294967295, 1, 1, 12, 0),
        GraphMgr::HcclFfftsTaskInfo(12, 0, 4, stream3.id(), notifyId, 1, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(13, 8, 5, stream1.id(), 4294967295, 1, 0, 0, 0),
    };
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }

    EXPECT_EQ(fftsCtxsPtr->contexts.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[1], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[2], (u32)4);

    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[0], (u32)5);

    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorNum, (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[0], (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[1], (u32)5);

    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[0], (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorNum, (u32)0);

    EXPECT_EQ(fftsCtxsPtr->taskInfos.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->refreshTaskIndex, 14);    

    // 子图复用验证
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "inchipwait_no_afterCtx_repeat2",true), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

    ResetInitState();
    GlobalMockObject::verify();
}


TEST_F(FftsGraphV2Test, FFTSGRAPHV2_record_no_prectx_repeat)
{
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);

    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream3(StreamType::STREAM_TYPE_OFFLINE);

    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "record_no_prectx_repeat1",true), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));

    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
    
    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 8, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyId, 1, 2, 2, 7),
        GraphMgr::HcclFfftsTaskInfo(2, 0, 0, stream1.id(), notifyId, 1, 2, 3, 10),
        GraphMgr::HcclFfftsTaskInfo(3, 4, 1, stream1.id(), 4294967295, 1, 1, 4, 0),
        GraphMgr::HcclFfftsTaskInfo(4, 1, 2, stream1.id(), notifyId, 2, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(5, 1, 2, stream1.id(), notifyId, 2, 1, 6, 0),
        GraphMgr::HcclFfftsTaskInfo(6, 4, 2, stream1.id(), 4294967295, 1, 1, 14, 0),
        GraphMgr::HcclFfftsTaskInfo(7, 1, 3, stream2.id(), notifyId, 1, 1, 8, 0),
        GraphMgr::HcclFfftsTaskInfo(8, 4, 3, stream2.id(), 4294967295, 1, 1, 9, 0),
        GraphMgr::HcclFfftsTaskInfo(9, 0, 3, stream2.id(), notifyId, 1, 2, 11, 4),
        GraphMgr::HcclFfftsTaskInfo(10, 1, 5, stream3.id(), notifyId, 1, 1, 12, 0),
        GraphMgr::HcclFfftsTaskInfo(11, 4, 4, stream2.id(), 4294967295, 1, 0, 0, 0),
        GraphMgr::HcclFfftsTaskInfo(12, 8, 5, stream3.id(), 4294967295, 1, 1, 13, 0),
        GraphMgr::HcclFfftsTaskInfo(13, 0, 5, stream3.id(), notifyId, 1, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(14, 4, 6, stream1.id(), 4294967295, 1, 0, 0, 0),
    };
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }

    EXPECT_EQ(fftsCtxsPtr->contexts.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[1], (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[2], (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[0], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[0], (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorNum, (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[0], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[1], (u32)4);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[0], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->taskInfos.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->refreshTaskIndex, 15);    

    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "record_no_prectx_repeat2",true), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
}


TEST_F(FftsGraphV2Test, FFTSGRAPHV2_no_label_task_repeat)
{
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    
    
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream3(StreamType::STREAM_TYPE_OFFLINE);
    
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "no_label_task_repeat1",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 4, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyId, 1, 2, 2, 7),
        GraphMgr::HcclFfftsTaskInfo(2, 0, 0, stream1.id(), notifyId, 1, 2, 3, 10),
        GraphMgr::HcclFfftsTaskInfo(3, 4, 1, stream1.id(), 4294967295, 1, 1, 4, 0),
        GraphMgr::HcclFfftsTaskInfo(4, 1, 2, stream1.id(), notifyId, 2, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(5, 1, 2, stream1.id(), notifyId, 2, 1, 6, 0),
        GraphMgr::HcclFfftsTaskInfo(6, 4, 2, stream1.id(), 4294967295, 1, 1, 14, 0),
        GraphMgr::HcclFfftsTaskInfo(7, 1, 3, stream2.id(), notifyId, 1, 1, 8, 0),
        GraphMgr::HcclFfftsTaskInfo(8, 4, 3, stream2.id(), 4294967295, 1, 1, 9, 0),
        GraphMgr::HcclFfftsTaskInfo(9, 0, 3, stream2.id(), notifyId, 1, 2, 11, 4),
        GraphMgr::HcclFfftsTaskInfo(10, 1, 5, stream3.id(), notifyId, 1, 1, 12, 0),
        GraphMgr::HcclFfftsTaskInfo(11, 4, 4, stream2.id(), 4294967295, 1, 0, 0, 0),
        GraphMgr::HcclFfftsTaskInfo(12, 8, 5, stream3.id(), 4294967295, 1, 1, 13, 0),
        GraphMgr::HcclFfftsTaskInfo(13, 0, 5, stream3.id(), notifyId, 1, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(14, 4, 6, stream1.id(), 4294967295, 1, 0, 0, 0), 
    };
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }

    EXPECT_EQ(fftsCtxsPtr->contexts.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[1], (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[2], (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[0], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[0], (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorNum, (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[0], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[1], (u32)4);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[0], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->taskInfos.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->refreshTaskIndex, 15);    

    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "no_label_task_repeat2",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1, hccl::LinkType::LINK_HCCS));
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
}


// 保证覆盖率最基本的用例
// 单条流连续  wait record  wait record 
// 单条流连续  record wait  record  wait
TEST_F(FftsGraphV2Test, FFTSGRAPHV2_wait_no_afterctx_2_repeat)
{
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);

    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);

    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "wait_no_afterctx_2_repeat1",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 4, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyId, 1, 2, 2, 6),
        GraphMgr::HcclFfftsTaskInfo(2, 1, 2, stream1.id(), notifyId, 2, 1, 3, 0),
        GraphMgr::HcclFfftsTaskInfo(3, 0, 0, stream1.id(), notifyId, 1, 2, 4, 9),
        GraphMgr::HcclFfftsTaskInfo(4, 1, 2, stream1.id(), notifyId, 2, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(5, 0, 0, stream1.id(), notifyId, 1, 2, 12, 11),
        GraphMgr::HcclFfftsTaskInfo(6, 1, 1, stream2.id(), notifyId, 1, 1, 7, 0),
        GraphMgr::HcclFfftsTaskInfo(7, 4, 1, stream2.id(), 4294967295, 1, 1, 8, 0),
        GraphMgr::HcclFfftsTaskInfo(8, 0, 1, stream2.id(), notifyId, 1, 2, 9, 2),
        GraphMgr::HcclFfftsTaskInfo(9, 1, 3, stream2.id(), notifyId, 2, 1, 10, 0),
        GraphMgr::HcclFfftsTaskInfo(10, 0, 1, stream2.id(), notifyId, 1, 2, 11, 4),
        GraphMgr::HcclFfftsTaskInfo(11, 1, 3, stream2.id(), notifyId, 2, 1, 13, 0),
        GraphMgr::HcclFfftsTaskInfo(12, 8, 2, stream1.id(), 4294967295, 1, 0, 0, 0),
        GraphMgr::HcclFfftsTaskInfo(13, 8, 3, stream2.id(), 4294967295, 1, 0, 0, 0),
    };
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }
    EXPECT_EQ(fftsCtxsPtr->taskInfos.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->refreshTaskIndex, 14);    

    EXPECT_EQ(fftsCtxsPtr->contexts.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[1], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[2], (u32)3);

    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[0], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[1], (u32)3);

    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->ctxNum, 4);    

    // 子图复用
    fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "wait_no_afterctx_2_repeat2",true);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);

    EXPECT_EQ(fftsCtxsPtr->ctxNum, 4);   
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
}

TEST_F(FftsGraphV2Test, FFTSGRAPHV2_successnotenough)
{
    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream3(StreamType::STREAM_TYPE_OFFLINE);

    HcclRtNotify signal1;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal1), HCCL_SUCCESS);
    u32 notifyID1 = 0;
    EXPECT_EQ(hrtGetNotifyID(signal1, &notifyID1), HCCL_SUCCESS);

    HcclRtNotify signal2;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal2), HCCL_SUCCESS);
    u32 notifyID2 = 0;
    EXPECT_EQ(hrtGetNotifyID(signal2, &notifyID2), HCCL_SUCCESS);

    fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "successnotenough1",true);
    for (int loop = 0; loop < 16; loop++) {
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal1, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal2, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
        EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
        EXPECT_EQ(fftsDispatcher->SignalWait(signal1, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
        EXPECT_EQ(fftsDispatcher->SignalWait(signal2, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);

        EXPECT_EQ(fftsDispatcher->SignalWait(signal1, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
        EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal1, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

        EXPECT_EQ(fftsDispatcher->SignalWait(signal2, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal2, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    }
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    u32 ctxNum = fftsCtxsPtr->refreshIndex;
    EXPECT_EQ(fftsDispatcher->LaunchTasksEx(stream1, subStreams), HCCL_SUCCESS);
    u32 expasionSuccessorCtxNum = fftsCtxsPtr->expasionSuccessorCtxNum;
    u32 inchipwaitPlaceHolderNum = fftsCtxsPtr->inchipwaitPlaceHolderNum;
    ctxNum = ctxNum + inchipwaitPlaceHolderNum + expasionSuccessorCtxNum;
    EXPECT_EQ(ctxNum, 43);
    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 8, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyID1, 1, 2, 2, 6),
        GraphMgr::HcclFfftsTaskInfo(2, 0, 0, stream1.id(), notifyID2, 1, 2, 3, 9),
        GraphMgr::HcclFfftsTaskInfo(3, 4, 1, stream1.id(), 4294967295, 1, 1, 4, 0),
        GraphMgr::HcclFfftsTaskInfo(4, 1, 4, stream1.id(), notifyID1, 2, 1, 5, 0),
        GraphMgr::HcclFfftsTaskInfo(5, 1, 4, stream1.id(), notifyID2, 2, 1, 12, 0),
        GraphMgr::HcclFfftsTaskInfo(6, 1, 2, stream2.id(), notifyID1, 1, 1, 7, 0),
        GraphMgr::HcclFfftsTaskInfo(7, 4, 2, stream2.id(), 4294967295, 1, 1, 8, 0),
        GraphMgr::HcclFfftsTaskInfo(8, 0, 2, stream2.id(), notifyID1, 1, 2, 17, 4),
        GraphMgr::HcclFfftsTaskInfo(9, 1, 3, stream3.id(), notifyID2, 1, 1, 10, 0),
        GraphMgr::HcclFfftsTaskInfo(10, 8, 3, stream3.id(), 4294967295, 1, 1, 11, 0),
        GraphMgr::HcclFfftsTaskInfo(11, 0, 3, stream3.id(), notifyID2, 1, 2, 20, 5),
        GraphMgr::HcclFfftsTaskInfo(12, 0, 1, stream1.id(), notifyID1, 1, 2, 13, 17),
        GraphMgr::HcclFfftsTaskInfo(13, 0, 1, stream1.id(), notifyID2, 1, 2, 14, 20),
        GraphMgr::HcclFfftsTaskInfo(14, 4, 4, stream1.id(), 4294967295, 1, 1, 15, 0),
        GraphMgr::HcclFfftsTaskInfo(15, 1, 6, stream1.id(), notifyID1, 2, 1, 16, 0),
        GraphMgr::HcclFfftsTaskInfo(16, 1, 6, stream1.id(), notifyID2, 2, 1, 22, 0),
        GraphMgr::HcclFfftsTaskInfo(17, 1, 5, stream2.id(), notifyID1, 2, 1, 18, 0),
        GraphMgr::HcclFfftsTaskInfo(18, 4, 5, stream2.id(), 4294967295, 1, 1, 19, 0),
        GraphMgr::HcclFfftsTaskInfo(19, 0, 5, stream2.id(), notifyID1, 1, 2, 27, 15),
        GraphMgr::HcclFfftsTaskInfo(20, 1, 35, stream3.id(), notifyID2, 2, 1, 21, 0),
        GraphMgr::HcclFfftsTaskInfo(21, 0, 3, stream3.id(), notifyID2, 1, 2, 30, 16),
        GraphMgr::HcclFfftsTaskInfo(22, 0, 4, stream1.id(), notifyID1, 1, 2, 23, 27),
        GraphMgr::HcclFfftsTaskInfo(23, 0, 4, stream1.id(), notifyID2, 1, 2, 24, 30),
        GraphMgr::HcclFfftsTaskInfo(24, 4, 6, stream1.id(), 4294967295, 1, 1, 25, 0),
        GraphMgr::HcclFfftsTaskInfo(25, 1, 8, stream1.id(), notifyID1, 2, 1, 26, 0),
        GraphMgr::HcclFfftsTaskInfo(26, 1, 8, stream1.id(), notifyID2, 2, 1, 32, 0),
        GraphMgr::HcclFfftsTaskInfo(27, 1, 7, stream2.id(), notifyID1, 2, 1, 28, 0),
        GraphMgr::HcclFfftsTaskInfo(28, 4, 7, stream2.id(), 4294967295, 1, 1, 29, 0),
        GraphMgr::HcclFfftsTaskInfo(29, 0, 7, stream2.id(), notifyID1, 1, 2, 37, 25),
        GraphMgr::HcclFfftsTaskInfo(30, 1, 35, stream3.id(), notifyID2, 2, 1, 31, 0),
        GraphMgr::HcclFfftsTaskInfo(31, 0, 3, stream3.id(), notifyID2, 1, 2, 40, 26),
        GraphMgr::HcclFfftsTaskInfo(32, 0, 6, stream1.id(), notifyID1, 1, 2, 33, 37),
        GraphMgr::HcclFfftsTaskInfo(33, 0, 6, stream1.id(), notifyID2, 1, 2, 34, 40),
        GraphMgr::HcclFfftsTaskInfo(34, 4, 8, stream1.id(), 4294967295, 1, 1, 35, 0),
        GraphMgr::HcclFfftsTaskInfo(35, 1, 10, stream1.id(), notifyID1, 2, 1, 36, 0),
        GraphMgr::HcclFfftsTaskInfo(36, 1, 10, stream1.id(), notifyID2, 2, 1, 42, 0),
        GraphMgr::HcclFfftsTaskInfo(37, 1, 9, stream2.id(), notifyID1, 2, 1, 38, 0),
        GraphMgr::HcclFfftsTaskInfo(38, 4, 9, stream2.id(), 4294967295, 1, 1, 39, 0),
        GraphMgr::HcclFfftsTaskInfo(39, 0, 9, stream2.id(), notifyID1, 1, 2, 47, 35),
        GraphMgr::HcclFfftsTaskInfo(40, 1, 35, stream3.id(), notifyID2, 2, 1, 41, 0),
        GraphMgr::HcclFfftsTaskInfo(41, 0, 3, stream3.id(), notifyID2, 1, 2, 50, 36),
        GraphMgr::HcclFfftsTaskInfo(42, 0, 8, stream1.id(), notifyID1, 1, 2, 43, 47),
        GraphMgr::HcclFfftsTaskInfo(43, 0, 8, stream1.id(), notifyID2, 1, 2, 44, 50),
        GraphMgr::HcclFfftsTaskInfo(44, 4, 10, stream1.id(), 4294967295, 1, 1, 45, 0),
        GraphMgr::HcclFfftsTaskInfo(45, 1, 12, stream1.id(), notifyID1, 2, 1, 46, 0),
        GraphMgr::HcclFfftsTaskInfo(46, 1, 12, stream1.id(), notifyID2, 2, 1, 52, 0),
        GraphMgr::HcclFfftsTaskInfo(47, 1, 11, stream2.id(), notifyID1, 2, 1, 48, 0),
        GraphMgr::HcclFfftsTaskInfo(48, 4, 11, stream2.id(), 4294967295, 1, 1, 49, 0),
        GraphMgr::HcclFfftsTaskInfo(49, 0, 11, stream2.id(), notifyID1, 1, 2, 57, 45),
        GraphMgr::HcclFfftsTaskInfo(50, 1, 35, stream3.id(), notifyID2, 2, 1, 51, 0),
        GraphMgr::HcclFfftsTaskInfo(51, 0, 3, stream3.id(), notifyID2, 1, 2, 60, 46),
        GraphMgr::HcclFfftsTaskInfo(52, 0, 10, stream1.id(), notifyID1, 1, 2, 53, 57),
        GraphMgr::HcclFfftsTaskInfo(53, 0, 10, stream1.id(), notifyID2, 1, 2, 54, 60),
        GraphMgr::HcclFfftsTaskInfo(54, 4, 12, stream1.id(), 4294967295, 1, 1, 55, 0),
        GraphMgr::HcclFfftsTaskInfo(55, 1, 14, stream1.id(), notifyID1, 2, 1, 56, 0),
        GraphMgr::HcclFfftsTaskInfo(56, 1, 14, stream1.id(), notifyID2, 2, 1, 62, 0),
        GraphMgr::HcclFfftsTaskInfo(57, 1, 13, stream2.id(), notifyID1, 2, 1, 58, 0),
        GraphMgr::HcclFfftsTaskInfo(58, 4, 13, stream2.id(), 4294967295, 1, 1, 59, 0),
        GraphMgr::HcclFfftsTaskInfo(59, 0, 13, stream2.id(), notifyID1, 1, 2, 67, 55),
        GraphMgr::HcclFfftsTaskInfo(60, 1, 35, stream3.id(), notifyID2, 2, 1, 61, 0),
        GraphMgr::HcclFfftsTaskInfo(61, 0, 3, stream3.id(), notifyID2, 1, 2, 70, 56),
        GraphMgr::HcclFfftsTaskInfo(62, 0, 12, stream1.id(), notifyID1, 1, 2, 63, 67),
        GraphMgr::HcclFfftsTaskInfo(63, 0, 12, stream1.id(), notifyID2, 1, 2, 64, 70),
        GraphMgr::HcclFfftsTaskInfo(64, 4, 14, stream1.id(), 4294967295, 1, 1, 65, 0),
        GraphMgr::HcclFfftsTaskInfo(65, 1, 16, stream1.id(), notifyID1, 2, 1, 66, 0),
        GraphMgr::HcclFfftsTaskInfo(66, 1, 16, stream1.id(), notifyID2, 2, 1, 72, 0),
        GraphMgr::HcclFfftsTaskInfo(67, 1, 15, stream2.id(), notifyID1, 2, 1, 68, 0),
        GraphMgr::HcclFfftsTaskInfo(68, 4, 15, stream2.id(), 4294967295, 1, 1, 69, 0),
        GraphMgr::HcclFfftsTaskInfo(69, 0, 15, stream2.id(), notifyID1, 1, 2, 77, 65),
        GraphMgr::HcclFfftsTaskInfo(70, 1, 35, stream3.id(), notifyID2, 2, 1, 71, 0),
        GraphMgr::HcclFfftsTaskInfo(71, 0, 3, stream3.id(), notifyID2, 1, 2, 80, 66),
        GraphMgr::HcclFfftsTaskInfo(72, 0, 14, stream1.id(), notifyID1, 1, 2, 73, 77),
        GraphMgr::HcclFfftsTaskInfo(73, 0, 14, stream1.id(), notifyID2, 1, 2, 74, 80),
        GraphMgr::HcclFfftsTaskInfo(74, 4, 16, stream1.id(), 4294967295, 1, 1, 75, 0),
        GraphMgr::HcclFfftsTaskInfo(75, 1, 18, stream1.id(), notifyID1, 2, 1, 76, 0),
        GraphMgr::HcclFfftsTaskInfo(76, 1, 18, stream1.id(), notifyID2, 2, 1, 82, 0),
        GraphMgr::HcclFfftsTaskInfo(77, 1, 17, stream2.id(), notifyID1, 2, 1, 78, 0),
        GraphMgr::HcclFfftsTaskInfo(78, 4, 17, stream2.id(), 4294967295, 1, 1, 79, 0),
        GraphMgr::HcclFfftsTaskInfo(79, 0, 17, stream2.id(), notifyID1, 1, 2, 87, 75),
        GraphMgr::HcclFfftsTaskInfo(80, 1, 35, stream3.id(), notifyID2, 2, 1, 81, 0),
        GraphMgr::HcclFfftsTaskInfo(81, 0, 3, stream3.id(), notifyID2, 1, 2, 90, 76),
        GraphMgr::HcclFfftsTaskInfo(82, 0, 16, stream1.id(), notifyID1, 1, 2, 83, 87),
        GraphMgr::HcclFfftsTaskInfo(83, 0, 16, stream1.id(), notifyID2, 1, 2, 84, 90),
        GraphMgr::HcclFfftsTaskInfo(84, 4, 18, stream1.id(), 4294967295, 1, 1, 85, 0),
        GraphMgr::HcclFfftsTaskInfo(85, 1, 20, stream1.id(), notifyID1, 2, 1, 86, 0),
        GraphMgr::HcclFfftsTaskInfo(86, 1, 20, stream1.id(), notifyID2, 2, 1, 92, 0),
        GraphMgr::HcclFfftsTaskInfo(87, 1, 19, stream2.id(), notifyID1, 2, 1, 88, 0),
        GraphMgr::HcclFfftsTaskInfo(88, 4, 19, stream2.id(), 4294967295, 1, 1, 89, 0),
        GraphMgr::HcclFfftsTaskInfo(89, 0, 19, stream2.id(), notifyID1, 1, 2, 97, 85),
        GraphMgr::HcclFfftsTaskInfo(90, 1, 35, stream3.id(), notifyID2, 2, 1, 91, 0),
        GraphMgr::HcclFfftsTaskInfo(91, 0, 3, stream3.id(), notifyID2, 1, 2, 100, 86),
        GraphMgr::HcclFfftsTaskInfo(92, 0, 18, stream1.id(), notifyID1, 1, 2, 93, 97),
        GraphMgr::HcclFfftsTaskInfo(93, 0, 18, stream1.id(), notifyID2, 1, 2, 94, 100),
        GraphMgr::HcclFfftsTaskInfo(94, 4, 20, stream1.id(), 4294967295, 1, 1, 95, 0),
        GraphMgr::HcclFfftsTaskInfo(95, 1, 22, stream1.id(), notifyID1, 2, 1, 96, 0),
        GraphMgr::HcclFfftsTaskInfo(96, 1, 22, stream1.id(), notifyID2, 2, 1, 102, 0),
        GraphMgr::HcclFfftsTaskInfo(97, 1, 21, stream2.id(), notifyID1, 2, 1, 98, 0),
        GraphMgr::HcclFfftsTaskInfo(98, 4, 21, stream2.id(), 4294967295, 1, 1, 99, 0),
        GraphMgr::HcclFfftsTaskInfo(99, 0, 21, stream2.id(), notifyID1, 1, 2, 107, 95),
        GraphMgr::HcclFfftsTaskInfo(100, 1, 35, stream3.id(), notifyID2, 2, 1, 101, 0),
        GraphMgr::HcclFfftsTaskInfo(101, 0, 3, stream3.id(), notifyID2, 1, 2, 110, 96),
        GraphMgr::HcclFfftsTaskInfo(102, 0, 20, stream1.id(), notifyID1, 1, 2, 103, 107),
        GraphMgr::HcclFfftsTaskInfo(103, 0, 20, stream1.id(), notifyID2, 1, 2, 104, 110),
        GraphMgr::HcclFfftsTaskInfo(104, 4, 22, stream1.id(), 4294967295, 1, 1, 105, 0),
        GraphMgr::HcclFfftsTaskInfo(105, 1, 24, stream1.id(), notifyID1, 2, 1, 106, 0),
        GraphMgr::HcclFfftsTaskInfo(106, 1, 24, stream1.id(), notifyID2, 2, 1, 112, 0),
        GraphMgr::HcclFfftsTaskInfo(107, 1, 23, stream2.id(), notifyID1, 2, 1, 108, 0),
        GraphMgr::HcclFfftsTaskInfo(108, 4, 23, stream2.id(), 4294967295, 1, 1, 109, 0),
        GraphMgr::HcclFfftsTaskInfo(109, 0, 23, stream2.id(), notifyID1, 1, 2, 117, 105),
        GraphMgr::HcclFfftsTaskInfo(110, 1, 35, stream3.id(), notifyID2, 2, 1, 111, 0),
        GraphMgr::HcclFfftsTaskInfo(111, 0, 3, stream3.id(), notifyID2, 1, 2, 120, 106),
        GraphMgr::HcclFfftsTaskInfo(112, 0, 22, stream1.id(), notifyID1, 1, 2, 113, 117),
        GraphMgr::HcclFfftsTaskInfo(113, 0, 22, stream1.id(), notifyID2, 1, 2, 114, 120),
        GraphMgr::HcclFfftsTaskInfo(114, 4, 24, stream1.id(), 4294967295, 1, 1, 115, 0),
        GraphMgr::HcclFfftsTaskInfo(115, 1, 26, stream1.id(), notifyID1, 2, 1, 116, 0),
        GraphMgr::HcclFfftsTaskInfo(116, 1, 26, stream1.id(), notifyID2, 2, 1, 122, 0),
        GraphMgr::HcclFfftsTaskInfo(117, 1, 25, stream2.id(), notifyID1, 2, 1, 118, 0),
        GraphMgr::HcclFfftsTaskInfo(118, 4, 25, stream2.id(), 4294967295, 1, 1, 119, 0),
        GraphMgr::HcclFfftsTaskInfo(119, 0, 25, stream2.id(), notifyID1, 1, 2, 127, 115),
        GraphMgr::HcclFfftsTaskInfo(120, 1, 35, stream3.id(), notifyID2, 2, 1, 121, 0),
        GraphMgr::HcclFfftsTaskInfo(121, 0, 3, stream3.id(), notifyID2, 1, 2, 130, 116),
        GraphMgr::HcclFfftsTaskInfo(122, 0, 24, stream1.id(), notifyID1, 1, 2, 123, 127),
        GraphMgr::HcclFfftsTaskInfo(123, 0, 24, stream1.id(), notifyID2, 1, 2, 124, 130),
        GraphMgr::HcclFfftsTaskInfo(124, 4, 26, stream1.id(), 4294967295, 1, 1, 125, 0),
        GraphMgr::HcclFfftsTaskInfo(125, 1, 28, stream1.id(), notifyID1, 2, 1, 126, 0),
        GraphMgr::HcclFfftsTaskInfo(126, 1, 28, stream1.id(), notifyID2, 2, 1, 132, 0),
        GraphMgr::HcclFfftsTaskInfo(127, 1, 27, stream2.id(), notifyID1, 2, 1, 128, 0),
        GraphMgr::HcclFfftsTaskInfo(128, 4, 27, stream2.id(), 4294967295, 1, 1, 129, 0),
        GraphMgr::HcclFfftsTaskInfo(129, 0, 27, stream2.id(), notifyID1, 1, 2, 137, 125),
        GraphMgr::HcclFfftsTaskInfo(130, 1, 35, stream3.id(), notifyID2, 2, 1, 131, 0),
        GraphMgr::HcclFfftsTaskInfo(131, 0, 3, stream3.id(), notifyID2, 1, 2, 140, 126),
        GraphMgr::HcclFfftsTaskInfo(132, 0, 26, stream1.id(), notifyID1, 1, 2, 133, 137),
        GraphMgr::HcclFfftsTaskInfo(133, 0, 26, stream1.id(), notifyID2, 1, 2, 134, 140),
        GraphMgr::HcclFfftsTaskInfo(134, 4, 28, stream1.id(), 4294967295, 1, 1, 135, 0),
        GraphMgr::HcclFfftsTaskInfo(135, 1, 30, stream1.id(), notifyID1, 2, 1, 136, 0),
        GraphMgr::HcclFfftsTaskInfo(136, 1, 30, stream1.id(), notifyID2, 2, 1, 142, 0),
        GraphMgr::HcclFfftsTaskInfo(137, 1, 29, stream2.id(), notifyID1, 2, 1, 138, 0),
        GraphMgr::HcclFfftsTaskInfo(138, 4, 29, stream2.id(), 4294967295, 1, 1, 139, 0),
        GraphMgr::HcclFfftsTaskInfo(139, 0, 29, stream2.id(), notifyID1, 1, 2, 147, 135),
        GraphMgr::HcclFfftsTaskInfo(140, 1, 35, stream3.id(), notifyID2, 2, 1, 141, 0),
        GraphMgr::HcclFfftsTaskInfo(141, 0, 3, stream3.id(), notifyID2, 1, 2, 150, 136),
        GraphMgr::HcclFfftsTaskInfo(142, 0, 28, stream1.id(), notifyID1, 1, 2, 143, 147),
        GraphMgr::HcclFfftsTaskInfo(143, 0, 28, stream1.id(), notifyID2, 1, 2, 144, 150),
        GraphMgr::HcclFfftsTaskInfo(144, 4, 30, stream1.id(), 4294967295, 1, 1, 145, 0),
        GraphMgr::HcclFfftsTaskInfo(145, 1, 32, stream1.id(), notifyID1, 2, 1, 146, 0),
        GraphMgr::HcclFfftsTaskInfo(146, 1, 32, stream1.id(), notifyID2, 2, 1, 152, 0),
        GraphMgr::HcclFfftsTaskInfo(147, 1, 31, stream2.id(), notifyID1, 2, 1, 148, 0),
        GraphMgr::HcclFfftsTaskInfo(148, 4, 31, stream2.id(), 4294967295, 1, 1, 149, 0),
        GraphMgr::HcclFfftsTaskInfo(149, 0, 31, stream2.id(), notifyID1, 1, 2, 157, 145),
        GraphMgr::HcclFfftsTaskInfo(150, 1, 35, stream3.id(), notifyID2, 2, 1, 151, 0),
        GraphMgr::HcclFfftsTaskInfo(151, 0, 3, stream3.id(), notifyID2, 1, 2, 160, 146),
        GraphMgr::HcclFfftsTaskInfo(152, 0, 30, stream1.id(), notifyID1, 1, 2, 153, 157),
        GraphMgr::HcclFfftsTaskInfo(153, 0, 30, stream1.id(), notifyID2, 1, 2, 154, 160),
        GraphMgr::HcclFfftsTaskInfo(154, 4, 32, stream1.id(), 4294967295, 1, 1, 155, 0),
        GraphMgr::HcclFfftsTaskInfo(155, 1, 34, stream1.id(), notifyID1, 2, 1, 156, 0),
        GraphMgr::HcclFfftsTaskInfo(156, 1, 34, stream1.id(), notifyID2, 2, 1, 162, 0),
        GraphMgr::HcclFfftsTaskInfo(157, 1, 33, stream2.id(), notifyID1, 2, 1, 158, 0),
        GraphMgr::HcclFfftsTaskInfo(158, 4, 33, stream2.id(), 4294967295, 1, 1, 159, 0),
        GraphMgr::HcclFfftsTaskInfo(159, 0, 33, stream2.id(), notifyID1, 1, 1, 155, 0),
        GraphMgr::HcclFfftsTaskInfo(160, 1, 35, stream3.id(), notifyID2, 2, 1, 161, 0),
        GraphMgr::HcclFfftsTaskInfo(161, 0, 3, stream3.id(), notifyID2, 1, 2, 163, 156),
        GraphMgr::HcclFfftsTaskInfo(162, 8, 34, stream1.id(), 4294967295, 1, 0, 0, 0),
        GraphMgr::HcclFfftsTaskInfo(163, 8, 35, stream3.id(), 4294967295, 1, 0, 0, 0),
    };
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }

    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[1], (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[2], (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[0], (u32)4);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[1], (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[2], (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[3], (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[4], (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[5], (u32)9);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[6], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[7], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[8], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[9], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[10], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[11], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[12], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[13], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[14], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[15], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[16], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[17], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[18], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[19], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[20], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[21], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[22], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[23], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[24], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[25], (u32)36);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[0], (u32)4);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[1], (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[2], (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[3], (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[4], (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[5], (u32)9);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[6], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[7], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[8], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[9], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[10], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[11], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[12], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[13], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[14], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[15], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[16], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[17], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[18], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[19], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[20], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[21], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[22], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[23], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[24], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[25], (u32)37);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[0], (u32)4);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[1], (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[2], (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[3], (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[4], (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[5], (u32)9);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[6], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[7], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[8], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[9], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[10], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[11], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[12], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[13], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[14], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[15], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[16], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[17], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[18], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[19], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[20], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[21], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[22], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[23], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[24], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorList[25], (u32)38);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[0], (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[1], (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[2], (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[3], (u32)9);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[4], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[5], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[6], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[7], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[8], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[9], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[10], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[11], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[12], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[13], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[14], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[15], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[16], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[17], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[18], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[19], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[20], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[21], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[22], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[23], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[24], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[25], (u32)39);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[0], (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[1], (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[2], (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[3], (u32)9);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[4], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[5], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[6], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[7], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[8], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[9], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[10], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[11], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[12], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[13], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[14], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[15], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[16], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[17], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[18], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[19], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[20], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[21], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[22], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[23], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[24], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[25], (u32)40);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[0], (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[1], (u32)9);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[2], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[3], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[4], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[5], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[6], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[7], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[8], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[9], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[10], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[11], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[12], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[13], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[14], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[15], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[16], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[17], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[18], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[19], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[20], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[21], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[22], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[23], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[24], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[25], (u32)41);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[0], (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[1], (u32)9);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[2], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[3], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[4], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[5], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[6], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[7], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[8], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[9], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[10], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[11], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[12], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[13], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[14], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[15], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[16], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[17], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[18], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[19], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[20], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[21], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[22], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[23], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[24], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[25], (u32)42);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[0], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[1], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[2], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[3], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[4], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[5], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[6], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[7], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[8], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[9], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[10], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[11], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[12], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[13], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[14], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[15], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[16], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[17], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[18], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[19], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[20], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[21], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[22], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[23], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[24], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[25], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorNum, (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[0], (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[1], (u32)11);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[2], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[3], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[4], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[5], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[6], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[7], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[8], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[9], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[10], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[11], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[12], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[13], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[14], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[15], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[16], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[17], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[18], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[19], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[20], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[21], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[22], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[23], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[24], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[25], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorNum, (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[0], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[1], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[2], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[3], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[4], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[5], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[6], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[7], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[8], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[9], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[10], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[11], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[12], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[13], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[14], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[15], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[16], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[17], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[18], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[19], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[20], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[21], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[22], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[23], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorNum, (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[0], (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[1], (u32)13);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[2], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[3], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[4], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[5], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[6], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[7], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[8], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[9], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[10], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[11], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[12], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[13], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[14], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[15], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[16], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[17], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[18], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[19], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[20], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[21], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[22], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[23], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorNum, (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[0], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[1], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[2], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[3], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[4], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[5], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[6], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[7], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[8], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[9], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[10], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[11], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[12], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[13], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[14], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[15], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[16], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[17], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[18], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[19], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[20], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorList[21], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorNum, (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[0], (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[1], (u32)15);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[2], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[3], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[4], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[5], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[6], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[7], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[8], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[9], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[10], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[11], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[12], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[13], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[14], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[15], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[16], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[17], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[18], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[19], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[20], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[13].successorList[21], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorNum, (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[0], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[1], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[2], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[3], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[4], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[5], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[6], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[7], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[8], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[9], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[10], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[11], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[12], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[13], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[14], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[15], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[16], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[17], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[18], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[14].successorList[19], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorNum, (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[0], (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[1], (u32)17);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[2], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[3], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[4], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[5], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[6], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[7], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[8], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[9], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[10], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[11], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[12], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[13], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[14], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[15], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[16], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[17], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[18], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[15].successorList[19], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorNum, (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[0], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[1], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[2], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[3], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[4], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[5], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[6], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[7], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[8], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[9], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[10], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[11], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[12], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[13], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[14], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[15], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[16], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[16].successorList[17], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorNum, (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[0], (u32)18);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[1], (u32)19);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[2], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[3], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[4], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[5], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[6], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[7], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[8], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[9], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[10], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[11], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[12], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[13], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[14], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[15], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[16], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[17].successorList[17], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorNum, (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[0], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[1], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[2], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[3], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[4], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[5], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[6], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[7], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[8], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[9], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[10], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[11], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[12], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[13], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[14], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[18].successorList[15], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorNum, (u32)16);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[0], (u32)20);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[1], (u32)21);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[2], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[3], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[4], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[5], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[6], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[7], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[8], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[9], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[10], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[11], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[12], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[13], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[14], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[19].successorList[15], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorNum, (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[0], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[1], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[2], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[3], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[4], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[5], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[6], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[7], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[8], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[9], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[10], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[11], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[12], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[20].successorList[13], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorNum, (u32)14);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[0], (u32)22);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[1], (u32)23);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[2], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[3], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[4], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[5], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[6], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[7], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[8], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[9], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[10], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[11], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[12], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[21].successorList[13], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorNum, (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[0], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[1], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[2], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[3], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[4], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[5], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[6], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[7], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[8], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[9], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[10], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[22].successorList[11], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorNum, (u32)12);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[0], (u32)24);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[1], (u32)25);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[2], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[3], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[4], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[5], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[6], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[7], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[8], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[9], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[10], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[23].successorList[11], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorNum, (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[0], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[1], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[2], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[3], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[4], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[5], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[6], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[7], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[8], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[24].successorList[9], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorNum, (u32)10);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[0], (u32)26);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[1], (u32)27);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[2], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[3], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[4], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[5], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[6], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[7], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[8], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[25].successorList[9], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorNum, (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[0], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[1], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[2], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[3], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[4], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[5], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[6], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[26].successorList[7], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorNum, (u32)8);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[0], (u32)28);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[1], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[2], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[3], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[4], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[5], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[6], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[27].successorList[7], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[28].successorNum, (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[28].successorList[0], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[28].successorList[1], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[28].successorList[2], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[28].successorList[3], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[28].successorList[4], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[28].successorList[5], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[29].successorNum, (u32)6);
    EXPECT_EQ(fftsCtxsPtr->contexts[29].successorList[0], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[29].successorList[1], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[29].successorList[2], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[29].successorList[3], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[29].successorList[4], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[29].successorList[5], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[30].successorNum, (u32)4);
    EXPECT_EQ(fftsCtxsPtr->contexts[30].successorList[0], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[30].successorList[1], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[30].successorList[2], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[30].successorList[3], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[31].successorNum, (u32)4);
    EXPECT_EQ(fftsCtxsPtr->contexts[31].successorList[0], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[31].successorList[1], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[31].successorList[2], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[31].successorList[3], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[32].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[32].successorList[0], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[33].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[33].successorList[0], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[34].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->contexts[35].successorNum, (u32)0);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorNum, (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorList[0], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorList[1], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorList[2], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorList[3], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorList[4], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorList[5], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[36].successorList[6], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorNum, (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorList[0], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorList[1], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorList[2], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorList[3], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorList[4], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorList[5], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[37].successorList[6], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorNum, (u32)7);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorList[0], (u32)29);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorList[1], (u32)30);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorList[2], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorList[3], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorList[4], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorList[5], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[38].successorList[6], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[39].successorNum, (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[39].successorList[0], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[39].successorList[1], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[39].successorList[2], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[39].successorList[3], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[39].successorList[4], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[40].successorNum, (u32)5);
    EXPECT_EQ(fftsCtxsPtr->contexts[40].successorList[0], (u32)31);
    EXPECT_EQ(fftsCtxsPtr->contexts[40].successorList[1], (u32)32);
    EXPECT_EQ(fftsCtxsPtr->contexts[40].successorList[2], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[40].successorList[3], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[40].successorList[4], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[41].successorNum, (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[41].successorList[0], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[41].successorList[1], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[41].successorList[2], (u32)35);
    EXPECT_EQ(fftsCtxsPtr->contexts[42].successorNum, (u32)3);
    EXPECT_EQ(fftsCtxsPtr->contexts[42].successorList[0], (u32)33);
    EXPECT_EQ(fftsCtxsPtr->contexts[42].successorList[1], (u32)34);
    EXPECT_EQ(fftsCtxsPtr->contexts[42].successorList[2], (u32)35);

    fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "successnotenough2",true);
    for (int loop = 0; loop < 16; loop++) {
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal1, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal2, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
        EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
        EXPECT_EQ(fftsDispatcher->SignalWait(signal1, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);
        EXPECT_EQ(fftsDispatcher->SignalWait(signal2, stream1, 0, 0, 0, true, 1), HCCL_SUCCESS);

        EXPECT_EQ(fftsDispatcher->SignalWait(signal1, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
        EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal1, stream2, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);

        EXPECT_EQ(fftsDispatcher->SignalWait(signal2, stream3, 0, 0, 0, true, 1), HCCL_SUCCESS);
        EXPECT_EQ(fftsDispatcher->SignalRecord(signal2, stream3, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    }
    EXPECT_EQ(fftsDispatcher->LaunchTasksEx(stream1, subStreams), HCCL_SUCCESS);
}

TEST_F(FftsGraphV2Test, FFTSGRAPHV2_all_task)
{
    u32 dbIndex = 0;
    u64 dbInfo = 0;
    u32 userRank;
    u64 offset = 0;
    struct SendWr wr;

    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    u64 dataCount = 64;
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "all_task1",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream1, userRank), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)1, (u64)0x0, (s32)0, false, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream1, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream2, userRank, offset), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 1, 0, 0, false, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream2, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);

    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 4, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyId, 1, 2, 2, 5),
        GraphMgr::HcclFfftsTaskInfo(2, 5, 1, stream1.id(), 4294967295, 1, 1, 3, 0),
        GraphMgr::HcclFfftsTaskInfo(3, 2, 2, stream1.id(), 4294967295, 1, 1, 4, 0),
        GraphMgr::HcclFfftsTaskInfo(4, 4, 3, stream1.id(), 4294967295, 1, 0, 0, 0),
        GraphMgr::HcclFfftsTaskInfo(5, 1, 4, stream2.id(), notifyId, 1, 1, 6, 0),
        GraphMgr::HcclFfftsTaskInfo(6, 4, 4, stream2.id(), 4294967295, 1, 1, 7, 0),
        GraphMgr::HcclFfftsTaskInfo(7, 5, 5, stream2.id(), 4294967295, 1, 1, 8, 0),
        GraphMgr::HcclFfftsTaskInfo(8, 3, 6, stream2.id(), 4294967295, 1, 1, 9, 0),
        GraphMgr::HcclFfftsTaskInfo(9, 4, 7, stream2.id(), 4294967295, 1, 0, 0, 0),
    };
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }

    EXPECT_EQ(fftsCtxsPtr->taskInfos.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->refreshTaskIndex, 10);    

    EXPECT_EQ(fftsCtxsPtr->contexts.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[1], (u32)4);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[0], (u32)2);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[0], (u32)3);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorNum, (u32)0);

    
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[0], (u32)5);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[0], (u32)6);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[0], (u32)7);

    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorNum, (u32)0);

    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "all_task2",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream1, userRank), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)1, (u64)0x0, (s32)0, false, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream1, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream2, userRank, offset), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 1, 0, 0, false, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream2, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);

    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
}


TEST_F(FftsGraphV2Test, FFTSGRAPHV2_wait_no_record)
{
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    u64 dataCount = 64;
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "wait_no_record1",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
  
    EXPECT_EQ(HCCL_E_INTERNAL, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

}

TEST_F(FftsGraphV2Test, FFTSGRAPHV2_tasksuccessor_exceed)
{
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream3(StreamType::STREAM_TYPE_OFFLINE);
    u64 dataCount = 64;
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "tasksuccessor_exceed",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream3, (u32)1));
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->taskInfos[1].successorNum = 2;

    EXPECT_EQ(HCCL_E_INTERNAL, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
}


TEST_F(FftsGraphV2Test, FFTSGRAPHV2_indegree0_not_exist)
{
    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream3(StreamType::STREAM_TYPE_OFFLINE);
    u64 dataCount = 64;
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "indegree0_not_exist",true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    fftsCtxsPtr->taskInfos[0].predCnt = 1;
    EXPECT_EQ(HCCL_E_INTERNAL, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
}


TEST_F(FftsGraphV2Test, FFTSGRAPHV2_TBEReduce)
{
    u32 dbIndex = 0;
    u64 dbInfo = 0;
    u32 userRank;
    u64 offset = 0;
    struct SendWr wr;


    HcclRtNotify signal;
    EXPECT_EQ(hrtNotifyCreateWithFlag(0, &signal), HCCL_SUCCESS);
    u32 notifyId = 0;
    EXPECT_EQ(GetNotifyID(signal, &notifyId), HCCL_SUCCESS);
    HCCL_INFO("notify id %u", notifyId);

    Stream stream1(StreamType::STREAM_TYPE_OFFLINE);
    Stream stream2(StreamType::STREAM_TYPE_OFFLINE);
    u64 dataCount = 64;
    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "TBEReduce",true), HCCL_SUCCESS);
    

    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream1, userRank), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)1, (u64)0x0, (s32)0, false, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream1, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream2, userRank, offset), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 1, 0, 0, false, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream2, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream2, HcclReduceType::HCCL_TBE_REDUCE), HCCL_SUCCESS);

    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));

    std::vector<GraphMgr::HcclFfftsTaskInfo> taskInfos = {
        GraphMgr::HcclFfftsTaskInfo(0, 4, 0, stream1.id(), 4294967295, 0, 1, 1, 0),
        GraphMgr::HcclFfftsTaskInfo(1, 0, 0, stream1.id(), notifyId, 1, 2, 2, 5),
        GraphMgr::HcclFfftsTaskInfo(2, 5, 1, stream1.id(), 4294967295, 1, 1, 3, 0),
        GraphMgr::HcclFfftsTaskInfo(3, 2, 2, stream1.id(), 4294967295, 1, 1, 4, 0),
        GraphMgr::HcclFfftsTaskInfo(4, 4, 3, stream1.id(), 4294967295, 1, 0, 0, 0),
        GraphMgr::HcclFfftsTaskInfo(5, 1, 4, stream2.id(), notifyId, 1, 1, 6, 0),
        GraphMgr::HcclFfftsTaskInfo(6, 4, 4, stream2.id(), 4294967295, 1, 1, 7, 0),
        GraphMgr::HcclFfftsTaskInfo(7, 5, 5, stream2.id(), 4294967295, 1, 1, 8, 0),
        GraphMgr::HcclFfftsTaskInfo(8, 3, 6, stream2.id(), 4294967295, 1, 1, 9, 0),
        GraphMgr::HcclFfftsTaskInfo(9, 4, 7, stream2.id(), 4294967295, 1, 1, 10, 0),
        GraphMgr::HcclFfftsTaskInfo(10, 7, 8, stream2.id(), 4294967295, 1, 1, 11, 0),
        GraphMgr::HcclFfftsTaskInfo(11, 4, 9, stream2.id(), 4294967295, 1, 1, 12, 0),
        GraphMgr::HcclFfftsTaskInfo(12, 4, 10, stream2.id(), 4294967295, 1, 1, 13, 0),
        GraphMgr::HcclFfftsTaskInfo(13, 7, 11, stream2.id(), 4294967295, 1, 1, 14, 0),
        GraphMgr::HcclFfftsTaskInfo(14, 4, 12, stream2.id(), 4294967295, 1, 0, 0, 0),
    };
    GraphMgr::HcclFftsContextsInfo *fftsCtxsPtr = static_cast<GraphMgr::HcclFftsContextsInfo*>(fftsDispatcher->fftsCtxsPtr);
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        EXPECT_EQ(fftsCtxsPtr->taskInfos[i], taskInfos[i]);
    }

    EXPECT_EQ(fftsCtxsPtr->taskInfos.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->refreshTaskIndex, 15);    

    EXPECT_EQ(fftsCtxsPtr->contexts.size(), 100);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorNum, (u32)2);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[0], (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[0].successorList[1], (u32)4);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[1].successorList[0], (u32)2);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[2].successorList[0], (u32)3);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[3].successorNum, (u32)0);

    
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[4].successorList[0], (u32)5);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[5].successorList[0], (u32)6);
    
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[6].successorList[0], (u32)7);

    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[7].successorList[0], (u32)8);

    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[8].successorList[0], (u32)9);

    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[9].successorList[0], (u32)10);

    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[10].successorList[0], (u32)11);

    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorNum, (u32)1);
    EXPECT_EQ(fftsCtxsPtr->contexts[11].successorList[0], (u32)12);

    EXPECT_EQ(fftsCtxsPtr->contexts[12].successorNum, (u32)0);


    EXPECT_EQ(fftsDispatcher->ResetGraphCtx(meta.isEnableCache, "TBEReduce2", true), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream1, (u32)1));
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)0, (u64)0x0, (s32)0, true, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream1, userRank), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalRecord(signal, stream1, (u32)1, (u64)0x0, (s32)0, false, (u64)0x0, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream1, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);

    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 0, 0, 0, true, 1), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->MemcpyAsync(dst, src, stream2, (u32)1));
    EXPECT_EQ(fftsDispatcher->RdmaSend(dbIndex, dbInfo, wr, stream2, userRank, offset), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->SignalWait(signal, stream2, 1, 0, 0, false, 1), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream2, HcclReduceType::HCCL_INLINE_REDUCE), HCCL_SUCCESS);
    EXPECT_EQ(fftsDispatcher->ReduceAsync(src.ptr(), dst.ptr(), dataCount, HcclDataType::HCCL_DATA_TYPE_INT8,
        HcclReduceOp::HCCL_REDUCE_SUM, stream2, HcclReduceType::HCCL_TBE_REDUCE), HCCL_SUCCESS);
    EXPECT_EQ(HCCL_SUCCESS, fftsDispatcher->LaunchTasksEx(stream1, subStreams));
}
