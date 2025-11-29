/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <runtime/rt.h>


#include <cassert>
#include <semaphore.h>
#include <csignal>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <cerrno>
#include <cce/dnn.h>
#include <securec.h>


#include <sys/types.h>
#include <cstddef>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#include "dlra_function.h"
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#define private public
#define protected public
#include "transport_ibverbs_pub.h"
#include "sender_pub.h"
#include "reducer_pub.h"
#include "network_manager_pub.h"
#undef protected
#undef private
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "rank_consistentcy_checker.h"
#include "profiler_manager.h"
#include "externalinput.h"
#include "hccl_api_data.h"
#include "hccl_primitive_local.h"
#include "hccl_primitive_remote.h"
#include "hccl_dispatcher_ctx.h"
#include "dispatcher_ctx.h"
#include "hccl_thread.h"


using namespace std;
using namespace hccl;

extern HcclResult CommTaskPrepare(char *key, uint32_t keyLen);
extern HcclResult CommTaskLaunch(ThreadHandle *threads, uint32_t threadNum);

class MPI_Link_Ibv_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_Link_Ibv_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Link_Ibv_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "MPI_Link_ibv_exp_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        GlobalMockObject::verify();
        std::cout << "MPI_Link_Ibv_Test TearDown" << std::endl;
    }
};

const char *ibv_exp_server_ip  = "192.168.1.62";
const char *ibv_exp_client_ip  = "192.168.1.63";

// 注：socket 与 rdma返回相同的值
void* InitRaNicResource(u32 deviceId, const char *deviceIp, void *rdmaHandle)
{
    HcclResult ret;
    HcclIpAddress ipaddr(deviceIp);
    CHK_PRT_RET(ipaddr.IsInvalid(), HCCL_ERROR("ip[%s] change failed", deviceIp), nullptr);

    struct rdev nicRdevInfo;
    nicRdevInfo.phy_id = deviceId;
    nicRdevInfo.family = ipaddr.IsIPv6() ? AF_INET6 : AF_INET;
    nicRdevInfo.local_ip.addr = ipaddr.GetBinaryAddress().addr;
    nicRdevInfo.local_ip.addr6 = ipaddr.GetBinaryAddress().addr6;
    ret = hrtRaSocketInit(NETWORK_OFFLINE, nicRdevInfo, rdmaHandle);
    CHK_PRT_RET(ret, HCCL_ERROR("errNo[0x%016llx] deviceId[%d] init nic resource failed", HCCL_ERROR_CODE(ret), deviceId), nullptr);
    return rdmaHandle;
}

extern std::unique_ptr<NotifyPool> get_notifyPool(s32 rank);
#if 1
TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_init)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);
    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.tag = "my tag";

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    char strTag[100] = "my tag";
    char strGroup[100] = "my group";
    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";
    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);
    HCCL_INFO(" RankConsistentcyChecker information set SUCCESS[INFO]");

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /* 让dispatcher在link之后析构 */
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    TsdClose(1);
}

TEST_F(MPI_Link_Ibv_Test, ut_mpi_aicpuMc2_link_exp_init)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);
    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.tag = "my tag";


    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank) {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    } else if (1 == rank) {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank) {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    } else if (1 == rank) {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    char strTag[100] = "my tag";
    char strGroup[100] = "my group";
    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";
    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);
    HCCL_INFO(" RankConsistentcyChecker information set SUCCESS[INFO]");

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    GlobalMockObject::verify();

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /* 让dispatcher在link之后析构 */
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    TsdClose(1);
}
#endif

#if 1
TEST_F(MPI_Link_Ibv_Test, ut_mpi_notify_test1)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//1910
    machine_para.tag = "my tag";
    machine_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }

    IpSocket ipsocketInfo;
    ipsocketInfo.nicSocketHandle = (void*)0x00000001;
    ipsocketInfo.nicRdmaHandle = nicRdmaHandle;
    machine_para.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(machine_para.deviceLogicId).raResourceInfo_;
    raResourceInfo.nicSocketMap.insert({machine_para.localIpAddr, ipsocketInfo});

    std::vector<std::vector<HcclSocketInfo>> socketsInfo;
    std::vector<HcclSocketInfo> socketInfoVer;
    HcclSocketInfo socketInfo;
    socketInfo.socketHandle = (void*)0x00000001;
    socketInfoVer.push_back(socketInfo);
    socketsInfo.push_back(socketInfoVer);

    const std::string tag = "my tag";

    hrtSetDevice(rank);

    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));
    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(server )，发送数据给rank 1(client)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret = link_exp->RxAck(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);

	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    raResourceInfo.nicSocketMap.clear();

    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}
#endif

#if 1
TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_client_tx_81)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);
    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);
    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910B;//V80
    machine_para.tag = "my tag";



    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    HostMem host_mem =  HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello server from client";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(client)，发送数据给rank 1(server)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret =  link_exp->RxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxWithReduce(UserMemType::OUTPUT_MEM, 0, tx_buf.ptr(), 1,
                                     HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = outputMem;

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxDataSignal(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}
#endif
#if 1
TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_op_base)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 设置为单算子模式
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    hrtSetDevice(rank);
    /*创建dispatcher*/
    SetFftsSwitch(false);
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//V80
    machine_para.tag = "my tag";


    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));
   
    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    HostMem host_mem =  HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello server from client";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(client)，发送数据给rank 1(server)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret =  link_exp->RxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = outputMem;

        ret = link_exp->TxAck(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 设置回图模式
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);
    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}

TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_server_tx)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);
    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//1910
    machine_para.tag = "my tag";

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(server )，发送数据给rank 1(client)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret = link_exp->RxAck(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}

TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_create_link)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, device_id, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);
    RaResourceInfo raResourceInfo;


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.tag = "my tag";

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    /*创建link*/
    std::shared_ptr<Transport> link_exp = nullptr;
    link_exp.reset(new (std::nothrow) Transport(new (std::nothrow) TransportIbverbs(
        dispatcher, notifyPool, machine_para, timeout)));

    EXPECT_NE(link_exp, nullptr);

    MPI_Barrier(MPI_COMM_WORLD);

	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}


TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_get_qp_status_timeout)
{
    s32 ret;
    s32 size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DevType chipType = DevType::DEV_TYPE_910;
    hrtSetDevice(rank);
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.tag = "my tag";

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    char strTag[100] = "my tag";
    char strGroup[100] = "my group";
    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";
    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);
    HCCL_INFO(" RankConsistentcyChecker information set SUCCESS[INFO]");

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    setenv("HCCL_CONNECT_TIMEOUT", "1", 1);
    MOCKER(hrtGetRaQpStatus)
    .expects(atMost(2000))
    .will(returnValue(1));
    ret = link_exp->Init();
    EXPECT_NE(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    unsetenv("HCCL_CONNECT_TIMEOUT");
    TsdClose(1);
}

TEST_F(MPI_Link_Ibv_Test, ut_mpi_RevAndCheckCrcInfo_status_timeout)
{
    s32 ret,resRet;
    s32 size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DevType chipType = DevType::DEV_TYPE_910;
    hrtSetDevice(rank);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);

    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.tag = "my tag";


    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(0);

    char strTag[100] = "my tag";
    char strGroup[100] = "my group";
    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";
    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);
    HCCL_INFO(" RankConsistentcyChecker information set SUCCESS[INFO]");

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    SaluSleep(1000);
    resRet = link_exp->Init();
    EXPECT_EQ(resRet, HCCL_E_TIMEOUT);

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    TsdClose(1);
}

TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_multi)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//1910
    machine_para.tag = "my tag";
    machine_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }


    IpSocket ipsocketInfo;
    ipsocketInfo.nicSocketHandle = (void*)0x00000001;
    ipsocketInfo.nicRdmaHandle = nicRdmaHandle;
    machine_para.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(machine_para.deviceLogicId).raResourceInfo_;
    raResourceInfo.nicSocketMap.insert({machine_para.localIpAddr, ipsocketInfo});

    std::vector<std::vector<HcclSocketInfo>> socketsInfo;
    std::vector<HcclSocketInfo> socketInfoVer;
    HcclSocketInfo socketInfo;
    socketInfo.socketHandle = (void*)0x00000001;
    socketInfoVer.push_back(socketInfo);
    socketsInfo.push_back(socketInfoVer);

    const std::string tag = "my tag";

    hrtSetDevice(rank);

    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));
    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(server )，发送数据给rank 1(client)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret = link_exp->RxAck(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;
        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);

	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    raResourceInfo.nicSocketMap.clear();

    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}

#if 1
TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_multi_msg)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//1910
    machine_para.tag = "my tag";
    machine_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;    

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }

    IpSocket ipsocketInfo;
    ipsocketInfo.nicSocketHandle = (void*)0x00000001;
    ipsocketInfo.nicRdmaHandle = nicRdmaHandle;
    machine_para.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(machine_para.deviceLogicId).raResourceInfo_;
    raResourceInfo.nicSocketMap.insert({machine_para.localIpAddr, ipsocketInfo});

    std::vector<std::vector<HcclSocketInfo>> socketsInfo;
    std::vector<HcclSocketInfo> socketInfoVer;
    HcclSocketInfo socketInfo;
    socketInfo.socketHandle = (void*)0x00000001;
    socketInfoVer.push_back(socketInfo);
    socketsInfo.push_back(socketInfoVer);

    const std::string tag = "my tag";

    hrtSetDevice(rank);

    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));
    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(server )，发送数据给rank 1(client)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        std::vector<TxMemoryInfo> txMems;
        txMems.emplace_back(TxMemoryInfo{UserMemType::INPUT_MEM, 0, tx_buf.ptr(), msg_len});

        ret = link_exp->TxAsync(txMems, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxAck(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;

        std::vector<RxMemoryInfo> rxMems;
        rxMems.emplace_back(RxMemoryInfo{UserMemType::INPUT_MEM, 0, rx_buf.ptr(), msg_len});

        ret = link_exp->RxAsync(rxMems, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);

	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    raResourceInfo.nicSocketMap.clear();

    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}
#endif

#if 1
TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_sender_reducer_multi_msg)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//1910
    machine_para.tag = "my tag";
    machine_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }

    IpSocket ipsocketInfo;
    ipsocketInfo.nicSocketHandle = (void*)0x00000001;
    ipsocketInfo.nicRdmaHandle = nicRdmaHandle;
    machine_para.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(machine_para.deviceLogicId).raResourceInfo_;
    raResourceInfo.nicSocketMap.insert({machine_para.localIpAddr, ipsocketInfo});

    std::vector<std::vector<HcclSocketInfo>> socketsInfo;
    std::vector<HcclSocketInfo> socketInfoVer;
    HcclSocketInfo socketInfo;
    socketInfo.socketHandle = (void*)0x00000001;
    socketInfoVer.push_back(socketInfo);
    socketsInfo.push_back(socketInfoVer);

    const std::string tag = "my tag";

    hrtSetDevice(rank);

    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    std::shared_ptr<Transport> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new Transport(new (std::nothrow) TransportIbverbs(
        dispatcher, notifyPool, machine_para, timeout)));
    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(server )，发送数据给rank 1(client)*/
    if (0 == rank)
    {
        Sender* sender = new Sender(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 1);

        DeviceMem src =  DeviceMem::alloc(256);

        std::vector<SenderMemoryInfo> senderMems;
        senderMems.emplace_back(SenderMemoryInfo{0, src});

        ret = sender->run(link_exp, senderMems, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxAck(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        delete sender;
    }
    else if (1 == rank)
    {
        Reducer* reducer = new Reducer(HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 1);

        DeviceMem local_src =  DeviceMem::alloc(256);
        DeviceMem local_dst = DeviceMem::alloc(256);
        DeviceMem remote_rev = DeviceMem::alloc(256);

        std::vector<ReducerMemoryInfo> reduceMems;
        reduceMems.emplace_back(ReducerMemoryInfo{0, local_src, local_dst, remote_rev});

        ret = reducer->run(dispatcher, link_exp, reduceMems, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxDataSignal(stream);
    	EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        delete reducer;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);

	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    raResourceInfo.nicSocketMap.clear();

    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}
#endif

TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_multi_v81)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910B;//1981
    machine_para.tag = "my tag";
    machine_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }

    IpSocket ipsocketInfo;
    ipsocketInfo.nicSocketHandle = (void*)0x00000001;
    ipsocketInfo.nicRdmaHandle = nicRdmaHandle;
    machine_para.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(machine_para.deviceLogicId).raResourceInfo_;
    raResourceInfo.nicSocketMap.insert({machine_para.localIpAddr, ipsocketInfo});

    std::vector<std::vector<HcclSocketInfo>> socketsInfo;
    std::vector<HcclSocketInfo> socketInfoVer;
    HcclSocketInfo socketInfo;
    socketInfo.socketHandle = (void*)0x00000001;
    socketInfoVer.push_back(socketInfo);
    socketsInfo.push_back(socketInfoVer);

    const std::string tag = "my tag";

    hrtSetDevice(rank);

    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));
    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(server )，发送数据给rank 1(client)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret = link_exp->TxWithReduce(UserMemType::OUTPUT_MEM, 0, tx_buf.ptr(), 1,
                                     HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxDataSignal(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxDataSignal(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);

	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    raResourceInfo.nicSocketMap.clear();

    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    TsdClose(1);
}

#if 1
TEST_F(MPI_Link_Ibv_Test, ut_mpi_link_exp_multi_msg_v81)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910B;//1981
    machine_para.tag = "my tag";
    machine_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }

    IpSocket ipsocketInfo;
    ipsocketInfo.nicSocketHandle = (void*)0x00000001;
    ipsocketInfo.nicRdmaHandle = nicRdmaHandle;
    machine_para.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(machine_para.deviceLogicId).raResourceInfo_;
    raResourceInfo.nicSocketMap.insert({machine_para.localIpAddr, ipsocketInfo});

    std::vector<std::vector<HcclSocketInfo>> socketsInfo;
    std::vector<HcclSocketInfo> socketInfoVer;
    HcclSocketInfo socketInfo;
    socketInfo.socketHandle = (void*)0x00000001;
    socketInfoVer.push_back(socketInfo);
    socketsInfo.push_back(socketInfoVer);

    const std::string tag = "my tag";

    hrtSetDevice(rank);

    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));
    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;

    /*rank 0(server )，发送数据给rank 1(client)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        std::vector<TxMemoryInfo> txMems;
        txMems.emplace_back(TxMemoryInfo{UserMemType::INPUT_MEM, 0, tx_buf.ptr(), msg_len});

        ret = link_exp->TxAsync(txMems, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        std::vector<TxMemoryInfo> txWithReduceMems;
        txWithReduceMems.emplace_back(TxMemoryInfo{UserMemType::OUTPUT_MEM, 0, tx_buf.ptr(), msg_len});

        ret = link_exp->TxWithReduce(txWithReduceMems, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxDataSignal(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;
        DeviceMem &out_buf = outputMem;

        std::vector<RxMemoryInfo> rxMems;
        rxMems.emplace_back(RxMemoryInfo{UserMemType::INPUT_MEM, 0, rx_buf.ptr(), msg_len});

        ret = link_exp->RxAsync(rxMems, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        std::vector<RxWithReduceMemoryInfo> rxWithReduceMems;
        rxWithReduceMems.emplace_back(RxWithReduceMemoryInfo{UserMemType::INPUT_MEM, 0, rx_buf.ptr(), msg_len,
            out_buf.ptr(), out_buf.ptr(), msg_len});

        ret = link_exp->RxWithReduce(rxWithReduceMems, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream, 0);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->RxDataSignal(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);

	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    raResourceInfo.nicSocketMap.clear();

    /*让dispatcher在link之后析构*/
    link_exp = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }


    TsdClose(1);
}
#endif

#endif

extern u32 NOTIFY_TIMEOUT;
HcclResult PrimitiRemoteBatchWriteRead(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank)
{
    std::string key = "PrimitiRemoteWriteWithReduce";
    HostMem hostMem = HostMem::alloc(inputMem.size());
    u32 count = 4;
    float *ptr = (float*)hostMem.ptr();
    for (uint32_t i = 0; i < count; i++) {
        ptr[i] = 1;
    }
    CHK_RET(HcclMemcpyAsync(dispatcher, inputMem.ptr(), inputMem.size(), hostMem.ptr(), hostMem.size(), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE,
        stream, 1, hccl::LinkType::LINK_ONCHIP));
    sal_memset(outputMem.ptr(), sizeof(outputMem.size()), 0, sizeof(outputMem.size()));
    CHK_RET(hcclStreamSynchronize(stream.ptr()));

    CHK_RET(CommTaskPrepare(const_cast<char*>(key.c_str()), key.length()));
    // 4个值，0写2个，1读2个
    // constexpr u32 block = 2;
    // 当前rdma桩read有问题，全部write过去
    constexpr u32 block = 4;
    if (rank == 0) {
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        u64 len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];

        HcclBufPair bufPairs[block];
        for (u32 i = 0; i < block; i++) {
            bufPairs[i].loc.addr = reinterpret_cast<u8*>(inputMem.ptr()) + i * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
            bufPairs[i].loc.len = SIZE_TABLE[HCCL_DATA_TYPE_FP32];
            bufPairs[i].rmt.addr = reinterpret_cast<u8*>(remoteOutputPtr) + i * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
            bufPairs[i].rmt.len = SIZE_TABLE[HCCL_DATA_TYPE_FP32];
        }
        CHK_RET(HcclRemoteBatchWrite(&stream, link.get(), bufPairs, block));    
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 2));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 2, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 2));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 2, NOTIFY_TIMEOUT));
        HcclBufPair bufPairs[block];
        for (u32 i = 0; i < block; i++) {
            bufPairs[i].loc.addr = reinterpret_cast<u8*>(outputMem.ptr()) + ((block + i) * SIZE_TABLE[HCCL_DATA_TYPE_FP32]);
            bufPairs[i].loc.len = SIZE_TABLE[HCCL_DATA_TYPE_FP32];
            bufPairs[i].rmt.addr = reinterpret_cast<u8*>(remoteInputPtr) + ((block + i) * SIZE_TABLE[HCCL_DATA_TYPE_FP32]);
            bufPairs[i].rmt.len = SIZE_TABLE[HCCL_DATA_TYPE_FP32];
        }
        CHK_RET(HcclRemoteBatchRead(&stream, link.get(), bufPairs, 0));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
    }
    ThreadHandle threads[1];
    threads[0] = thread;
    CHK_RET(CommTaskLaunch(threads, 1));
    CHK_RET(hcclStreamSynchronize(stream.ptr()));
    if (rank == 1) {
        CHK_RET(HcclMemcpyAsync(dispatcher, hostMem.ptr(), hostMem.size(), outputMem.ptr(), outputMem.size(), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST,
            stream, 1, hccl::LinkType::LINK_ONCHIP));
        CHK_RET(hcclStreamSynchronize(stream.ptr()));
        ptr = (float*)hostMem.ptr();
        for (uint32_t i = 0; i < count; i++) {
            if (ptr[i] != 1) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

extern HcclResult RemoteWriteWithReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank, bool isWithReduce);
extern HcclResult PrimitiRemoteWriteWithReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank, bool isWithReduce);
extern HcclResult RemoteWriteWithLocalReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank);
extern HcclResult PrimitiRemoteWriteWithLocalReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank);
TEST_F(MPI_Link_Ibv_Test, st_data_plane_interface_rdma)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);
    /*创建dispatcher*/
    bool ffts = GetExternalInputHcclEnableFfts();
    SetFftsSwitch(false);
    u32 output = 1;
    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .with(outBoundP(&output, sizeof(output)))
    .will(returnValue(HCCL_SUCCESS));
    DispatcherCtxPtr ctx;
    ret = CreateDispatcherCtx(&ctx, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DispatcherCtx *ctxPtr = static_cast<DispatcherCtx *>(ctx);
    EXPECT_NE(GetDispatcherCtx(), nullptr);
    HcclDispatcher dispatcherPtr = ctxPtr->GetDispatcher();
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    /*创建link*/
    std::string port_name = "mlx5_0";


    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910B;
    machine_para.tag = "my tag";
    machine_para.notifyNum = 3;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendBuff));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvBuff));

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketSendString));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(std::string &))
    .stubs()
    .with(any())
    .will(invoke(HcclSocketRecvString));

    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResource(0, ibv_exp_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_server_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_client_ip);
    }

    const std::string tag = "my tag";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::shared_ptr<Transport> link = nullptr;
    TransportPara para = {};
    para.timeout = std::chrono::milliseconds(1000);
    link.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP, para, dispatcher, notifyPool, machine_para));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    dynamic_cast<TransportIbverbs*>(link->pimpl_)->nicRdmaHandle_ = nicRdmaHandle;
    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步
    HcclThread mianThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY);
    ret = mianThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    Stream *stream = mianThread.GetStream();
    EXPECT_NE(stream, nullptr);
    uint64_t len = inputMem.size() * SIZE_TABLE[HCCL_DATA_TYPE_FP32];

    HcclThread subThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY);
    ret = subThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteBatchWriteRead(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclStreamSynchronize(stream->ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommInterThreadNotifyRecordOnThread(reinterpret_cast<uint64_t>(&mianThread), reinterpret_cast<uint64_t>(&subThread), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommInterThreadNotifyWaitOnThread(reinterpret_cast<uint64_t>(&subThread), 0, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcommInterThreadNotifyRecordOnThread(reinterpret_cast<uint64_t>(&subThread), reinterpret_cast<uint64_t>(&mianThread), 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommInterThreadNotifyWaitOnThread(reinterpret_cast<uint64_t>(&mianThread), 1, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclStreamSynchronize(stream->ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 count = 1;
    ret = CommWriteReduceWithNotify(reinterpret_cast<uint64_t>(&mianThread), reinterpret_cast<uint64_t>(link.get()),
        outputMem.ptr(), inputMem.ptr(), count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    HcclBuf rmtBuf{inputMem.ptr(), inputMem.size(), nullptr};
    HcclReduceInfo reduceInfo{HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM};
    ret = HcclRemoteWriteReduceWithNotify(stream, link.get(), &rmtBuf, &rmtBuf, reduceInfo, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = HcommWriteWithNotifyOnThread(reinterpret_cast<uint64_t>(&mianThread), reinterpret_cast<uint64_t>(link.get()),
        outputMem.ptr(), inputMem.ptr(), len, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = HcclRemoteWriteWithNotify(stream, link.get(), &rmtBuf, &rmtBuf, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    HcclBatchTransferInfo info;
    ret = HcclRemoteBatchTransfer(stream, link.get(), &info, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = CommFence(reinterpret_cast<uint64_t>(&mianThread), reinterpret_cast<uint64_t>(link.get()));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclRemoteFence(stream, link.get(), 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    /* 让dispatcher在link之后析构 */
    link = nullptr;
    if (ctx != nullptr) {
        ret = DestoryDispatcherCtx(ctx);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    TsdClose(1);
    SetFftsSwitch(ffts);
    GlobalMockObject::verify();
}

TEST_F(MPI_Link_Ibv_Test, st_hrt_socket_recv)
{
    int i = 0;
    FdHandle fdHandle = &i;
    HcclResult ret = HCCL_SUCCESS;

    MOCKER(ra_socket_recv)
    .expects(once())
    .will(returnValue(10));
    ret = hrtRaSocketBlockRecv(fdHandle, &i, sizeof(int));
    EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);
    GlobalMockObject::verify();
}
