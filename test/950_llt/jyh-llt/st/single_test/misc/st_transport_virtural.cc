#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
 
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
 
#include "sal.h"
 
#include "dispatcher_pub.h"
#include "transport_virtural_pub.h"
#include "transport_heterog_p2p_pub.h"
 
#include "llt_hccl_stub_pub.h"
#include "profiler_manager.h"
#include "transport_heterog_p2p_pub.h"
#include "externalinput_pub.h"
 
 
 
using namespace std;
using namespace hccl;
 
class TransportVirturalTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--CommBaseTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--CommBaseTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher TransportVirturalTest::dispatcherPtr = nullptr;
DispatcherPub *TransportVirturalTest::dispatcher = nullptr;

TEST_F(TransportVirturalTest, ut_function_for_batchsendrecv_virtural)
{
    std::string collectiveId = "test_collective";
 
    s32 device_id = 0;

    MachinePara machine_para;
    machine_para.deviceLogicId = device_id;
    machine_para.supportDataReceivedAck = true;
 
    std::shared_ptr<TransportVirtural> linktmp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10);
    linktmp.reset(new TransportVirtural(dispatcher, nullptr, machine_para, timeout, 0));
 
    Stream streamObj(StreamType::STREAM_TYPE_OFFLINE);
    HcclResult ret = linktmp->TxPrepare(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = linktmp->RxPrepare(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = linktmp->TxData(UserMemType::INPUT_MEM, 0, nullptr, 0, streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = linktmp->RxData(UserMemType::INPUT_MEM, 0, nullptr, 0, streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = linktmp->TxDone(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = linktmp->RxDone(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(TransportVirturalTest, st_function_for_batchsendrecv_heterog_p2p)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);
    MachinePara machinePara;
 
    std::chrono::milliseconds timeout;
    const std::string tag;
 
    std::shared_ptr<TransportHeterogP2P> linktmp = nullptr;
    linktmp.reset(new TransportHeterogP2P(dispatcher, nullptr, machinePara, timeout));

    u32 status = HETEROG_P2P_WAIT;
    MOCKER_CPP_VIRTUAL(*linktmp.get(), &TransportHeterogP2P::ConnectQuerry)
    .stubs()
    .with(outBound(status))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .will(returnValue(2));

    linktmp->Init();
    linktmp->TxPrepare(stream);
    linktmp->RxPrepare(stream);
    linktmp->TxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->RxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->TxDone(stream);
    linktmp->RxDone(stream);
}
