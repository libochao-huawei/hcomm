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
#include "all_gather_ring_concurrent_direct_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
#include "dispatcher_pub.h"
#include "alg_template_register.h"

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

class AllGatherRingConcurrentDirectTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllGatherRingConcurrentDirectTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllGatherRingConcurrentDirectTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
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
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
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
    std::vector<Stream>                       subStreams = {stream};
    std::shared_ptr<LocalNotify>              mainSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> mainSignals = {mainSignal};
    std::shared_ptr<LocalNotify>              subSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> subSignals = {subSignal};
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
HcclDispatcher AllGatherRingConcurrentDirectTest::dispatcherPtr = nullptr;
DispatcherPub *AllGatherRingConcurrentDirectTest::dispatcher = nullptr;

TEST_F(AllGatherRingConcurrentDirectTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);
    AlgTemplateBase *tempAlg = new AllGatherRingConcurrentDirect(dispatcher);
    ret = tempAlg->Prepare(&opInfo, userRank, subStreams, mainSignals, subSignals, ringsOrder, userMemInputSlices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

// 用例中comminter类型需要改变为commcombine的对应类型
TEST_F(AllGatherRingConcurrentDirectTest, run_async_00)
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
    AlgTemplateBase *tempAlg = new AllGatherRingConcurrentDirect(dispatcher);
    ret = tempAlg->Prepare(&opInfo, userRank, subStreams, mainSignals, subSignals,
        tmpRingsOrder, tmpUserMemInputSlices, true);
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

TEST_F(AllGatherRingConcurrentDirectTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_01_combine";

    std::vector<RankInfo>     para_vector(2);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    DeviceMem output = DeviceMem::alloc(6);

    DeviceMem input = DeviceMem::alloc(3);

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(2);

    for (int i = 0; i < 2; i++) {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }
    std::vector<u32>   tmpRingsOrder = {0, 1};
    std::vector<Slice> tmpUserMemInputSlices;
    tmpUserMemInputSlices.push_back(slice1);
    tmpUserMemInputSlices.push_back(slice2);
    AlgTemplateBase *tempAlg = new AllGatherRingConcurrentDirect(dispatcher);
    ret = tempAlg->Prepare(&opInfo, userRank, subStreams, mainSignals, subSignals,
        tmpRingsOrder, tmpUserMemInputSlices, true);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::MemcpyAsync, HcclResult(DispatcherPub::*)(hccl::DeviceMem &, const hccl::DeviceMem &, hccl::Stream &, u32,
        hccl::LinkType)).stubs().will(returnValue(HCCL_SUCCESS));
    ret = tempAlg->RunAsync(1, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
    GlobalMockObject::verify();
}

TEST_F(AllGatherRingConcurrentDirectTest, create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_GATHER_RING_CONCURRENT_DIRECT, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherRingConcurrentDirect *alg = dynamic_cast<AllGatherRingConcurrentDirect *>(tempAlg.get());
    // 校验成员变量
    s32 ret = tempAlg->Prepare(&opInfo, userRank, subStreams, mainSignals, subSignals, ringsOrder, userMemInputSlices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(alg->opInfo_, &opInfo);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_TRUE(alg->isSdma_);
    EXPECT_EQ(alg->subStreams_.size(), subStreams.size());
    for (u32 i = 0; i < subStreams.size(); ++i) {
        EXPECT_EQ(alg->subStreams_[i].id(), subStreams[i].id());
    }
    EXPECT_EQ(alg->mainSignals_.size(), mainSignals.size());
    for (u32 i = 0; i < mainSignals.size(); ++i) {
        EXPECT_EQ(alg->mainSignals_[i], mainSignals[i]);
    }

    EXPECT_EQ(alg->subSignals_.size(), subSignals.size());
    for (u32 i = 0; i < subSignals.size(); ++i) {
        EXPECT_EQ(alg->subSignals_[i], subSignals[i]);
    }

    EXPECT_EQ(alg->ringsOrder_.size(), ringsOrder.size());
    for (u32 i = 0; i < ringsOrder.size(); ++i) {
        EXPECT_EQ(alg->ringsOrder_[i], ringsOrder[i]);
    }

    EXPECT_EQ(alg->userMemOutputSlices_.size(), userMemInputSlices.size());
    for (u32 i = 0; i < userMemInputSlices.size(); ++i) {
        EXPECT_EQ(alg->userMemOutputSlices_[i].offset, userMemInputSlices[i].offset);
    }
}
