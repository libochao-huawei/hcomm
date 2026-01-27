#define private public
#define protected public
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "alg_template_base_pub.h"
#include "aligned_reduce_scatter_double_ring_pub.h"
#include "alg_template_register.h"
#include "sal.h"
#include "comm_ring_pub.h"
#undef private
#undef protected

using namespace std;
using namespace hccl;

class StubTransportBase : public TransportBase
{
public:
    StubTransportBase(const HcclDispatcher dispatcher,
    MachinePara &machinePara,
    std::chrono::milliseconds timeout, void *remotePtr)
    : TransportBase(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machinePara, timeout), remotePtr_(remotePtr)
    {
    }

    HcclResult GetRemoteMem(UserMemType memType, void **remotePtr) {
        *remotePtr = remotePtr_;
        return HCCL_SUCCESS;
    }
private:
    void *remotePtr_;
};

class AlignedReduceScatterDoubleRingTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AlignedReduceScatterDoubleRingTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AlignedReduceScatterDoubleRingTest TearDown--\033[0m" << std::endl;
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
        GlobalMockObject::verify();
    }
    DeviceMem                                 inputMem  = DeviceMem::alloc(360);
    DeviceMem                                 outputMem = DeviceMem::alloc(90);
    HcomCollOpInfo                            opInfo;
    u32                                       userRank   = 0;
    Stream                                    stream = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream>                       subStreams = {stream, stream, stream};
    std::shared_ptr<LocalNotify>              mainSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> mainSignals = {mainSignal, mainSignal, mainSignal};
    std::shared_ptr<LocalNotify>              subSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> subSignals = {subSignal, subSignal, subSignal};
    std::vector<u32>                          ringsOrder = {0, 1, 2, 3};
    std::vector<std::vector<u32>> ringsOrders = {
        {0, 1, 2, 3},
        {0, 3, 2, 1}
    };
    std::vector<Slice>                        userMemInputSlices;
    std::vector<std::vector<Slice>>           userMemInputSlicesOfDoubleRing;
    std::vector<Slice>                        slices;
    Slice                                     slice1;
    Slice                                     slice2;
    Slice                                     slice3;
    Slice                                     slice4;
    u64 reduceAttrBitMap = 0x01;

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher AlignedReduceScatterDoubleRingTest::dispatcherPtr = nullptr;
DispatcherPub *AlignedReduceScatterDoubleRingTest::dispatcher = nullptr;

TEST_F(AlignedReduceScatterDoubleRingTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_DB_RING, dispatcher);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 用例中comminter类型需要改变为commcombine的对应类型
TEST_F(AlignedReduceScatterDoubleRingTest, run_async_00)
{
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));

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
    // std::vector<u32>   tmpRingsOrder = {0};
    std::vector<std::vector<u32>> tmpRingsOrders = {
        {0},
        {0}
    };
    std::vector<Slice> tmpUserMemInputSlices;
    std::vector<Slice> cCLslices;
    std::vector<std::vector<Slice>> multRingsSlices;
    tmpUserMemInputSlices.push_back(slice1);
    std::vector<std::vector<Slice>> userMemInputSlicesOfDoubleRing;
    userMemInputSlicesOfDoubleRing.push_back(tmpUserMemInputSlices);
    userMemInputSlicesOfDoubleRing.push_back(tmpUserMemInputSlices);
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_DB_RING, dispatcher);

    DeviceMem output = DeviceMem::alloc(1);

    DeviceMem input = DeviceMem::alloc(1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    cCLslices.push_back(slice1);
    multRingsSlices.push_back(cCLslices);
    multRingsSlices.push_back(cCLslices);
    ret = tempAlg->Prepare(input, output, output, 1, HCCL_DATA_TYPE_INT8, stream, multRingsSlices, HCCL_REDUCE_SUM, 0,
        0, false, reduceAttrBitMap, &opInfo, userRank, subStreams, mainSignals, subSignals,
        tmpRingsOrders, userMemInputSlicesOfDoubleRing);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(AlignedReduceScatterDoubleRingTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_01_combine";

    std::vector<RankInfo>     para_vector(2);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    DeviceMem output = DeviceMem::alloc(180);

    DeviceMem input = DeviceMem::alloc(180);

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(2);

    for (int i = 0; i < 2; i++) {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
        link[i]->pimpl_->transportAttr_.linkType = hccl::LinkType::LINK_HCCS_SW;
    }
    std::vector<u32>   tmpRingsOrder = {0, 1};
    std::vector<std::vector<u32>> tmpRingsOrders = {
        {0, 1},
        {0, 1}
    };
    std::vector<Slice> tmpUserMemInputSlices;
    std::vector<Slice> cCLslices;
    std::vector<std::vector<Slice>> multRingsSlices;
    tmpUserMemInputSlices.push_back(slice1);
    tmpUserMemInputSlices.push_back(slice2);
    userMemInputSlicesOfDoubleRing.push_back(tmpUserMemInputSlices);
    userMemInputSlicesOfDoubleRing.push_back(tmpUserMemInputSlices);
    opInfo.outputAddr  = nullptr;
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_DB_RING, dispatcher);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    cCLslices.push_back(slice1);
    cCLslices.push_back(slice2);
    multRingsSlices.push_back(cCLslices);
    multRingsSlices.push_back(cCLslices);
    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, multRingsSlices, HCCL_REDUCE_SUM, 0,
        0, false, reduceAttrBitMap, &opInfo, userRank, subStreams, mainSignals, subSignals,
        tmpRingsOrders, userMemInputSlicesOfDoubleRing);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclD2DMemcpyAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclReduceAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = tempAlg->RunAsync(1, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(AlignedReduceScatterDoubleRingTest, MemcpyInitSlices)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_01_combine";

    std::vector<RankInfo>     para_vector(2);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    DeviceMem output = DeviceMem::alloc(180);

    DeviceMem input = DeviceMem::alloc(180);

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(2);

    for (int i = 0; i < 2; i++) {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
        link[i]->pimpl_->transportAttr_.linkType = hccl::LinkType::LINK_HCCS_SW;
    }
    std::vector<u32>   tmpRingsOrder = {0, 1};
    std::vector<std::vector<u32>> tmpRingsOrders = {
        {0, 1},
        {0, 1}
    };
    std::vector<Slice> tmpUserMemInputSlices;
    std::vector<Slice> cCLslices;
    std::vector<std::vector<Slice>> multRingsSlices;
    tmpUserMemInputSlices.push_back(slice1);
    tmpUserMemInputSlices.push_back(slice2);
    userMemInputSlicesOfDoubleRing.push_back(tmpUserMemInputSlices);
    userMemInputSlicesOfDoubleRing.push_back(tmpUserMemInputSlices);
    opInfo.outputAddr  = nullptr;
    std::unique_ptr<AlgTemplateBase> basePtr = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_DB_RING, dispatcher);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    cCLslices.push_back(slice1);
    cCLslices.push_back(slice2);
    multRingsSlices.push_back(cCLslices);
    multRingsSlices.push_back(cCLslices);
    std::unique_ptr<AlignedReduceScatterDoubleRing> tempAlg = std::unique_ptr<AlignedReduceScatterDoubleRing>(
        dynamic_cast<AlignedReduceScatterDoubleRing*>(basePtr.release()));
    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, multRingsSlices, HCCL_REDUCE_SUM, 0,
        0, false, reduceAttrBitMap, &opInfo, userRank, subStreams, mainSignals, subSignals,
        tmpRingsOrders, userMemInputSlicesOfDoubleRing);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclD2DMemcpyAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclReduceAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    u64 ringIndex = 0;
    DeviceMem dstInit;
    DeviceMem srcInit;
    DeviceMem dstSubInit;
    DeviceMem srcSubInit;
    ret = tempAlg->MemcpyInitSlices(ringIndex, dstInit, srcInit, dstSubInit, srcSubInit);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}