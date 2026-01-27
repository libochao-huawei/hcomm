#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
 
#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dlra_function.h"
#include "externalinput_pub.h"
 
#define private public
#define protected public
#include "adapter_rts.h"
#include "send_recv_executor.h"
#include "typical_sync_mem.h"
#undef private

#include <hccl/hccl.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <iostream>
#include <fstream>

#define WINDOW_MEM_SIZE 1024
#define SEND_RECV_COUNT 1024
using namespace std;
using namespace hccl;

class TypicalSendRecvTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TypicalSendRecvTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TypicalSendRecvTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
        PrepareMem();
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }

    void PrepareMem();

public:
    struct MrInfoT testlocalWindowMem_;
    struct MrInfoT testremoteWindowMem_;

    struct MrInfoT testlocalSyncMemPrepare_;
    struct MrInfoT testlocalSyncMemDone_;
    struct MrInfoT testlocalSyncMemAck_;

    struct MrInfoT testremoteSyncMemPrepare_;
    struct MrInfoT testremoteSyncMemDone_;
    struct MrInfoT testremoteSyncMemAck_;

    struct MrInfoT testnotifySrcMem_;

};


HcclResult hrtRaRecvWrlistMock(QpHandle handle, struct RecvWrlistData *wr, unsigned int recvNum,
    unsigned int *completeNum)
{
    wr[0].wrId = 0;
    *completeNum = 1;
    return HCCL_SUCCESS;
}

s32 hrtRaPollCqMock(QpHandle handle, bool is_send_cq, unsigned int num, void *wc)
{
    struct ibv_wc* wcPtr = static_cast<struct ibv_wc*>(wc);
    wcPtr[0].status = IBV_WC_SUCCESS;
    return 1;
}

void TypicalSendRecvTest::PrepareMem()
{
    void *memptr = nullptr;
    hrtMalloc(&memptr, WINDOW_MEM_SIZE);
    testlocalWindowMem_.addr = memptr;
    testlocalWindowMem_.size = WINDOW_MEM_SIZE;
    memptr = nullptr;
    hrtMalloc(&memptr, WINDOW_MEM_SIZE);
    testremoteWindowMem_.addr = memptr;
    testremoteWindowMem_.size = WINDOW_MEM_SIZE;
}

TEST_F(TypicalSendRecvTest, ut_TypicalSendRecv_send)
{
    QpHandle qpHandle;
    QpHandle fakeQpHandle = (void *)0x1000000;
    HcclResult ret;
    rtStream_t stream;
    s8* sendbuf;
    u64 count = SEND_RECV_COUNT;
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sendbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(sendbuf, count * sizeof(s8), 0, count * sizeof(s8));
    for (int j = 0; j < count; j++)
    {
        sendbuf[j] = 2;
    }
    MOCKER(hrtMemAsyncCopy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetTaskIdAndStreamID)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclExecTimeOut)
    .stubs()
    .will(returnValue(27 * 68.0));

    MOCKER(HrtRaSendWrV2)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRDMADBSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::InitNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::GetNotifyHandle)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::GetNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    SendRecvExecutor executor(stream, fakeQpHandle, testlocalWindowMem_, testremoteWindowMem_, testlocalSyncMemPrepare_, testlocalSyncMemDone_, testlocalSyncMemAck_,
                            testremoteSyncMemPrepare_, testremoteSyncMemDone_, testremoteSyncMemAck_, 0);
    executor.prepareNotify_ = (HcclRtSignal)0x01;
    executor.ackNotify_ = (HcclRtSignal)0x01;
    executor.doneNotify_ = (HcclRtSignal)0x01;
    executor.notifySrcMem_.addr = (void*)0x01;

    ret = executor.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor.Send(sendbuf, count, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    SendRecvExecutor executorWithImm(stream, fakeQpHandle, testlocalWindowMem_, testremoteWindowMem_, testlocalSyncMemPrepare_, testlocalSyncMemDone_, testlocalSyncMemAck_,
                            testremoteSyncMemPrepare_, testremoteSyncMemDone_, testremoteSyncMemAck_, 1);
    executorWithImm.prepareNotify_ = (HcclRtSignal)0x01;
    executorWithImm.ackNotify_ = (HcclRtSignal)0x01;
    executorWithImm.doneNotify_ = (HcclRtSignal)0x01;
    executorWithImm.notifySrcMem_.addr = (void*)0x01;

    ret = executorWithImm.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executorWithImm.Send(sendbuf, count, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtStreamDestroy(stream);
    sal_free(sendbuf);

    GlobalMockObject::verify();
}

TEST_F(TypicalSendRecvTest, ut_TypicalSendRecv_recv)
{
    QpHandle fakeQpHandle = (void *)0x1000000;
    HcclResult ret;

    rtStream_t stream;
    s8* recvbuf;
    u64 count = SEND_RECV_COUNT;
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    recvbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(recvbuf, count * sizeof(s8), 0, count * sizeof(s8));

    MOCKER(hrtRaRecvWrlist)
    .stubs()
    .with(any())
    .will(invoke(hrtRaRecvWrlistMock));
    
    MOCKER(hrtRaPollCq)
    .stubs()
    .will(invoke(hrtRaPollCqMock));

    MOCKER(hrtMemAsyncCopy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetTaskIdAndStreamID)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclExecTimeOut)
    .stubs()
    .will(returnValue(27 * 68.0));

    MOCKER(HrtRaSendWrV2)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRDMADBSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::InitNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::GetNotifyHandle)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::GetNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    SendRecvExecutor executor(stream, fakeQpHandle, testlocalWindowMem_, testremoteWindowMem_, testlocalSyncMemPrepare_, testlocalSyncMemDone_, testlocalSyncMemAck_,
                            testremoteSyncMemPrepare_, testremoteSyncMemDone_, testremoteSyncMemAck_, 0);
    executor.prepareNotify_ = (HcclRtSignal)0x01;
    executor.ackNotify_ = (HcclRtSignal)0x01;
    executor.doneNotify_ = (HcclRtSignal)0x01;
    executor.notifySrcMem_.addr = (void*)0x01;

    ret = executor.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor.Receive(recvbuf, count, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    SendRecvExecutor executorWithImm(stream, fakeQpHandle, testlocalWindowMem_, testremoteWindowMem_, testlocalSyncMemPrepare_, testlocalSyncMemDone_, testlocalSyncMemAck_,
                            testremoteSyncMemPrepare_, testremoteSyncMemDone_, testremoteSyncMemAck_, 1);
    executorWithImm.prepareNotify_ = (HcclRtSignal)0x01;
    executorWithImm.ackNotify_ = (HcclRtSignal)0x01;
    executorWithImm.doneNotify_ = (HcclRtSignal)0x01;
    executorWithImm.notifySrcMem_.addr = (void*)0x01;

    ret = executorWithImm.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executorWithImm.Receive(recvbuf, count, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtStreamDestroy(stream);
    sal_free(recvbuf);

    GlobalMockObject::verify();
}

TEST_F(TypicalSendRecvTest, ut_TypicalSendRecv_send_when_user_addr_overlapped_with_window_mem_then_success)
{
    QpHandle qpHandle;
    QpHandle fakeQpHandle = (void *)0x1000000;
    HcclResult ret;
    rtStream_t stream;

    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    SendRecvExecutor executor(stream, fakeQpHandle, testlocalWindowMem_, testremoteWindowMem_, testlocalSyncMemPrepare_, testlocalSyncMemDone_, testlocalSyncMemAck_,
                            testremoteSyncMemPrepare_, testremoteSyncMemDone_, testremoteSyncMemAck_, 0);
    executor.prepareNotify_ = (HcclRtSignal)0x01;
    executor.ackNotify_ = (HcclRtSignal)0x01;
    executor.doneNotify_ = (HcclRtSignal)0x01;

    s8* sendbuf;
    u64 count = SEND_RECV_COUNT;
    sendbuf= (s8*)testlocalWindowMem_.addr;

    MOCKER(hrtMemAsyncCopy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtGetTaskIdAndStreamID)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclExecTimeOut)
    .stubs()
    .will(returnValue(27 * 68.0));

    MOCKER(HrtRaSendWrV2)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRDMADBSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::InitNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::GetNotifyHandle)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::GetNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ret = executor.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor.Send(sendbuf, count, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = executor.Send(sendbuf, count + 1, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = executor.Send(sendbuf - 1, count, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_E_PARA);

    rtStreamDestroy(stream);

    GlobalMockObject::verify();
}

TEST_F(TypicalSendRecvTest, ut_TypicalSendRecv_put)
{
    QpHandle fakeQpHandle = (void *)0x1000000;
    HcclResult ret;
 
    rtStream_t stream;
 
    MOCKER(hrtRaPollCq)
    .stubs()
    .will(invoke(hrtRaPollCqMock));
 
    MOCKER(hrtMemAsyncCopy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtGetTaskIdAndStreamID)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(GetExternalInputHcclExecTimeOut)
    .stubs()
    .will(returnValue(27 * 68.0));
 
    MOCKER(HrtRaSendWrV2)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtRDMADBSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TypicalSyncMem::InitNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TypicalSyncMem::GetNotifyHandle)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TypicalSyncMem::GetNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(HrtRaMrDereg)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    s8* putbuf;
    u64 count = SEND_RECV_COUNT;
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    putbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(putbuf, count * sizeof(s8), 0, count * sizeof(s8));
 
    SendRecvExecutor executor(stream, fakeQpHandle, testlocalWindowMem_, testremoteWindowMem_, testlocalSyncMemPrepare_, testlocalSyncMemDone_, testlocalSyncMemAck_,
                            testremoteSyncMemPrepare_, testremoteSyncMemDone_, testremoteSyncMemAck_, 0);
    executor.prepareNotify_ = (HcclRtSignal)0x01;
    executor.ackNotify_ = (HcclRtSignal)0x01;
    executor.doneNotify_ = (HcclRtSignal)0x01;
    executor.notifySrcMem_.addr = (void*)0x01;
 
    ret = executor.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor.Put(putbuf, count, HcclDataType::HCCL_DATA_TYPE_INT8);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rtStreamDestroy(stream);
    sal_free(putbuf);
 
    GlobalMockObject::verify();
}
 
TEST_F(TypicalSendRecvTest, ut_TypicalSendRecv_batchput)
{
    QpHandle fakeQpHandle = (void *)0x1000000;
    HcclResult ret;
 
    rtStream_t stream;
 
    MOCKER(hrtRaPollCq)
    .stubs()
    .will(invoke(hrtRaPollCqMock));
 
    MOCKER(hrtMemAsyncCopy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtGetTaskIdAndStreamID)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(GetExternalInputHcclExecTimeOut)
    .stubs()
    .will(returnValue(27 * 68.0));
 
    MOCKER(HrtRaSendWrV2)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtRDMADBSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TypicalSyncMem::InitNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TypicalSyncMem::GetNotifyHandle)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&TypicalSyncMem::GetNotifySrcMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(HrtRaMrDereg)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    s8* putbuf;
    u64 count = SEND_RECV_COUNT;
    rtError_t rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    putbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(putbuf, count * sizeof(s8), 0, count * sizeof(s8));
 
    SendRecvExecutor executor(stream, fakeQpHandle, testlocalWindowMem_, testremoteWindowMem_, testlocalSyncMemPrepare_, testlocalSyncMemDone_, testlocalSyncMemAck_,
                            testremoteSyncMemPrepare_, testremoteSyncMemDone_, testremoteSyncMemAck_, 0);
    executor.prepareNotify_ = (HcclRtSignal)0x01;
    executor.ackNotify_ = (HcclRtSignal)0x01;
    executor.doneNotify_ = (HcclRtSignal)0x01;
    executor.notifySrcMem_.addr = (void*)0x01;
 
    AscendMrInfo localMR[2] = {{0x9999, 8, 1024}, {0x999999, 8, 1025}};
    AscendMrInfo remoteMR[2] = {{0x999999, 8, 1026}, {0x99999999, 8, 1027}};
 
 
    ret = executor.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor.BatchPutMR(2, localMR, remoteMR);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    rtStreamDestroy(stream);
    sal_free(putbuf);
 
    GlobalMockObject::verify();
}