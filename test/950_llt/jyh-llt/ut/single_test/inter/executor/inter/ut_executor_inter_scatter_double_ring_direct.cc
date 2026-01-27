#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#define private public
#define protected public
#include "alg_template_base_pub.h"
#include "scatter_double_ring_direct_pub.h"
#include "heartbeat.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
#include "local_notify.h"

using namespace std;
using namespace hccl;

class ScatterDoubleRingDirectTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ScatterDoubleRingDirectTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ScatterDoubleRingDirectTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::RegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::UnRegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

        std::cout << "A Test SetUP" << std::endl;
        opInfo.inputAddr  = inputMem.ptr();
        opInfo.outputAddr = outputMem.ptr();
        opInfo.count      = 360;
        opInfo.dataType   = HCCL_DATA_TYPE_FP32;
        opInfo.reduceOp   = HCCL_REDUCE_SUM;
        slice1.size       = 90;
        slice1.offset     = 0;
        slice2.size       = 90;
        slice2.offset     = 90;
        slice3.size       = 90;
        slice3.offset     = 180;
        slice4.size       = 90;
        slice4.offset     = 270;
        slices.clear();
        std::vector<Slice> multiRingSliceZero;
        std::vector<Slice> userMemInputSlices;
        multiRingSliceZero.push_back(slice1);
        multiRingSliceZero.push_back(slice2);
        multiRingSliceZero.push_back(slice3);
        multiRingSliceZero.push_back(slice4);
        userMemInputSlices.push_back(slice1);
        userMemInputSlices.push_back(slice2);
        userMemInputSlices.push_back(slice3);
        userMemInputSlices.push_back(slice4);
        slices.push_back(slice1);
        slices.push_back(slice2);
        slices.push_back(slice3);
        slices.push_back(slice4);

        for (int i = 0; i < 3; i++) {
            Stream doubleRingStream = Stream(StreamType::STREAM_TYPE_OFFLINE);
            std::shared_ptr<LocalNotify> mainSignal{new LocalNotify()};
            std::shared_ptr<LocalNotify> subSignal{new LocalNotify()};
            subStreams.push_back(doubleRingStream);
            mainSignals.push_back(mainSignal);
            subSignals.push_back(subSignal);

        }
        for (int i = 0; i < 2; i++) {
            std::vector<u32> ringsOrderVector = {0, 1, 2, 3};
            ringsOrder.push_back(ringsOrderVector);
            multiRingSliceZeroVector.push_back(multiRingSliceZero);
            userMemInputSlicesVector.push_back(userMemInputSlices);
        }
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
    DeviceMem                                 inputMem  = DeviceMem::alloc(360);
    DeviceMem                                 outputMem = DeviceMem::alloc(90);
    HcomCollOpInfo                            opInfo;
    u32                                       userRank   = 0;
    u32                                       localRank  = 0;
    Stream                                    stream = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream>                       subStreams;
    std::vector<std::shared_ptr<LocalNotify>> mainSignals;
    std::vector<std::shared_ptr<LocalNotify>> subSignals;
    std::vector<std::vector<u32>>             ringsOrder;
    std::vector<std::vector<Slice>>           multiRingSliceZeroVector;
    std::vector<std::vector<Slice>>           userMemInputSlicesVector;
    std::vector<Slice>                        slices;
    Slice                                     slice1;
    Slice                                     slice2;
    Slice                                     slice3;
    Slice                                     slice4;

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher ScatterDoubleRingDirectTest::dispatcherPtr = nullptr;
DispatcherPub *ScatterDoubleRingDirectTest::dispatcher = nullptr;

TEST_F(ScatterDoubleRingDirectTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    AlgTemplateBase *tempAlg = new ScatterDoubleRingDirect(dispatcher);
    ret = tempAlg ->Prepare(&opInfo, userRank, localRank, subStreams, mainSignals, subSignals,
        ringsOrder, multiRingSliceZeroVector, userMemInputSlicesVector);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

// 用例中comminter类型需要改变为commcombine的对应类型
TEST_F(ScatterDoubleRingDirectTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_00_combine";

    std::vector<RankInfo>     para_vector(4);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(4);

    for (int i = 0; i < 4; i++) {
        link[i].reset(new (std::nothrow)
                        Transport(new (std::nothrow) TransportBase(dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }
    std::vector<u32>   tmpRingsOrder = {0};
    std::vector<Slice> tmpUserMemInputSlices;
    std::vector<Slice> cclSlices;
    tmpUserMemInputSlices.push_back(slice1);
    AlgTemplateBase *tempAlg = new ScatterDoubleRingDirect(dispatcher);
    ret = tempAlg ->Prepare(&opInfo, userRank, localRank, subStreams, mainSignals, subSignals,
        ringsOrder, multiRingSliceZeroVector, userMemInputSlicesVector);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem output = DeviceMem::alloc(2048);

    DeviceMem input = DeviceMem::alloc(512);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    cclSlices.push_back(slice1);
    ret = tempAlg->Prepare(input, output, output, 512, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0, cclSlices, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(hccl::UserMemType, void**))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclD2DMemcpyAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(AlgTemplateBase::ExecEmptyTask)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&ScatterDoubleRingDirect::CheckParameters)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = tempAlg->RunAsync(0, 4, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
    GlobalMockObject::verify();
}
