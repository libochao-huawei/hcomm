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
#include "all_reduce_mesh_oneshot_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
#include "reducer_pub.h"
#include "sender_pub.h"

using namespace hccl;
using namespace std;

class AllReduceMeshDirectOneshotTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceMeshDirectOneshotTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceMeshDirectOneshotTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllReduceMeshDirectOneshotTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceMeshDirectOneshotTest::dispatcher = nullptr;

void run_async_test_all_reduce_mesh_oneshot_nhr(DispatcherPub *dispatcher, u32 rank, u32 rankSize, u32 root, s32 nLinks = -1, bool barrier = true)
{
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 ret = HCCL_SUCCESS;
    DeviceMem output =  DeviceMem::alloc(128*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*rankSize);

    std::string collect_id = "test_run_async";
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

    u64 reduceAttrBitMap;
    std::vector<Stream> meshStreams;
    std::vector<std::shared_ptr<LocalNotify>> meshSignal;
    std::vector<std::shared_ptr<LocalNotify>> meshSignalAux;
    u32 interRank;
    u32 interRankSize;
    u32 userRank;
    u32 addr = 4;
    HcomCollOpInfo opInfo;
    opInfo.inputAddr = &addr;
    opInfo.outputAddr = &addr;
    opInfo.count = 0;
    opInfo.dataType = HCCL_DATA_TYPE_INT8;

    AllReduceMeshDirectOneshot *tempAlg = new AllReduceMeshDirectOneshot(dispatcher);
    tempAlg->Prepare(
        reduceAttrBitMap, meshStreams, meshSignal, meshSignalAux, interRank, interRankSize, userRank, &opInfo);

    if (!barrier) {
        tempAlg->CloseBarrier();
    }

    
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
// ranksize>1, links.size() < rankSize
TEST_F(AllReduceMeshDirectOneshotTest, run_async_01)
{
    run_async_test_all_reduce_mesh_oneshot_nhr(dispatcher, /*rank=*/0, /*rankSize=*/4, /*root=*/0, /*nLinks=*/3);
}
#endif
