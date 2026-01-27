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
#include "scatter_ring_direct_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"

using namespace std;
using namespace hccl;

class ScatterRingDirectTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ScatterRingDirectTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ScatterRingDirectTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
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
        userMemInputSlices.clear();
        slices.clear();
        userMemInputSlices.push_back(slice1);
        userMemInputSlices.push_back(slice2);
        userMemInputSlices.push_back(slice3);
        userMemInputSlices.push_back(slice4);
        slices.push_back(slice1);
        slices.push_back(slice2);
        slices.push_back(slice3);
        slices.push_back(slice4);
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
    Stream                                    stream = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<u32>                          ringsOrder = {0, 1, 2, 3};
    std::vector<Slice>                        userMemInputSlices;
    std::vector<Slice>                        slices;
    Slice                                     slice1;
    Slice                                     slice2;
    Slice                                     slice3;
    Slice                                     slice4;

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher ScatterRingDirectTest::dispatcherPtr = nullptr;
DispatcherPub *ScatterRingDirectTest::dispatcher = nullptr;

TEST_F(ScatterRingDirectTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    AlgTemplateBase *tempAlg = new ScatterRingDirect(dispatcher);
    ret = tempAlg->Prepare(&opInfo, userRank, ringsOrder,userMemInputSlices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

// 用例中comminter类型需要改变为commcombine的对应类型
TEST_F(ScatterRingDirectTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_00_combine";

    std::vector<RankInfo>     para_vector(1);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(1);

    for (int i = 0; i < 1; i++) {
        link[i].reset(new (std::nothrow)
                        Transport(new (std::nothrow) TransportBase(dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }
    std::vector<u32>   tmpRingsOrder = {0};
    std::vector<Slice> tmpUserMemInputSlices;
    std::vector<Slice> cCLslices;
    tmpUserMemInputSlices.push_back(slice1);
    AlgTemplateBase *tempAlg = new ScatterRingDirect(dispatcher);
    ret = tempAlg->Prepare(&opInfo, userRank, tmpRingsOrder, tmpUserMemInputSlices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output = DeviceMem::alloc(1);

    DeviceMem input = DeviceMem::alloc(1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    cCLslices.push_back(slice1);
    ret = tempAlg->Prepare(input, output, output, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, cCLslices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

TEST_F(ScatterRingDirectTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_01_combine";

    std::vector<RankInfo>     para_vector(2);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(2);

    for (int i = 0; i < 2; i++) {
        link[i].reset(new (std::nothrow)
                        Transport(new (std::nothrow) TransportBase(dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }
    std::vector<u32>   tmpRingsOrder = {0, 1};
    std::vector<Slice> tmpUserMemInputSlices;
    tmpUserMemInputSlices.push_back(slice1);
    tmpUserMemInputSlices.push_back(slice2);

    AlgTemplateBase *tempAlg = new ScatterRingDirect(dispatcher);
    ret = tempAlg->Prepare(&opInfo, userRank, tmpRingsOrder, tmpUserMemInputSlices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output = DeviceMem::alloc(3);

    DeviceMem input = DeviceMem::alloc(6);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    ret = tempAlg->RunAsync(1, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
    GlobalMockObject::verify();
}

TEST_F(ScatterRingDirectTest, run_async_root)
{
    s32                              ret        = HCCL_SUCCESS;
    std::string                      collect_id = "test_run_async_02_combine";
    std::vector<RankInfo>     para_vector(3);
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

    AlgTemplateBase *tempAlg = new ScatterRingDirect(dispatcher);
    ret = tempAlg->Prepare(&opInfo, userRank, ringsOrder, userMemInputSlices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(inputMem, outputMem, outputMem, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));

    ret = tempAlg->RunAsync(0, 4, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
    GlobalMockObject::verify();
}