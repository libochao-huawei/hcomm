#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#define private public
#define protected public
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "all_reduce_doubling_direct_pub.h"
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

class AllReduceDoublingDirectInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceDoublingDirectInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceDoublingDirectInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllReduceDoublingDirectInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceDoublingDirectInterTest::dispatcher = nullptr;


void run_async_test_allreduce_doubling_direct(HcclDispatcher dispatcher, u32 rank, u32 rankSize, s32 nLinks = -1)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(128);
    DeviceMem input =  DeviceMem::alloc(128);

    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(nLinks);
    for (int i = 0; i < nLinks; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
        link[i]->pimpl_->transportAttr_.linkType = hccl::LinkType::LINK_HCCS;
    }

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    HcomCollOpInfo opInfo;
    opInfo.inputAddr = input.ptr();
    opInfo.outputAddr = output.ptr();
    opInfo.count = 32;
    opInfo.dataType = HCCL_DATA_TYPE_FP32;

    u64 reduceAttrBitMap = 0 | INLINE_REDUCE_BITMASK;
    AllReduceDoublingDirect* tempAlg = new AllReduceDoublingDirect(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap, &opInfo);


    ret = tempAlg->Prepare(input, output, output, 128, HCCL_DATA_TYPE_FP32, stream, HcclReduceOp::HCCL_REDUCE_SUM, 0);
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
//对应代码中，ranksize == 1
TEST_F(AllReduceDoublingDirectInterTest, run_async_00)
{
    run_async_test_allreduce_doubling_direct(dispatcher, /*rank=*/0, /*rankSize=*/1);
}
#endif

#if 1
//对应代码中，ranksize == 2
TEST_F(AllReduceDoublingDirectInterTest, run_async_01)
{
    run_async_test_allreduce_doubling_direct(dispatcher, /*rank=*/0, /*rankSize=*/2);
}
#endif

#if 1
//对应代码中，ranksize ==4
TEST_F(AllReduceDoublingDirectInterTest, run_async_02)
{
    run_async_test_allreduce_doubling_direct(dispatcher, /*rank=*/0, /*rankSize=*/4);
}
#endif

#if 1
//对应代码中，ranksize == 8
TEST_F(AllReduceDoublingDirectInterTest, run_async_03)
{
    run_async_test_allreduce_doubling_direct(dispatcher, /*rank=*/0, /*rankSize=*/8);
}
#endif

