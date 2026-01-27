#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "comm_base_pub.h"
#include "send_receive_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"

using namespace std;
using namespace hccl;

class SendReceiveTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--SendReceiveTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--SendReceiveTest TearDown--\033[0m" << std::endl;
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
};
#if 1
//对应代码中，ranksize ==1
TEST_F(SendReceiveTest, ut_BatchSendRunAsync_test)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_00_combine";

    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

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

    SendReceive* tempAlg = new SendReceive(dispatcher, link[0]);
    DeviceMem input =  DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    DeviceMem inputMemNull;
    inputMemNull.ptr_ = nullptr;

    ret = tempAlg->SendPrepare(inputMemNull, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->BatchSendRunAsync();
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = tempAlg->SendPrepare(input, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->BatchSendRunAsync();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
}

TEST_F(SendReceiveTest, ut_BatchReceiveRunAsync_test)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_00_combine";

    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

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

    SendReceive* tempAlg = new SendReceive(dispatcher, link[0]);
    DeviceMem output =  DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    DeviceMem outMemNull;
    outMemNull.ptr_ = nullptr;

    ret = tempAlg->ReceivePrepare(outMemNull, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->BatchReceiveRunAsync();
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = tempAlg->ReceivePrepare(output, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->BatchReceiveRunAsync();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
}
#endif