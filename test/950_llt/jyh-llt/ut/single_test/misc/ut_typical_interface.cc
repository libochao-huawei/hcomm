#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <sys/time.h>
#include "adapter_rts.h"
#define private public
#define protected public
#include "interface_hccl.h"
#include "typical_qp_manager.h"
#include "typical_window_mem.h"
#include "typical_sync_mem.h"
#include "typical_mr_manager.h"
#include "rdma_resource_manager.h"
#include "send_recv_executor.h"
#undef private
#undef protected
 
#include "dlra_function.h" 

using namespace hccl;
 
class TypicalInterfaceTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "TypicalInterfaceTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TypicalInterfaceTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        DlRaFunction::GetInstance().DlRaFunctionInit();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

HcclResult stub_hrtRaGetCqeErrInfoList(RdmaHandle handle, struct CqeErrInfo *infolist, unsigned int *num)
{
    *num = 1;
    infolist[0].qpn=1;
    infolist[0].status = 12;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    infolist[0].time = tv;
    return HCCL_SUCCESS;
}

HcclResult stub_HrtRaGetNotifyBaseAddr_2(RdmaHandle handle, u64 *va, u64 *size)
{
    HCCL_ERROR("stub_HrtRaGetNotifyBaseAddr");
    *va = 0x20000000;
    *size = 4;
    return HCCL_SUCCESS;
}
 
TEST_F(TypicalInterfaceTest, ut_interface_create_ascend_qp_test)
{
    MOCKER(hrtRaTypicalQpCreate).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaGetNotifyBaseAddr).stubs().will(invoke(stub_HrtRaGetNotifyBaseAddr_2));
    HcclResult ret = RdmaResourceManager::GetInstance().Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    AscendQPInfo QPInfo = {};
    ret = hcclCreateAscendQP(&QPInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalInterfaceTest, ut_interface_modify_ascend_qp_test)
{
    MOCKER_CPP(&TypicalQpManager::ModifyQp)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AscendQPInfo localQPInfo = {};
    AscendQPInfo remoteQPInfo = {};
    HcclResult ret = hcclModifyAscendQP(&localQPInfo, &remoteQPInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    AscendQPQos qpQos;
    qpQos.sl = 4;
    qpQos.tc = 4;
    ret = hcclModifyAscendQPEx(&localQPInfo, &remoteQPInfo, &qpQos);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalInterfaceTest, ut_interface_destroy_ascend_qp_test)
{
    MOCKER_CPP(&TypicalQpManager::DestroyQp)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AscendQPInfo QPInfo = {};
    HcclResult ret = hcclDestroyAscendQP(&QPInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalInterfaceTest, ut_interface_alloc_free_window_mem_test)
{
    MOCKER_CPP(&TypicalWindowMem::AllocWindowMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtMemSyncCopy)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalWindowMem::FreeWindowMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    void *ptr;
    size_t size = 8;
    HcclResult ret = hcclAllocWindowMem(&ptr, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclFreeWindowMem(ptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalInterfaceTest, ut_interface_alloc_free_sync_mem_test)
{
    MOCKER_CPP(&TypicalSyncMem::AllocSyncMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::FreeSyncMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtMemSyncCopy)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    int32_t *ptr;
    HcclResult ret = hcclAllocSyncMem(&ptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclFreeSyncMem(ptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalInterfaceTest, ut_interface_register_deregister_mem_test)
{
    MOCKER_CPP(&TypicalMrManager::RegisterMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalMrManager::DeRegisterMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AscendMrInfo mrInfo = {};
    mrInfo.addr = 0x12345678;
    mrInfo.size = 8;
    HcclResult ret = hcclRegisterMem(&mrInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclDeRegisterMem(&mrInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalInterfaceTest, ut_interface_send_by_ascend_qp_test)
{
    MOCKER_CPP(&TypicalQpManager::GetQpHandleByQpn)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendRecvExecutor::Init)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendRecvExecutor::Send)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    void* sendBuf = (void *)0x12345678;
    uint64_t count = 1024;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    aclrtStream stream = (aclrtStream)0x87654321;

    AscendQPInfo fakeQpInfo = {};
    fakeQpInfo.qpn = 1234;
    fakeQpInfo.gidIdx = 4321;
    fakeQpInfo.psn = 222;

    AscendMrInfo fakeMrInfo = {};
    fakeMrInfo.addr = 0x1111111;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 3;

    AscendSendRecvInfo sendRecvInfo = {};
    sendRecvInfo.localQPinfo = &fakeQpInfo;
    sendRecvInfo.localWindowMem = &fakeMrInfo;
    sendRecvInfo.remoteWindowMem = &fakeMrInfo;
    sendRecvInfo.localSyncMemPrepare = &fakeMrInfo;
    sendRecvInfo.localSyncMemDone = &fakeMrInfo;
    sendRecvInfo.localSyncMemAck = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemPrepare = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemDone = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemAck = &fakeMrInfo;

    HcclResult ret = HcclSendByAscendQP(sendBuf, count, dataType, &sendRecvInfo, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalInterfaceTest, ut_interface_recv_by_ascend_qp_test)
{
    MOCKER_CPP(&TypicalQpManager::GetQpHandleByQpn)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendRecvExecutor::Init)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&SendRecvExecutor::Receive)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    void* recvBuf = (void *)0x12345678;
    uint64_t count = 1024;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    aclrtStream stream = (aclrtStream)0x87654321;

    AscendQPInfo fakeQpInfo = {};
    fakeQpInfo.qpn = 1234;
    fakeQpInfo.gidIdx = 4321;

    fakeQpInfo.psn = 222;

    AscendMrInfo fakeMrInfo = {};
    fakeMrInfo.addr = 0x1111111;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 3;

    AscendSendRecvInfo sendRecvInfo = {};
    sendRecvInfo.localQPinfo = &fakeQpInfo;
    sendRecvInfo.localWindowMem = &fakeMrInfo;
    sendRecvInfo.remoteWindowMem = &fakeMrInfo;
    sendRecvInfo.localSyncMemPrepare = &fakeMrInfo;
    sendRecvInfo.localSyncMemDone = &fakeMrInfo;
    sendRecvInfo.localSyncMemAck = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemPrepare = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemDone = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemAck = &fakeMrInfo;

    HcclResult ret = HcclRecvByAscendQP(recvBuf, count, dataType, &sendRecvInfo, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalInterfaceTest, ut_interface_put_by_ascend_qp_test)
{
    MOCKER_CPP(&TypicalQpManager::GetQpHandleByQpn)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&SendRecvExecutor::Init)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&SendRecvExecutor::Put)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    void* putBuf = (void *)0x12345678;
    uint64_t count = 1024;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    aclrtStream stream = (aclrtStream)0x87654321;
    AscendQPInfo fakeQpInfo = {};
    fakeQpInfo.qpn = 1234;
    fakeQpInfo.gidIdx = 4321;
 
    fakeQpInfo.psn = 222;
 
    AscendMrInfo fakeMrInfo = {};
    fakeMrInfo.addr = 0x1111111;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 3;
 
    AscendSendRecvInfo sendRecvInfo = {};
    sendRecvInfo.localQPinfo = &fakeQpInfo;
    sendRecvInfo.localWindowMem = &fakeMrInfo;
    sendRecvInfo.remoteWindowMem = &fakeMrInfo;
    sendRecvInfo.localSyncMemPrepare = &fakeMrInfo;
    sendRecvInfo.localSyncMemDone = &fakeMrInfo;
    sendRecvInfo.localSyncMemAck = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemPrepare = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemDone = &fakeMrInfo;
    sendRecvInfo.remoteSyncMemAck = &fakeMrInfo;
 
    HcclResult ret = HcclPutByAscendQP(putBuf, count, dataType, &sendRecvInfo, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalInterfaceTest, ut_interface_batch_put_by_ascend_qp_test)
{
    MOCKER_CPP(&SendRecvExecutor::WaitSignal)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&TypicalQpManager::GetQpHandleByQpn)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&SendRecvExecutor::Init)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(HrtRaSendWrV2)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtRDMADBSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    void* putBuf = (void *)0x12345678;
    uint64_t count = 1024;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    aclrtStream stream = (aclrtStream)0x87654321;
    AscendQPInfo fakeQpInfo = {};
    fakeQpInfo.qpn = 1234;
    fakeQpInfo.gidIdx = 4321;
 
    fakeQpInfo.psn = 222;
 
    AscendMrInfo fakeMrInfo = {};
    fakeMrInfo.addr = 0x1111111;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 3;
 
    AscendMrInfo fakeLocalPutMrInfo = {};
    fakeMrInfo.addr = 0x11111111;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 4;
 
    AscendMrInfo fakeRemotePutMrInfo = {};
    fakeMrInfo.addr = 0x22222222;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 4;
 
    AscendSendRecvLinkInfo sendRecvLinkInfo = {};
    sendRecvLinkInfo.localQPinfo = &fakeQpInfo;
    sendRecvLinkInfo.localSyncMemPrepare = &fakeMrInfo;
    sendRecvLinkInfo.localSyncMemDone = &fakeMrInfo;
    sendRecvLinkInfo.localSyncMemAck = &fakeMrInfo;
    sendRecvLinkInfo.remoteSyncMemPrepare = &fakeMrInfo;
    sendRecvLinkInfo.remoteSyncMemDone = &fakeMrInfo;
    sendRecvLinkInfo.remoteSyncMemAck = &fakeMrInfo;
    sendRecvLinkInfo.wqePerDoorbell = 2;
 
    HcclResult ret = HcclBatchPutMRByAscendQP(1, &fakeLocalPutMrInfo, &fakeRemotePutMrInfo, &sendRecvLinkInfo, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalInterfaceTest, ut_interface_wait_batch_put_by_ascend_qp_test)
{
    MOCKER_CPP(&SendRecvExecutor::WaitSignal)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&TypicalQpManager::GetQpHandleByQpn)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&SendRecvExecutor::Init)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    struct MrInfoT notifySrcMem;
    notifySrcMem.addr = (void*)0x1111111;

    MOCKER_CPP(&TypicalSyncMem::GetNotifySrcMem)
    .stubs()
    .with(outBound(notifySrcMem))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TypicalSyncMem::GetNotifyHandle)
    .stubs()
    .with(any(), outBound((void*)0x11))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetNotifySize)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtNotifyWaitWithTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(HrtRaSendWrV2)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(hrtRDMADBSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    void* putBuf = (void *)0x12345678;
    uint64_t count = 1024;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    aclrtStream stream = (aclrtStream)0x87654321;
    AscendQPInfo fakeQpInfo = {};
    fakeQpInfo.qpn = 1234;
    fakeQpInfo.gidIdx = 4321;
 
    fakeQpInfo.psn = 222;
 
    AscendMrInfo fakeMrInfo = {};
    fakeMrInfo.addr = 0x1111111;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 3;
 
    AscendMrInfo fakeLocalPutMrInfo = {};
    fakeMrInfo.addr = 0x11111111;
    fakeMrInfo.size = 8;
    fakeMrInfo.key = 4;
 
    AscendSendRecvLinkInfo sendRecvLinkInfo = {};
    sendRecvLinkInfo.localQPinfo = &fakeQpInfo;
    sendRecvLinkInfo.localSyncMemPrepare = &fakeMrInfo;
    sendRecvLinkInfo.localSyncMemDone = &fakeMrInfo;
    sendRecvLinkInfo.localSyncMemAck = &fakeMrInfo;
    sendRecvLinkInfo.remoteSyncMemPrepare = &fakeMrInfo;
    sendRecvLinkInfo.remoteSyncMemDone = &fakeMrInfo;
    sendRecvLinkInfo.remoteSyncMemAck = &fakeMrInfo;
    sendRecvLinkInfo.wqePerDoorbell = 2;
 
    HcclResult ret = HcclWaitPutMRByAscendQP(&sendRecvLinkInfo, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclWaitPutMRDoWait(&sendRecvLinkInfo, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclWaitPutMRDoRecord(&sendRecvLinkInfo, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalInterfaceTest, ut_interface_hcclrdma_init_and_deInit)
{
    void* mrHandle = (void*)0x11;
    MOCKER(hrtRaRegGlobalMr)
    .stubs()
    .with(outBound(mrHandle))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaDeRegGlobalMr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = hcclAscendRdmaInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclAscendRdmaDeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalInterfaceTest, ut_interface_get_err_cqe_list)
{
    void* mrHandle = (void*)0x11;
    MOCKER(hrtRaRegGlobalMr)
    .stubs()
    .with(outBound(mrHandle))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaDeRegGlobalMr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaGetCqeErrInfoList)
    .stubs().
    will(invoke(stub_hrtRaGetCqeErrInfoList));
    u32 num = 1;
    struct HcclErrCqeInfo info[1] = {};
    HcclResult ret = hcclAscendRdmaInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclGetCqeErrInfoList(info, &num);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(num, 1);
    ret = HcclGetCqeErrInfoListByQpn(1, info, &num);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(num, 1);
    EXPECT_EQ(info[0].status, 12);
    EXPECT_EQ(info[0].qpn, 1);
    ret = hcclAscendRdmaDeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}