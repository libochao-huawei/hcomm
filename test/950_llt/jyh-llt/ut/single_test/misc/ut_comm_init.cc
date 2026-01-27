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
 
#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <hccl/hccl.h>
 
#define private public
#define protected public
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
#include "hccl_communicator.h"
#include "transport_ibverbs_pub.h"
#undef protected
#undef private
#include "llt_hccl_stub_pub.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../inc/hccl/base.h"
#include "../inc/hccl/hccl_ex.h"
#include <hccl/hccl.h>
#include <hccl/hccl_types.h>
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include <unistd.h>
#include "externalinput_pub.h"
#include "externalinput.h"
#include "op_base.h"
#include "hcom_pub.h"
#include "comm_config_pub.h"
#include "transport_manager.h"
#include "dlra_function.h"
#include "device_capacity.cc"
#define private public
#include "transport_remote_access.h"
#include "hccl_comm_conn.h"
#include "peterson_lock.h"
#undef private
using namespace std;
using namespace hccl;
 
class CommInitTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--CommInitTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--CommInitTest TearDown--\033[0m" << std::endl;
    }
 
    virtual void SetUp()
    {
        std::cout << "CommInitTest Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "CommInitTest Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
 
HcclDispatcher CommInitTest::dispatcherPtr = nullptr;
DispatcherPub *CommInitTest::dispatcher = nullptr;
 
TEST_F(CommInitTest, ut_HcclCommInitRootInfoConfig_not_set_config_01)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    HcclRootInfo id;
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclRdmaTrafficClass = 0xffffffff;
    config.hcclRdmaServiceLevel = 0xffffffff;
    strcpy_s(config.hcclCommName, 128, HCCL_WORLD_GROUP);

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_HcclCommInitRootInfoConfig_not_set_config_02)
{
    HcclRootInfo id;
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclRdmaTrafficClass = 0xffffffff;
    config.hcclRdmaServiceLevel = 0xffffffff;
    strcpy_s(config.hcclCommName, 128, HCCL_WORLD_GROUP);
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_HcclCommInitRootInfoConfig_by_config_file)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    HcclRootInfo id;
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclRdmaTrafficClass = 132;
    config.hcclRdmaServiceLevel = 4;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_HcclCommInitRootInfoConfig_by_config_file_sl)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    HcclRootInfo id;
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclRdmaTrafficClass = 0xffffffff;
    config.hcclRdmaServiceLevel = 4;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_HcclCommInitRootInfoConfig_by_config_file_tc)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    HcclRootInfo id;
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclRdmaTrafficClass = 132;
    config.hcclRdmaServiceLevel = 0xffffffff;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_HcclCommInitRootInfoConfig_in_valiad_para)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    HcclRootInfo id;
    // 设置op base 模式
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclRdmaTrafficClass = 256;
    config.hcclRdmaServiceLevel = 256;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_E_PARA);
 
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_HcclCommInitRootInfoConfig_in_boundary_value)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    HcclRootInfo id;
    // 设置op base 模式
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
 
    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclCommConfig config;
    HcclCommConfigInit(&config);
 
    config.hcclRdmaTrafficClass = 0;
    config.hcclRdmaServiceLevel = 0;
    strcpy_s(config.hcclCommName, COMM_NAME_MAX_LENGTH, "comm1");
 
    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_transport_ibverbs_createoneqp_not_set_config)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    MachinePara machinePara;
    machinePara.tc = 0xffffffff;
    machinePara.sl = 0xffffffff;
    machinePara.localDeviceId = 0;
 
    HcclIpAddress remoteIp{};
    std::chrono::milliseconds timeout;
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);
 
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    HcclResult ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);
 
    MOCKER_CPP(&TransportIbverbs::IsUseQpCreateWithAttrs).stubs().will(returnValue(HCCL_SUCCESS));
 
    MOCKER(HrtRaQpCreate).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
 
    MOCKER(SetQpAttrTimeOut).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
 
    MOCKER(SetQpAttrRetryCnt).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
 
    std::shared_ptr<TransportIbverbs> IbvPtr = nullptr;
    IbvPtr.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    QpHandle qpHandle;
    AiQpInfo qpinfo;
    IbvPtr->CreateOneQp(4, 1, qpHandle, qpinfo, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}
 
TEST_F(CommInitTest, ut_transport_ibverbs_createoneqp_by_config_file)
{
    setenv("HCCL_RDMA_TC", "120", 1);
    setenv("HCCL_RDMA_SL", "2", 1);
    MachinePara machinePara;
    machinePara.tc = 80;
    machinePara.sl = 2;
    machinePara.localDeviceId = 0;
 
    HcclIpAddress remoteIp{};
    std::chrono::milliseconds timeout;
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);
 
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    HcclResult ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);
 
    MOCKER_CPP(&TransportIbverbs::IsUseQpCreateWithAttrs).stubs().will(returnValue(HCCL_SUCCESS));
 
    MOCKER(HrtRaQpCreate).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
 
    MOCKER(SetQpAttrTimeOut).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
 
    MOCKER(SetQpAttrRetryCnt).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
 
    std::shared_ptr<TransportIbverbs> IbvPtr = nullptr;
    IbvPtr.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));
 
    QpHandle qpHandle;
    AiQpInfo qpinfo;
    IbvPtr->CreateOneQp(4, 2, qpHandle, qpinfo, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
    unsetenv("HCCL_RDMA_TC");
    unsetenv("HCCL_RDMA_SL");
    GlobalMockObject::verify();
}

TEST_F(CommInitTest, ut_device_CheckDeviceType_Error)
{
s32 ret = HCCL_SUCCESS;
ret = CheckDeviceType(DevType::DEV_TYPE_COUNT);
EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CommInitTest, ut_CreateQp_Error)
{
    s32 ret = HCCL_SUCCESS;
    std::string tag = "null";
    s32 deviceLogicId = 0;
    void *dispatcherPtr = nullptr;
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    RemoteAccessPara remoteaccesspara;
    vector<MemRegisterAddr> addrInfos;
    TransportRemoteAccess trasport(tag, dispatcherPtr, notifyPool, remoteaccesspara, addrInfos, deviceLogicId);

    MOCKER(HrtRaQpCreate).stubs().with().will(returnValue(HCCL_E_NETWORK));
    ret = trasport.CreateQp();
    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);

    GlobalMockObject::verify();
}

TEST_F(CommInitTest, ut_Connect_Error)
{
    s32 ret = HCCL_SUCCESS;
    HcclCommConn conn;
    conn.isListen_ = true;
    conn.role_ = 2;
    HcclAddr connectAddr;
    ret = conn.Connect(connectAddr);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    GlobalMockObject::verify();
}

TEST_F(CommInitTest, ut_PetersonLock_Error)
{
    s32 ret = HCCL_SUCCESS;
    void *devPtr = nullptr;
    PetersonLock lock(1);
    u32 selfFlag = 1;
    u32 peerFlag = 1;
    u32 turn = 1;
    MOCKER(hrtMemSyncCopy).stubs().with().will(returnValue(HCCL_E_NOT_SUPPORT));

    ret = lock.WriteSelfFlag(selfFlag);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret = lock.WriteTurn();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret = lock.ReadPeerFlag(peerFlag);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret = lock.ReadTurn(turn);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    GlobalMockObject::verify();
}

