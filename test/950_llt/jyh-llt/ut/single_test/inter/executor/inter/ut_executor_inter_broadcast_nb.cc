#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "broadcast_nb_pub.h"
#include "alg_template_register.h"

using namespace std;
using namespace hccl;

class BroadcastNBInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--BroadcastNBInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--BroadcastNBInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher BroadcastNBInterTest::dispatcherPtr = nullptr;
DispatcherPub *BroadcastNBInterTest::dispatcher = nullptr;

#if 1
TEST_F(BroadcastNBInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";
    std::vector<RankInfo> para_vector(1);

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_NB, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

void run_async_test_bcast_nb(DispatcherPub *dispatcher, u32 rank, u32 rankSize, u32 root, s32 nLinks = -1, bool barrier = true)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(nLinks);
    for (int i = 0; i < nLinks; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_NB, dispatcher);
    if (!barrier) {
        tempAlg->CloseBarrier();
    }

    DeviceMem output =  DeviceMem::alloc(128*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 128*rankSize, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    if(nLinks == rankSize) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else {
        EXPECT_NE(ret, HCCL_SUCCESS);
    }

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

#if 1
//对应代码中，ranksize ==1
TEST_F(BroadcastNBInterTest, run_async_00)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/0, /*rankSize=*/1, /*root=*/0);
}
#endif

#if 1
// ranksize>1, links.size() < rankSize
TEST_F(BroadcastNBInterTest, run_async_01)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/0, /*rankSize=*/4, /*root=*/0, /*nLinks=*/3);
}
#endif

#if 1
// rankszie>1, root = rank, root=0
TEST_F(BroadcastNBInterTest, run_async_02)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/0);
}
#endif

#if 1
// rankszie>1, root = rank, root>0
TEST_F(BroadcastNBInterTest, run_async_03)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/2, /*rankSize=*/8, /*root=*/2);
}
#endif

#if 1
// rankszie>1, root != rank,root > rank
TEST_F(BroadcastNBInterTest, run_async_04)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/2);
}
#endif

#if 1
// rankszie>1, root != rank, root<rank
TEST_F(BroadcastNBInterTest, run_async_05)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/2, /*rankSize=*/8, /*root=*/0);
}
#endif

#if 1
// rankszie>1, root != rank, rank == rankSize-1
TEST_F(BroadcastNBInterTest, run_async_06)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/7, /*rankSize=*/8, /*root=*/0);
}
#endif


// barrier
TEST_F(BroadcastNBInterTest, run_async_07)
{
    run_async_test_bcast_nb(dispatcher, /*rank=*/1, /*rankSize=*/8, /*root=*/0, /*nLinks=*/-1, /*barrier=*/false);
}
#if 1
// transport 优化
TEST_F(BroadcastNBInterTest, run_async_08)
{
    setenv("HCCL_SCATTER_ENABLE_DOORBELL_OPT", "1", 1);
    setenv("HCCL_SCATTER_ENABLE_SYNC", "1", 1);
    setenv("HCCL_REDUCE_SCATTER_ENABLE_DOORBELL_OPT", "1", 1);
    setenv("HCCL_REDUCE_SCATTER_ENABLE_SYNC", "1", 1);
    setenv("HCCL_ALL_GATHER_ENABLE_DOORBELL_OPT", "1", 1);
    setenv("HCCL_ALL_GATHER_ENABLE_SYNC", "1", 1);

    run_async_test_bcast_nb(dispatcher, /*rank=*/1, /*rankSize=*/8, /*root=*/0);

    unsetenv("HCCL_SCATTER_ENABLE_DOORBELL_OPT");
    unsetenv("HCCL_SCATTER_ENABLE_SYNC");
    unsetenv("HCCL_REDUCE_SCATTER_ENABLE_DOORBELL_OPT");
    unsetenv("HCCL_REDUCE_SCATTER_ENABLE_SYNC");
    unsetenv("HCCL_ALL_GATHER_ENABLE_DOORBELL_OPT");
    unsetenv("HCCL_ALL_GATHER_ENABLE_SYNC");
}
#endif