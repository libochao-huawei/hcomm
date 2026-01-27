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
#include "nonuniform_hierarchical_ring_v1_base_pub.h"
#include "all_gather_nhr_v1_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
#include "reducer_pub.h"
#include "sender_pub.h"

using namespace hccl;
using namespace std;

class AllGatherNHRV1Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllGatherNHRV1Test SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllGatherNHRV1Test TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllGatherNHRV1Test::dispatcherPtr = nullptr;
DispatcherPub *AllGatherNHRV1Test::dispatcher = nullptr;

#if 1
TEST_F(AllGatherNHRV1Test, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    AllGatherNHRV1* tempAlg = new AllGatherNHRV1(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif

void run_async_test_all_gather_nhr_v1(DispatcherPub *dispatcher, u32 rank, u32 rankSize, u32 root, s32 nLinks = -1, bool barrier = true)
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

    AllGatherNHRV1* tempAlg = new AllGatherNHRV1(dispatcher);

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
    delete tempAlg;
}

#if 1
//对应代码中，ranksize ==1
TEST_F(AllGatherNHRV1Test, run_async_00)
{
    run_async_test_all_gather_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/1, /*root=*/0);
}
#endif

#if 1
// ranksize>1, links.size() < rankSize
TEST_F(AllGatherNHRV1Test, run_async_01)
{
    run_async_test_all_gather_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/4, /*root=*/0, /*nLinks=*/3);
}
#endif

#if 1
// rankszie>1, root = rank, hIndex < sqrtRankSize
TEST_F(AllGatherNHRV1Test, run_async_02)
{
    run_async_test_all_gather_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/0);
}
#endif

#if 1
// rankszie>1, root = rank, hIndex == sqrtRankSize
TEST_F(AllGatherNHRV1Test, run_async_03)
{
    run_async_test_all_gather_nhr_v1(dispatcher, /*rank=*/2, /*rankSize=*/8, /*root=*/2);
}
#endif

#if 1
// rankszie>1, root != rank, hIndex < sqrtRankSize
TEST_F(AllGatherNHRV1Test, run_async_04)
{
    run_async_test_all_gather_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/2);
}
#endif

#if 1
// rankszie>1, root != rank, hIndex == sqrtRankSize
TEST_F(AllGatherNHRV1Test, run_async_05)
{
    run_async_test_all_gather_nhr_v1(dispatcher, /*rank=*/2, /*rankSize=*/8, /*root=*/0);
}
#endif

// barrier
TEST_F(AllGatherNHRV1Test, run_async_06)
{
    run_async_test_all_gather_nhr_v1(dispatcher, /*rank=*/1, /*rankSize=*/8, /*root=*/0, /*nLinks=*/-1, /*barrier=*/false);
}
