#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "broadcast_nhr_pub.h"
#include "broadcast_nhr_oneshot_pub.h"
#include "alg_template_register.h"

using namespace std;
using namespace hccl;

class BroadcastNHRInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--BroadcastNHRInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--BroadcastNHRInterTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
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
HcclDispatcher BroadcastNHRInterTest::dispatcherPtr = nullptr;
DispatcherPub *BroadcastNHRInterTest::dispatcher = nullptr;

#if 1
TEST_F(BroadcastNHRInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_NHR, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

void run_async_test_bcast_nhr(DispatcherPub *dispatcher, u32 rank, u32 rankSize, u32 root, s32 nLinks = -1, bool barrier = true)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_NHR, dispatcher);
    if (!barrier) {
        tempAlg->CloseBarrier();
    }

    // 中大数据量
    DeviceMem output =  DeviceMem::alloc(524288 * rankSize);

    DeviceMem input =  DeviceMem::alloc(524288 * rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 524288 * rankSize, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

void run_async_test_bcast_nhr_smalldata(DispatcherPub *dispatcher, u32 rank, u32 rankSize, u32 root, s32 nLinks = -1, bool barrier = true)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_NHR_ONESHOT, dispatcher);
    if (!barrier) {
        tempAlg->CloseBarrier();
    }

    // 小数据量
    DeviceMem output =  DeviceMem::alloc(128*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 128*rankSize, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

#if 1
//对应代码中，ranksize ==1
TEST_F(BroadcastNHRInterTest, run_async_00)
{
    run_async_test_bcast_nhr(dispatcher, /*rank=*/0, /*rankSize=*/1, /*root=*/0);
}
#endif

#if 1
// rankszie>1, root = rank,root==0
TEST_F(BroadcastNHRInterTest, run_async_01)
{
    run_async_test_bcast_nhr(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/0);
}
#endif

#if 1
// rankszie>1, root = rank,root>0
TEST_F(BroadcastNHRInterTest, run_async_02)
{
    run_async_test_bcast_nhr(dispatcher, /*rank=*/2, /*rankSize=*/8, /*root=*/2);
}
#endif

#if 1
// rankszie>1, root != rank,root>rank
TEST_F(BroadcastNHRInterTest, run_async_03)
{
    run_async_test_bcast_nhr(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/2);
}
#endif

#if 1
// rankszie>1, root != rank,root<rank
TEST_F(BroadcastNHRInterTest, run_async_04)
{
    run_async_test_bcast_nhr(dispatcher, /*rank=*/2, /*rankSize=*/8, /*root=*/0);
}
#endif

#if 1
// rankszie>1, root != rank, rank == rankSize-1
TEST_F(BroadcastNHRInterTest, run_async_05)
{
    run_async_test_bcast_nhr(dispatcher, /*rank=*/7, /*rankSize=*/8, /*root=*/0);
}
#endif

// barrier
TEST_F(BroadcastNHRInterTest, run_async_06)
{
    run_async_test_bcast_nhr(dispatcher, /*rank=*/1, /*rankSize=*/8, /*root=*/0, /*barrier=*/false);
}

#if 1
// rankszie>1, root = rank,root==0
TEST_F(BroadcastNHRInterTest, run_async_07)
{
    run_async_test_bcast_nhr_smalldata(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/0);
}
#endif
