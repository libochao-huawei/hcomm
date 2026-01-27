#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#include "reducer_pub.h"
#include "transport_pub.h"
#include "transport_base_pub.h"

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

class ReducerTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ReducerTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ReducerTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
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
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher ReducerTest::dispatcherPtr = nullptr;
DispatcherPub *ReducerTest::dispatcher = nullptr;

#if 1
TEST_F(ReducerTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    Reducer* reducer_int8 = new Reducer(HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, 0);
    Reducer* reducer_int = new Reducer(HCCL_DATA_TYPE_INT32, HCCL_REDUCE_SUM, 0);
    Reducer* reducer_half = new Reducer(HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, 0);
    Reducer* reducer_float = new Reducer(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 0);
    Reducer* reducer_res = new Reducer(HCCL_DATA_TYPE_RESERVED, HCCL_REDUCE_SUM, 0);
    Reducer* reducer_int16 = new Reducer(HCCL_DATA_TYPE_INT16, HCCL_REDUCE_SUM, 0);
    Reducer* reducer_bfp16 = new Reducer(HCCL_DATA_TYPE_BFP16, HCCL_REDUCE_SUM, 0);

    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete reducer_int8;
    delete reducer_int;
    delete reducer_half;
    delete reducer_float;
    delete reducer_res;
    delete reducer_int16;
    delete reducer_bfp16;
}
#endif
#if 1
TEST_F(ReducerTest, run)
{
    s32 ret = HCCL_SUCCESS;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::shared_ptr<Transport> link (new Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    link->Init();

    Reducer* reducer = new Reducer(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 1);

    DeviceMem local_src =  DeviceMem::alloc(10);
    DeviceMem local_dst = DeviceMem::alloc(10);
    DeviceMem remote_rev = DeviceMem::alloc(10);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = reducer->run(dispatcher, link, 0, local_src, local_dst, remote_rev, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = reducer->run(dispatcher, link, 0, local_src, local_src, remote_rev, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete reducer;
}
#endif

#if 1
TEST_F(ReducerTest, run_multi)
{
    s32 ret = HCCL_SUCCESS;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    DeviceMem local_src1 =  DeviceMem::alloc(10);
    DeviceMem local_dst1 = DeviceMem::alloc(10);
    DeviceMem remote_rev1 = DeviceMem::alloc(10);

    std::shared_ptr<Transport> link (new Transport(new (std::nothrow) StubTransportBase(
        dispatcher, machinePara, timeout, local_src1.ptr())));
    link->Init();

    Reducer* reducer = new Reducer(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<ReducerMemoryInfo> reduceMems1;
    reduceMems1.emplace_back(ReducerMemoryInfo{0, local_src1, local_dst1, remote_rev1});

    ret = reducer->run(dispatcher, link, reduceMems1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem local_src2 =  DeviceMem::alloc(10);
    DeviceMem local_dst2 = DeviceMem::alloc(10);
    DeviceMem remote_rev2 = DeviceMem::alloc(10);

    std::vector<ReducerMemoryInfo> reduceMems2;
    reduceMems2.emplace_back(ReducerMemoryInfo{0, local_src2, local_src2, remote_rev2});

    ret = reducer->run(dispatcher, link, reduceMems2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete reducer;
}
#endif

#if 1
class ReducerTestLink : public Transport
{
public:
    explicit ReducerTestLink(HcclDispatcher dispatcher,
                        MachinePara& machine_para, std::chrono::milliseconds timeout);
    virtual ~ReducerTestLink();

    bool isSupportTxReduce()
    {
        return true;
    }
};

ReducerTestLink::ReducerTestLink(HcclDispatcher dispatcher,
                        MachinePara& machine_para, std::chrono::milliseconds timeout)
    : Transport(new (std::nothrow) TransportBase(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machine_para, timeout))
{

}

ReducerTestLink::~ReducerTestLink()
{

}

TEST_F(ReducerTest, run_link_support_txWithReduce)
{
    MOCKER(&HcclReduceAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;

    if (dispatcher == nullptr) {
        HCCL_ERROR("run_link_support_txWithReduce dispatcher is null");
    } else {
        HCCL_ERROR("run_link_support_txWithReduce dispatcher addr[%p]", dispatcher);
    }
    SalSleep(1);

    std::shared_ptr<Transport> link;
    link.reset(new (std::nothrow) ReducerTestLink(dispatcher, machinePara, timeout));

    const Reducer reducer(HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, RDMA_REDUCE_BITMASK);

    DeviceMem local_src =  DeviceMem::alloc(128);
    DeviceMem local_dst = DeviceMem::alloc(128);
    DeviceMem remote_rev = DeviceMem::alloc(128);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = reducer.run(dispatcher, link, 0, local_src, local_dst, remote_rev, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = reducer.run(dispatcher, link, 0, local_src, local_dst, remote_rev, stream,
        DstMemType::RESULT_OUTPUT_MEM);

    EXPECT_EQ(ret, HCCL_SUCCESS);
}

#if 1
TEST_F(ReducerTest, run_link_support_txWithReduce_multi)
{
    s32 ret = HCCL_SUCCESS;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    DeviceMem local_src =  DeviceMem::alloc(10);
    DeviceMem local_dst = DeviceMem::alloc(10);
    DeviceMem remote_rev = DeviceMem::alloc(10);

    std::shared_ptr<Transport> link;
    link.reset(new (std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, local_src.ptr())));


    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    const Reducer reducer(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, RDMA_REDUCE_BITMASK);

    std::vector<ReducerMemoryInfo> reduceMems1;
    reduceMems1.emplace_back(ReducerMemoryInfo{0, local_src, local_dst, remote_rev});
    ret = reducer.run(dispatcher, link, reduceMems1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<ReducerMemoryInfo> reduceMems2;
    reduceMems2.emplace_back(ReducerMemoryInfo{0, local_src, local_src, remote_rev});
    ret = reducer.run(dispatcher, link, reduceMems2, stream, DstMemType::RESULT_OUTPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    const Reducer reducer1(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, INLINE_REDUCE_BITMASK);
    ret = reducer1.run(dispatcher, link, reduceMems1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}
#endif

#endif