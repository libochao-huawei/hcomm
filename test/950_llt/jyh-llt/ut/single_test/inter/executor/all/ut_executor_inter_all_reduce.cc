#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "all_reduce_ring_pub.h"
#include "sal.h"
#include "reducer_pub.h"
#include "sender_pub.h"
using namespace std;
using namespace hccl;

class AllReduceInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllReduceInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceInterTest::dispatcher = nullptr;

#if 1
TEST_F(AllReduceInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    AllReduceRing* tempAlg = new AllReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
TEST_F(AllReduceInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_00_inter";
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

    AllReduceRing* tempAlg = new AllReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output = DeviceMem::alloc(0);

    DeviceMem input = DeviceMem::alloc(0);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);


    ret = tempAlg->Prepare(input, output, output, 0, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif

#if  1
TEST_F(AllReduceInterTest, wait_completion)
{
    s32 ret = HCCL_SUCCESS;
    rtStream_t rtstream;

    rtStreamCreate(&rtstream, 5);
    Stream stream(rtstream) ;

    std::string collect_id_inter = "test_wait_completion_inter";

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

    AllReduceRing *tempAlg = new AllReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(0);

    DeviceMem input =  DeviceMem::alloc(0);

    ret = tempAlg->Prepare(input, output, 0, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
    rtStreamDestroy(rtstream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
// 满chunk的运算
TEST_F(AllReduceInterTest, run_full_chunk)
{
    s32 ret = HCCL_SUCCESS;
    rtStream_t rtstream;
    rtStreamCreate(&rtstream, 5);
    Stream stream(rtstream) ;

    std::string collect_id_inter = "test_run_residue_count_inter";
    std::vector<RankInfo> para_vector(2);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(2);
    for (int i = 0; i < 2; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AllReduceRing* tempAlg = new AllReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(1024*1024*50);

    DeviceMem input =  DeviceMem::alloc(1024*1024*50);

    ret = tempAlg->Prepare(input, output, 1024*1024*50, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 2, link);  //中间分支如何保证执行?
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
    rtStreamDestroy(rtstream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
//3个rank, 共191个数据
TEST_F(AllReduceInterTest, run_3ranks_01)
{
    s32 ret = HCCL_SUCCESS;
    rtStream_t rtstream;
    rtStreamCreate(&rtstream, 5);
    Stream stream(rtstream) ;

    std::string collect_id_inter = "test_run_3ranks_01";
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

    AllReduceRing* tempAlg = new AllReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(191);
    DeviceMem input =  DeviceMem::alloc(191);

    ret = tempAlg->Prepare(input, output, 191, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 3, link);
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
    rtStreamDestroy(rtstream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(AllReduceInterTest, run_3ranks_02)
{
    s32 ret = HCCL_SUCCESS;
    rtStream_t rtstream;
    rtStreamCreate(&rtstream, 5);
    Stream stream(rtstream) ;

    std::string collect_id_inter = "test_run_3ranks_02";
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


    AllReduceRing* tempAlg = new AllReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(191);
    DeviceMem input =  DeviceMem::alloc(191);

    ret = tempAlg->Prepare(input, output, 191, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(2, 3, link);
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
    rtStreamDestroy(rtstream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
TEST_F(AllReduceInterTest, run_async_linksize_error)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_linksize_error";
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

    AllReduceRing* tempAlg = new AllReduceRing(dispatcher);
    u64 reduceAttrBitMap = 0;
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output = DeviceMem::alloc(0);

    DeviceMem input = DeviceMem::alloc(0);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);


    ret = tempAlg->Prepare(input, output, output, 0, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 3, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

