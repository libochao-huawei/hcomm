#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "comm_base_pub.h"
#include "reduce_ring_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "reducer_pub.h"
#include "sender_pub.h"
using namespace std;
using namespace hccl;

class ReduceInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--BroadcastInterTest SetUP--\033[0m" << std::endl;
        std::cout << "\033[36m--ReduceInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ReduceInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher ReduceInterTest::dispatcherPtr = nullptr;
DispatcherPub *ReduceInterTest::dispatcher = nullptr;

#if 1
TEST_F(ReduceInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    ReduceRing* tempAlg = new ReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
//对应代码中，ranksize ==1
TEST_F(ReduceInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_00_combine";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);
    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRing* tempAlg = new ReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);
    DeviceMem output =  DeviceMem::alloc(1);
    DeviceMem input =  DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// rankszie>1, root = rank
TEST_F(ReduceInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_01_combine";

    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRing* tempAlg = new ReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);
    DeviceMem input =  DeviceMem::alloc(3);
    DeviceMem output =  DeviceMem::alloc(3);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(0, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// rankszie>1, root = next_rank
TEST_F(ReduceInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_02_combine";

    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRing* tempAlg = new ReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);
    DeviceMem output =  DeviceMem::alloc(3);
    DeviceMem input =  DeviceMem::alloc(3);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// rankszie>1, 其他情况
TEST_F(ReduceInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_03_combine";

    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRing* tempAlg = new ReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);
    DeviceMem output =  DeviceMem::alloc(3);
    DeviceMem input =  DeviceMem::alloc(3);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(2, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// chunksize更新
TEST_F(ReduceInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_04_combine";

    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRing* tempAlg = new ReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);
    DeviceMem output =  DeviceMem::alloc(3);
    DeviceMem input =  DeviceMem::alloc(3);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(2, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
