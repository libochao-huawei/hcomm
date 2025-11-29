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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <runtime/rt.h>


#include <assert.h>
#include <semaphore.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <cce/dnn.h>
#include <securec.h>
#define private public
#define protected public
#include "dispatcher_pub.h"
#include "dispatcher_aicpu.h"
#include "hccl_communicator.h"
#include "transport_ibverbs_pub.h"
#undef private
#undef protected

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>


#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"

#include "transport_device_ibverbs_pub.h"
#include "transport_p2p_pub.h"
#include "rank_consistentcy_checker.h"
#include "dlra_function.h"
#include "dlhns_function.h"
#include "network_manager_pub.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"
#include "remote_notify.h"
#include "externalinput.h"

using namespace std;
using namespace hccl;

int ibv_ext_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr,
				      struct ibv_send_wr **bad_wr, struct ibv_post_send_ext_attr *ext_attr,
				      struct ibv_post_send_ext_resp *ext_resp) {
    return 0;
}

class MPI_TRANSPORT_DEVICE_IBVERBS_TEST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_TRANSPORT_DEVICE_IBVERBS_TEST SetUP" << std::endl;
        localNotify.reset(new (std::nothrow) LocalIpcNotify());
        HcclResult ret = localNotify->Init(0, 0, NotifyLoadType::HOST_NOTIFY);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        MOCKER(stub_ibv_ext_post_send).stubs().will(returnValue(-12));
        MOCKER(stub_ibv_exp_post_send).stubs().will(returnValue(-12));
        DlHnsFunction::GetInstance().DlHnsFunctionInit();
        std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
        ret = localNotify->Serialize(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        remoteNotify.reset(new (std::nothrow) RemoteNotify());

        ret = remoteNotify->Init(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteNotify->Open();
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_TRANSPORT_DEVICE_IBVERBS_TEST TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        TsdOpen(1, 2);
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");

        HcclSignalInfo notifyInfo;
        notifyInfo.addr = 100;
        notifyInfo.devId = 1;
        notifyInfo.rankId = 2;
        notifyInfo.resId = 3;
        notifyInfo.tsId = 4;
        transDevIbverbsData.inputBufferPtr = nullptr;
        transDevIbverbsData.outputBufferPtr = nullptr;
        transDevIbverbsData.localInputMem.size = 10;
        transDevIbverbsData.localInputMem.addr = 0x10;
        transDevIbverbsData.localInputMem.key = 10;
        transDevIbverbsData.localOutputMem.size = 10;
        transDevIbverbsData.localOutputMem.size = 0x10;
        transDevIbverbsData.localOutputMem.size = 10;
        transDevIbverbsData.ackNotify = std::make_shared<LocalIpcNotify>();
        transDevIbverbsData.ackNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
        transDevIbverbsData.dataAckNotify = std::make_shared<LocalIpcNotify>();
        transDevIbverbsData.dataAckNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
        transDevIbverbsData.dataNotify = std::make_shared<LocalIpcNotify>();
        transDevIbverbsData.dataNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);

        u32 notifyNum = 8;
        transDevIbverbsData.userLocalNotify.resize(1);
        transDevIbverbsData.userLocalNotify[0].resize(notifyNum);
        for (u32 i=0; i<notifyNum; i++) {
            transDevIbverbsData.userLocalNotify[0][i] = std::make_shared<LocalIpcNotify>();
            transDevIbverbsData.userLocalNotify[0][i]->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
        }

        transDevIbverbsData.localNotifyValueAddr = 0x1000;

        transDevIbverbsData.remoteAckNotifyDetails.addr = 0x2000;
        transDevIbverbsData.remoteDataNotifyDetails.addr = 0x3000;
        transDevIbverbsData.remoteDataAckNotifyDetails.addr = 0x4000;
        transDevIbverbsData.remoteAckNotifyDetails.key = 20;
        transDevIbverbsData.remoteDataNotifyDetails.key = 20;
        transDevIbverbsData.remoteDataAckNotifyDetails.key = 20;

        transDevIbverbsData.userRemoteNotifyDetails.resize(1);
        transDevIbverbsData.userRemoteNotifyDetails[0].resize(notifyNum);

        transDevIbverbsData.userRemoteNotifyDetails[0][0].addr = 0x1010;
        transDevIbverbsData.userRemoteNotifyDetails[0][1].addr = 0x1020;
        transDevIbverbsData.userRemoteNotifyDetails[0][2].addr = 0x1030;
        transDevIbverbsData.userRemoteNotifyDetails[0][3].addr = 0x1040;
        transDevIbverbsData.userRemoteNotifyDetails[0][4].addr = 0x1050;
        transDevIbverbsData.userRemoteNotifyDetails[0][5].addr = 0x1060;
        transDevIbverbsData.userRemoteNotifyDetails[0][6].addr = 0x1070;
        transDevIbverbsData.userRemoteNotifyDetails[0][7].addr = 0x1080;

        for (u32 i=0; i<notifyNum; i++) {
            transDevIbverbsData.userRemoteNotifyDetails[0][i].key = 20;
        }

        transDevIbverbsData.notifyValueKey = 10;
        transDevIbverbsData.qpsPerConnection = 1;
        transDevIbverbsData.qpInfo.resize(1);
        transDevIbverbsData.qpInfo[0].qpPtr = 0x5000;
        transDevIbverbsData.qpInfo[0].sqIndex = 1;
        transDevIbverbsData.qpInfo[0].dbIndex = 2;
        transDevIbverbsData.remoteInputKey = 3;
        transDevIbverbsData.remoteOutputKey = 4;
        transDevIbverbsData.notifySize = 5;
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "MPI_TRANSPORT_DEVICE_IBVERBS_TEST SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "MPI_TRANSPORT_DEVICE_IBVERBS_TEST TearDown" << std::endl;
    }

    TransportDeviceIbverbsData transDevIbverbsData;
    static std::shared_ptr<LocalIpcNotify> localNotify;
    static std::shared_ptr<RemoteNotify> remoteNotify;
    static std::vector<std::shared_ptr<LocalIpcNotify>> userLocalNotify;
    static std::vector<std::shared_ptr<LocalIpcNotify>> userRemoteNotify;
};
std::shared_ptr<LocalIpcNotify> MPI_TRANSPORT_DEVICE_IBVERBS_TEST::localNotify = nullptr;
std::shared_ptr<RemoteNotify> MPI_TRANSPORT_DEVICE_IBVERBS_TEST::remoteNotify = nullptr;
std::vector<std::shared_ptr<LocalIpcNotify>> MPI_TRANSPORT_DEVICE_IBVERBS_TEST::userLocalNotify;
std::vector<std::shared_ptr<LocalIpcNotify>> MPI_TRANSPORT_DEVICE_IBVERBS_TEST::userRemoteNotify;

extern std::unique_ptr<NotifyPool> get_notifyPool(s32 rank);
const char *ibv_exp_device_server_ip  = "192.168.1.62";
const char *ibv_exp_device_client_ip  = "192.168.1.63";


void* InitRaNicResourcedevice(u32 deviceId, const char *deviceIp, void *rdmaHandle)
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

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_transport_device_link_test)
{
    s32 ret;
    s32 size, rank;
    u32 notifyNum = 8;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建dispatcher*/
    SetFftsSwitch(false);
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub *dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    DispatcherAiCpu *dispatcherAiCpu = reinterpret_cast<DispatcherAiCpu*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建link*/
    std::string port_name = "mlx5_0";

    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_device_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910_93;//1910
    machine_para.tag = "my tag";
    machine_para.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    machine_para.notifyNum = notifyNum;

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

    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    void *nicRdmaHandle;
    if (0 == rank)
    {
        nicRdmaHandle = InitRaNicResourcedevice(0, ibv_exp_device_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResourcedevice(0, ibv_exp_device_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_device_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_device_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_device_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_device_server_ip);
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

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

	ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    std::shared_ptr<TransportDeviceIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(0);

    link_exp.reset(new TransportDeviceIbverbs(dispatcher, notifyPool, machine_para, timeout, transDevIbverbsData));
    link_exp->Init();
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
        EXPECT_EQ(ret, HCCL_E_INTERNAL);

        ret = link_exp->RxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = link_exp->Post(10, stream);
    	EXPECT_EQ(ret, HCCL_E_INTERNAL);

        ret = link_exp->Wait(10, stream);
    	EXPECT_EQ(ret, HCCL_E_INTERNAL);

        std::vector<HcclSignalInfo> localNotify;
        ret =  link_exp->GetLocalNotify(localNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        std::vector<AddrKey> remoteNotify;
        ret =  link_exp->GetRemoteRdmaNotifyAddrKey(remoteNotify);
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
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_transport_device_link_all_test)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建dispatcher*/
    SetFftsSwitch(false);
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建link*/
    std::string port_name = "mlx5_0";

    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.serverId = ibv_exp_device_server_ip;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910_93;//1910
    machine_para.tag = "my tag";
    machine_para.notifyNum = 8;
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
        nicRdmaHandle = InitRaNicResourcedevice(0, ibv_exp_device_client_ip, nicRdmaHandle);
    }
    else if (1 == rank)
    {
        nicRdmaHandle = InitRaNicResourcedevice(0, ibv_exp_device_server_ip, nicRdmaHandle);
    }

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_device_server_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_device_client_ip);
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localIpAddr = HcclIpAddress(ibv_exp_device_client_ip);
        machine_para.remoteIpAddr = HcclIpAddress(ibv_exp_device_server_ip);
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

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

	ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);
    std::shared_ptr<TransportDeviceIbverbs> link_exp = nullptr;
    link_exp.reset(new TransportDeviceIbverbs(dispatcher, notifyPool, machine_para, timeout, transDevIbverbsData));
    link_exp->Init();
    MPI_Barrier(MPI_COMM_WORLD);
    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem = HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello client from server";
    s32 msg_len = SalStrLen(msg) + 1;
    DeviceMem tx_buf = inputMem.range(1, msg_len);
    ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
    std::vector<TxMemoryInfo> txMems;
    txMems.emplace_back(TxMemoryInfo{UserMemType::INPUT_MEM, 0, tx_buf.ptr(), msg_len});

    ret = link_exp->TxAsync(txMems, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret = link_exp->RxAck(stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = link_exp->Wait(10, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    u32 dst = 4;
    u32 src = 2;
    void *dstMemPtr = &dst;
    const void *srcMemPtr = &src;
    u64 srcMemSize = 10;
    u32 srcKey = 10;
    u32 dstKey = 20;
    u64 dstMemSize = 0;
    WqeType wqeType = WqeType::WQE_TYPE_DATA_WITH_NOTIFY;
    wr_aux_info aux;
    aux.data_type = 1;
    aux.notify_offset = 0;
    aux.reduce_type = 1;
    std::vector<WrInfo> wrInfoVec;

    ret = link_exp->AddWrList(dstMemPtr, srcMemPtr, srcMemSize, srcKey, dstKey, wqeType, aux, wrInfoVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wqeType = WqeType::WQE_TYPE_DATA_WITH_REDUCE;
    ret = link_exp->AddWrList(dstMemPtr, srcMemPtr, srcMemSize, srcKey, dstKey, wqeType, aux, wrInfoVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wqeType = WqeType::WQE_TYPE_RESEERVED;
    ret = link_exp->AddWrList(dstMemPtr, srcMemPtr, srcMemSize, srcKey, dstKey, wqeType, aux, wrInfoVec);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    UserMemType memType =UserMemType::INPUT_MEM;
    ret = link_exp->GetMemInfo(memType, &dstMemPtr, &dstKey, dstMemSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    memType =UserMemType::MEM_RESERVED;
    ret = link_exp->GetMemInfo(memType, &dstMemPtr, &dstKey, dstMemSize);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    memType =UserMemType::OUTPUT_MEM;
    ret = link_exp->GetMemInfo(memType, &dstMemPtr, &dstKey, dstMemSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 dstOffset = 200;
    ret = link_exp->TxPayLoad(memType, dstOffset, srcMemPtr, srcMemSize, wqeType, aux, wrInfoVec);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    wqeType = WqeType::WQE_TYPE_READ_DATA;
    ret = link_exp->AddWrList(dstMemPtr, srcMemPtr, srcMemSize, srcKey, dstKey, wqeType, aux, wrInfoVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    memType =UserMemType::INPUT_MEM;
    ret = link_exp->TxAsync(memType, dstOffset, srcMemPtr, srcMemSize, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    memType =UserMemType::INPUT_MEM;
    ret = link_exp->RxAsync(memType, dstOffset, dstMemPtr, srcMemSize, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = link_exp->RxData(memType, dstOffset, dstMemPtr, srcMemSize, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = link_exp->TxData(memType, dstOffset, srcMemPtr, srcMemSize, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    void *remoteAddr = nullptr;
    link_exp->GetRemoteMem(hccl::UserMemType::INPUT_MEM, &remoteAddr);
    struct Transport::Buffer remoteBuf(remoteAddr, srcMemSize);
    struct Transport::Buffer localBuf(inputMem.ptr(), srcMemSize);
    ret = link_exp->WriteAsync(remoteBuf, localBuf, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_INT8;
    HcclReduceOp redOp = HcclReduceOp::HCCL_REDUCE_SUM;
    ret = link_exp->TxWithReduce(memType, dstOffset, srcMemPtr, srcMemSize, datatype, redOp, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret = link_exp->WriteReduceAsync(remoteBuf, localBuf, datatype, redOp, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    std::vector<WrInfo> wrInfoVecTmp;
    std::vector<struct send_wr_rsp> opRspVec;
    ret = link_exp->TxWrList(wrInfoVecTmp, stream, opRspVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 qpInde = 10;
    u32 wrNum = 100;

    wqeType = WqeType::WQE_TYPE_DATA_WITH_REDUCE;
    u64 notifyAddr = 0x1000;
    u32 notifyId = 0;
    ret = link_exp->GetWrDataAddr(dstMemPtr, wqeType, notifyAddr, notifyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wqeType = WqeType::WQE_TYPE_DATA_NOTIFY;
    ret = link_exp->GetWrDataAddr(dstMemPtr, wqeType, notifyAddr, notifyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wqeType = WqeType::WQE_TYPE_ACK_NOTIFY;
    ret = link_exp->GetWrDataAddr(dstMemPtr, wqeType, notifyAddr, notifyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wqeType = WqeType::WQE_TYPE_DATA_ACK_NOTIFY;
    ret = link_exp->GetWrDataAddr(dstMemPtr, wqeType, notifyAddr, notifyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = link_exp->TxWaitDone(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = dispatcher->MemcpyAsync(host_mem, tx_buf, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aclrtSynchronizeStream(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

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
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_transport_ibverbs_stream_test)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);

    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    MachinePara machinePara;
    machinePara.isAicpuModeEn = true;
 
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::shared_ptr<TransportDeviceIbverbs> linktmp = nullptr;
    linktmp.reset(new TransportDeviceIbverbs(dispatcher, nullptr, machinePara, timeout, transDevIbverbsData));
    linktmp->Init();
    linktmp->RxAck(stream);
    linktmp->TxPrepare(stream);
    linktmp->TxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->RxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->RxDone(stream);
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
}

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_transport_ibverbs_all_test)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    u64 size = 245;
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);

    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    std::chrono::milliseconds timeout;
    const std::string tag;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    MOCKER_CPP_VIRTUAL(transportIbverbs, &TransportIbverbs::RegUserMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transportIbverbs, &TransportIbverbs::TxSendWqe)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transportIbverbs,&TransportIbverbs::RdmaSendAsync, HcclResult(TransportIbverbs::*)(std::vector<WqeInfo>&, Stream&, bool, u32))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportIbverbs::RdmaSendAsyncHostNIC, HcclResult(TransportIbverbs::*)(std::vector<WqeInfo>&, Stream&))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<TransportIbverbs> linktmp = nullptr;
    linktmp.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));
   
    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
   
    linktmp->Init();
    linktmp->TxPrepare(stream);
    linktmp->RxPrepare(stream);
    linktmp->TxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->RxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->GetRemoteMemSize(UserMemType::INPUT_MEM, size);
    linktmp->GetRemoteMemSize(UserMemType::OUTPUT_MEM, size);
    std::vector<HcclSignalInfo> rdmaNotify;
    linktmp->GetLocalRdmaNotify(rdmaNotify);
    std::vector<AddrKey> addkeyNotify;
    linktmp->GetRemoteRdmaNotifyAddrKey(addkeyNotify);
    linktmp->GetLocalNotifyValueAddrKey(addkeyNotify);
    uint32_t remoteMemKey = 10;
    linktmp->GetRemoteMemKey(UserMemType::INPUT_MEM, &remoteMemKey);
    linktmp->GetRemoteMemKey(UserMemType::MEM_RESERVED, &remoteMemKey);
    MemDetails memDetails;
    linktmp->GetLocalMemDetails(UserMemType::INPUT_MEM, memDetails);
    linktmp->GetLocalMemDetails(UserMemType::MEM_RESERVED, memDetails);
    struct ai_qp_info qpInfo;
    qpInfo.ai_qp_addr = 0x12345678;
    qpInfo.sq_index = 0;
    qpInfo.db_index = 1;
    std::vector<HcclQpInfoV2> v2(1);
    v2[0].dbIndex = 1;
    v2[0].qpPtr = 0x100;
    v2[0].sqIndex = 2;
    linktmp->combineAiQpInfos_.resize(1);
    linktmp->combineAiQpInfos_[0].aiQpInfo = qpInfo;
    linktmp->TxDone(stream);
    linktmp->RxDone(stream);
    linktmp->PostFinAck(stream);
    linktmp->WaitFinAck(stream);


    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
}

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_transport_ibverbs_aicpu_test)
{
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    u64 size = 245;
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);

    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    MachinePara machinePara;
    machinePara.localDeviceId = 0;
    
    machinePara.isAicpuModeEn = true;
    std::chrono::milliseconds timeout;
    const std::string tag;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    MOCKER_CPP_VIRTUAL(transportIbverbs, &TransportIbverbs::RegUserMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transportIbverbs, &TransportIbverbs::TxSendWqe)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transportIbverbs,&TransportIbverbs::RdmaSendAsync, HcclResult(TransportIbverbs::*)(std::vector<WqeInfo>&, Stream&, bool, u32))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportIbverbs::RdmaSendAsyncHostNIC, HcclResult(TransportIbverbs::*)(std::vector<WqeInfo>&, Stream&))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<TransportIbverbs> linktmp = nullptr;
    linktmp.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));
   
    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
   
    linktmp->Init();
    linktmp->TxPrepare(stream);
    linktmp->RxPrepare(stream);
    linktmp->TxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->RxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->GetRemoteMemSize(UserMemType::INPUT_MEM, size);
    linktmp->GetRemoteMemSize(UserMemType::OUTPUT_MEM, size);
    std::vector<HcclSignalInfo> rdmaNotify;
    linktmp->GetLocalRdmaNotify(rdmaNotify);
    std::vector<AddrKey> addkeyNotify;
    linktmp->GetRemoteRdmaNotifyAddrKey(addkeyNotify);
    linktmp->GetLocalNotifyValueAddrKey(addkeyNotify);
    uint32_t remoteMemKey = 10;
    linktmp->GetRemoteMemKey(UserMemType::INPUT_MEM, &remoteMemKey);
    linktmp->GetRemoteMemKey(UserMemType::MEM_RESERVED, &remoteMemKey);
    MemDetails memDetails;
    linktmp->GetLocalMemDetails(UserMemType::INPUT_MEM, memDetails);
    linktmp->GetLocalMemDetails(UserMemType::MEM_RESERVED, memDetails);
    std::vector<HcclQpInfoV2> v2(1);
    v2[0].dbIndex = 1;
    v2[0].qpPtr = 0x100;
    v2[0].sqIndex = 2;
    linktmp->GetAiQpInfo(v2);
    linktmp->TxDone(stream);
    linktmp->RxDone(stream);
    linktmp->PostFinAck(stream);
    linktmp->WaitFinAck(stream);
    linktmp->CreateMultiQp(1, 2);
    linktmp->ConnectSingleQp();
    linktmp->ConnectMultiQp(1);
    linktmp->ConstructExchangeForSend();
    linktmp->ParseReceivedExchangeData();

    QpHandle qpHandle;
    ai_qp_info qpInfo;
    linktmp->CreateOneQp(2, 2, qpHandle, qpInfo, true);
    MemType memType =MemType::DATA_NOTIFY_MEM;
    u8* exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 2;
    linktmp->RegUserMem(memType, exchangeDataPtr, exchangeDataBlankSize);
    linktmp->CreateNotifyValueBuffer();
    linktmp->DestroyQP();
    MemMsg memMsg;
    linktmp->DeRegMRForQPhandles(memMsg);
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
}

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_transport_base_all_test)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);

    void *dispatcher = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcher, nullptr);
    MachinePara machinePara;
    machinePara.localDeviceId = 0;

    std::chrono::milliseconds timeout;

    std::shared_ptr<Transport> link_(new Transport(new (std::nothrow) TransportBase(
        nullptr, nullptr, machinePara, timeout)));
    std::vector<HcclSignalInfo> rdmaNotify;
    link_->GetLocalRdmaNotify(rdmaNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::vector<AddrKey> rdmaNotifyAddr;
    link_->GetRemoteRdmaNotifyAddrKey(rdmaNotifyAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    link_->GetLocalNotifyValueAddrKey(rdmaNotifyAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::vector<HcclQpInfoV2> aiQpInfo;
    link_->GetAiQpInfo(aiQpInfo);
    s64 chipId = 1;
    link_->GetChipId(chipId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MemDetails memDetails;
    link_->GetLocalMemDetails(UserMemType::INPUT_MEM, memDetails);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclDispatcherDestroy(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_TransportIbverbs_GetRemoteMem)
{
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    u64 size = 245;
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);

    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    MachinePara machinePara;
    machinePara.isAicpuModeEn = true;
    std::chrono::milliseconds timeout;
    const std::string tag;

    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    MOCKER_CPP_VIRTUAL(transportIbverbs, &TransportIbverbs::RegUserMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transportIbverbs, &TransportIbverbs::TxSendWqe)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transportIbverbs,&TransportIbverbs::RdmaSendAsync, HcclResult(TransportIbverbs::*)(std::vector<WqeInfo>&, Stream&, bool, u32))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportIbverbs::RdmaSendAsyncHostNIC, HcclResult(TransportIbverbs::*)(std::vector<WqeInfo>&, Stream&))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<TransportIbverbs> linktmp = nullptr;
    linktmp.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));
   
    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
   
    linktmp->Init();

    u32 memory = 2;
    linktmp->remoteMemMsg_[static_cast<u32>(UserMemType::INPUT_MEM)].addr = &memory;
    linktmp->remoteMemMsg_[static_cast<u32>(UserMemType::OUTPUT_MEM)].addr = &memory;
    linktmp->remoteMemMsg_[static_cast<u32>(UserMemType::MEM_RESERVED)].addr = nullptr;

    void *remotePtr = &memory;
    ret = linktmp->GetRemoteMem(UserMemType::INPUT_MEM, &remotePtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = linktmp->GetRemoteMem(UserMemType::MEM_RESERVED, &remotePtr);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    linktmp->remoteMemMsg_[static_cast<u32>(MemType::ACK_NOTIFY_MEM)].addr = &memory;
    linktmp->remoteMemMsg_[static_cast<u32>(MemType::DATA_NOTIFY_MEM)].addr = &memory;
    linktmp->remoteMemMsg_[static_cast<u32>(MemType::DATA_ACK_NOTIFY_MEM)].addr = &memory;
    std::vector<AddrKey> addrKey;
    ret = linktmp->GetRemoteRdmaNotifyAddrKey(addrKey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::vector<HcclSignalInfo> rdmaNotify;
    linktmp->memMsg_[static_cast<u32>(MemType::ACK_NOTIFY_MEM)].addr = &memory;
    linktmp->memMsg_[static_cast<u32>(MemType::DATA_NOTIFY_MEM)].addr = &memory;
    linktmp->memMsg_[static_cast<u32>(MemType::DATA_ACK_NOTIFY_MEM)].addr = &memory;
    linktmp->ackNotify_ = localNotify;
    linktmp->dataNotify_ = localNotify;
    linktmp->dataAckNotify_ = localNotify;

    ret = linktmp->GetLocalRdmaNotify(rdmaNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
}

void InitTransportDeviceIbverbsData(TransportDeviceIbverbsData& transDevIbverbsData)
{
    HcclSignalInfo notifyInfo;
    notifyInfo.addr = 100;
    notifyInfo.devId = 1;
    notifyInfo.rankId = 2;
    notifyInfo.resId = 3;
    notifyInfo.tsId = 4;
    transDevIbverbsData.inputBufferPtr = nullptr;
    transDevIbverbsData.outputBufferPtr = nullptr;
    transDevIbverbsData.localInputMem.size = 10;
    transDevIbverbsData.localInputMem.addr = 0x10;
    transDevIbverbsData.localInputMem.key = 10;
    transDevIbverbsData.localOutputMem.size = 10;
    transDevIbverbsData.localOutputMem.addr = 0x1a;
    transDevIbverbsData.localOutputMem.key = 10;
    transDevIbverbsData.ackNotify = std::make_shared<LocalIpcNotify>();
    transDevIbverbsData.ackNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    transDevIbverbsData.dataAckNotify = std::make_shared<LocalIpcNotify>();
    transDevIbverbsData.dataAckNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    transDevIbverbsData.dataNotify = std::make_shared<LocalIpcNotify>();
    transDevIbverbsData.dataNotify->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
 
    u32 notifyNum = 8;
    transDevIbverbsData.userLocalNotify.resize(1);
    transDevIbverbsData.userLocalNotify[0].resize(notifyNum);
    for (u32 i=0; i<notifyNum; i++) {
        transDevIbverbsData.userLocalNotify[0][i] = std::make_shared<LocalIpcNotify>();
        transDevIbverbsData.userLocalNotify[0][i]->Init(notifyInfo, NotifyLoadType::DEVICE_NOTIFY);
    }
 
    transDevIbverbsData.localNotifyValueAddr = 0x1000;
 
    transDevIbverbsData.remoteAckNotifyDetails.addr = 0x2000;
    transDevIbverbsData.remoteDataNotifyDetails.addr = 0x3000;
    transDevIbverbsData.remoteDataAckNotifyDetails.addr = 0x4000;
    transDevIbverbsData.remoteAckNotifyDetails.key = 20;
    transDevIbverbsData.remoteDataNotifyDetails.key = 20;
    transDevIbverbsData.remoteDataAckNotifyDetails.key = 20;
 
    transDevIbverbsData.userRemoteNotifyDetails.resize(1);
    transDevIbverbsData.userRemoteNotifyDetails[0].resize(notifyNum);
 
    transDevIbverbsData.userRemoteNotifyDetails[0][0].addr = 0x1010;
    transDevIbverbsData.userRemoteNotifyDetails[0][1].addr = 0x1020;
    transDevIbverbsData.userRemoteNotifyDetails[0][2].addr = 0x1030;
    transDevIbverbsData.userRemoteNotifyDetails[0][3].addr = 0x1040;
    transDevIbverbsData.userRemoteNotifyDetails[0][4].addr = 0x1050;
    transDevIbverbsData.userRemoteNotifyDetails[0][5].addr = 0x1060;
    transDevIbverbsData.userRemoteNotifyDetails[0][6].addr = 0x1070;
    transDevIbverbsData.userRemoteNotifyDetails[0][7].addr = 0x1080;
    for (u32 i=0; i<notifyNum; i++) {
        transDevIbverbsData.userRemoteNotifyDetails[0][i].key = 20;
    }
 
    transDevIbverbsData.notifyValueKey = 10;
    transDevIbverbsData.qpsPerConnection = 1;
    transDevIbverbsData.qpInfo.resize(1);
    transDevIbverbsData.qpInfo[0].qpPtr = 0x5000;
    transDevIbverbsData.qpInfo[0].sqIndex = 1;
    transDevIbverbsData.qpInfo[0].dbIndex = 2;
    transDevIbverbsData.remoteInputKey = 3;
    transDevIbverbsData.remoteOutputKey = 4;
    transDevIbverbsData.notifySize = 5;
}
 
TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, st_transport_ibverbs_TxWithReduce)
{
    MOCKER(stub_ibv_exp_post_send).stubs().with(any()).will(returnValue(0));
 
    s32 ret;
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
 
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::RdmaSend, HcclResult(DispatcherPub::*)(u32, u64, hccl::Stream &,
        RdmaTaskInfo &)).stubs().will(returnValue(HCCL_SUCCESS));
    
    std::chrono::milliseconds timeout;
    std::shared_ptr<TransportDeviceIbverbs> linktmp = nullptr;
 
    TransportDeviceIbverbsData transDevIbverbsData;
    InitTransportDeviceIbverbsData(transDevIbverbsData);
    MachinePara machinePara;
    machinePara.isAicpuModeEn = true;
    machinePara.notifyNum = transDevIbverbsData.userLocalNotify[0].size();
 
    linktmp.reset(new TransportDeviceIbverbs(dispatcher, nullptr, machinePara, timeout, transDevIbverbsData));
    linktmp->Init();
    linktmp->useAtomicWrite_ = true;
    linktmp->TxWithReduce(UserMemType::INPUT_MEM, 0, reinterpret_cast<void*>(transDevIbverbsData.localInputMem.addr),
        transDevIbverbsData.localInputMem.size, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, stream);
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
}