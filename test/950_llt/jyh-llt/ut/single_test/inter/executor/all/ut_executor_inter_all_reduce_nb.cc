#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"

#define private public
#include "all_reduce_nb_pub.h"
#undef private

#include "sal.h"
#include "comm_ring_pub.h"
#include "alg_template_base_pub.h"
using namespace std;
using namespace hccl;

class AllReduceNBInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceNBInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceNBInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllReduceNBInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceNBInterTest::dispatcher = nullptr;

TEST_F(AllReduceNBInterTest, destructor_D0)
{
   s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);
    u64 reduceAttrBitMap = 0x01;
    AllReduceNB * tempAlg = new AllReduceNB(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}

void run_async_test_allreduce_nb(DispatcherPub *dispatcher, u32 rank, u32 rankSize,bool staged, RunStage stage=RunStage::RUN_PREPARE)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }
    u64 reduceAttrBitMap = 0x01;
    AllReduceNB * tempAlg = new AllReduceNB(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(128*2*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*2*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);
    ret = tempAlg->RunAsync(rank, rankSize, link);
    if(nLinks == rankSize) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else {
        EXPECT_NE(ret, HCCL_SUCCESS);
    }

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if(staged)
    {
    ret =tempAlg->RunAsyncStaged(rank, rankSize, link, stage);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    delete tempAlg;
}

#if 1
//对应代码中，ranksize ==1
TEST_F(AllReduceNBInterTest, run_async_00)
{
    run_async_test_allreduce_nb(dispatcher, /*rank=*/0, /*rankSize=*/1,/*staged*/false);
}
#endif
#if 1
//对应代码中，ranksize ==8
TEST_F(AllReduceNBInterTest, run_async_01)
{
    run_async_test_allreduce_nb(dispatcher, /*rank=*/0, /*rankSize=*/8,/*staged*/false);
}
#endif
#if 1
//对应代码中，ranksize ==8
TEST_F(AllReduceNBInterTest, run_async_02)
{
    run_async_test_allreduce_nb(dispatcher, /*rank=*/4, /*rankSize=*/8,/*staged*/false);
}
#endif

#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNBInterTest, run_async_03)
{
    run_async_test_allreduce_nb(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/false);
}
#endif
#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNBInterTest, run_async_04)
{
    run_async_test_allreduce_nb(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/true,/*Runstage*/RunStage::RUN_PREPARE);
}
#endif
#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNBInterTest, run_async_05)
{
    run_async_test_allreduce_nb(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/true,/*Runstage*/RunStage::RUN_REDUCE_SCATTER);
}
#endif
#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNBInterTest, run_async_06)
{
    run_async_test_allreduce_nb(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/true,/*Runstage*/RunStage::RUN_ALLGATHER);
}
#endif
#if 1
TEST_F(AllReduceNBInterTest, run_async_07)
{
    setenv("HCCL_SCATTER_ENABLE_DOORBELL_OPT", "1", 1);
    setenv("HCCL_SCATTER_ENABLE_SYNC", "1", 1);
    setenv("HCCL_REDUCE_SCATTER_ENABLE_DOORBELL_OPT", "1", 1);
    setenv("HCCL_REDUCE_SCATTER_ENABLE_SYNC", "1", 1);
    setenv("HCCL_ALL_GATHER_ENABLE_DOORBELL_OPT", "1", 1);
    setenv("HCCL_ALL_GATHER_ENABLE_SYNC", "1", 1);

    run_async_test_allreduce_nb(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/false);

    unsetenv("HCCL_SCATTER_ENABLE_DOORBELL_OPT");
    unsetenv("HCCL_SCATTER_ENABLE_SYNC");
    unsetenv("HCCL_REDUCE_SCATTER_ENABLE_DOORBELL_OPT");
    unsetenv("HCCL_REDUCE_SCATTER_ENABLE_SYNC");
    unsetenv("HCCL_ALL_GATHER_ENABLE_DOORBELL_OPT");
    unsetenv("HCCL_ALL_GATHER_ENABLE_SYNC");
}
#endif

