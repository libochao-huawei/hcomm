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
#define protected public
#include "dispatcher_pub.h"
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

#include "transport_p2p_pub.h"
#include "rank_consistentcy_checker.h"
#include "dlra_function.h"
#include "network_manager_pub.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"
#include "remote_notify.h"
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

class MPI_Pcie_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_Pcie_Test SetUP" << std::endl;
        localNotify.reset(new (std::nothrow) LocalIpcNotify());
        HcclResult ret = localNotify->Init(0, 0, NotifyLoadType::HOST_NOTIFY);
        EXPECT_EQ(ret, HCCL_SUCCESS);

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
        std::cout << "MPI_Pcie_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        TsdOpen(1, 2);
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "MPI_Pcie_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "MPI_Pcie_Test TearDown" << std::endl;
    }
    static std::shared_ptr<LocalIpcNotify> localNotify;
    static std::shared_ptr<RemoteNotify> remoteNotify;
};
std::shared_ptr<LocalIpcNotify> MPI_Pcie_Test::localNotify = nullptr;
std::shared_ptr<RemoteNotify> MPI_Pcie_Test::remoteNotify = nullptr;

std::unique_ptr<NotifyPool> get_notifyPool(s32 rank)
{
    HcclResult ret = HCCL_SUCCESS;

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return std::move(notifyPool);
}

TEST_F(MPI_Pcie_Test, st_mpi_pcie_init)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlTdtFunction::GetInstance().DlTdtFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    std::string inter_collective_id = commId.internal;

    std::string port_name = "mlx5_0";
    u32 ipAddr[size];
    if (rank == 0)
    {
        for (int i = 0; i < size; i++ )
        {
            (void)rt_get_dev_ip(0, i, &ipAddr[i]);
        }
    }
    MPI_Bcast(ipAddr, sizeof(ipAddr), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);
    const s32 ndev = 2;
    s32 dev_list[ndev] = {1, 0};
    std::vector<s32> device_ids(dev_list, dev_list+ndev);
    const u32 rank_list[ndev] = {0, 1};
    std::vector<u32> user_ranks(rank_list, rank_list+ndev);

    // set device
    s32 device_id = dev_list[rank];
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)device_id, devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, devicePhyId, devicePhyId, HcclIpAddress(devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(inter_collective_id, portCtx, device_id, devicePhyId, rank_list[rank], ndev,
        device_ids, user_ranks, true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    sal_memset(inputMem.ptr(), sizeof(mem_size), 0, sizeof(mem_size));
    sal_memset(outputMem.ptr(), sizeof(mem_size), 0, sizeof(mem_size));

    MachinePara machine_para;
    machine_para. localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.linkAttribute = 0x1;
    machine_para.deviceLogicId = device_id;
    machine_para.tag = "default";

    if (0 == rank)
    {
        machine_para.localUserrank = 0;
        machine_para.remoteUserrank = 1;
    }
    else if (1 == rank)
    {
        machine_para.localUserrank = 1;
        machine_para.remoteUserrank = 0;
    }

    auto item = exchanger.socketsMap.find(rank_list[machine_para.remoteUserrank]);
    auto socketLists = item->second;
    for (u32 i = 0; i < socketLists.size(); i++) {
        machine_para.sockets.push_back(socketLists[i]);
    }

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    const std::string tag = "default";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    char strTag[100] = "default";
    char strGroup[100] = "my group";

    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";

    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);
    HCCL_INFO(" RankConsistentcyChecker SUCCESS[INFO]");
    /*创建link*/
    machine_para.collectiveId = inter_collective_id;
    std::shared_ptr<TransportP2p> link = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

    link.reset(new TransportP2p(dispatcher, notifyPool, machine_para, timeout));
    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步

    /*让dispatcher在link之后析构*/
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool = nullptr;
    link = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId);
}

TEST_F(MPI_Pcie_Test, st_mpi_pcie_tx_rx)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlTdtFunction::GetInstance().DlTdtFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 ipAddr[size];
    if (rank == 0)
    {
        for (int i = 0; i < size; i++ )
        {
            (void)rt_get_dev_ip(0, i, &ipAddr[i]);
        }
    }
    MPI_Bcast(ipAddr, sizeof(ipAddr), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    std::string inter_collective_id = commId.internal;
    std::string port_name = "mlx5_0";
    const s32 ndev = 2;
    s32 dev_list[ndev] = {1, 0};
    std::vector<s32> device_ids(dev_list, dev_list+ndev);
    const u32 rank_list[ndev] = {0, 1};
    std::vector<u32> user_ranks(rank_list, rank_list+ndev);

    // set device
    s32 device_id = dev_list[rank];
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)device_id, devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, devicePhyId, devicePhyId, HcclIpAddress(devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(inter_collective_id, portCtx, device_id, devicePhyId, rank_list[rank], ndev,
        device_ids, user_ranks, true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para. localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.linkAttribute = 0x1;
    machine_para.tag = "default";
    machine_para.deviceLogicId = device_id;

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localUserrank = 0;
        machine_para.remoteUserrank = 1;
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localUserrank = 1;
        machine_para.remoteUserrank = 0;
    }

    auto item = exchanger.socketsMap.find(rank_list[machine_para.remoteUserrank]);
    auto socketLists = item->second;
    for (u32 i = 0; i < socketLists.size(); i++) {
        machine_para.sockets.push_back(socketLists[i]);
    }

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    const std::string tag = "default";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    /*创建link*/
    machine_para.collectiveId = inter_collective_id;
    std::shared_ptr<TransportP2p> link = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

    link.reset(new TransportP2p(dispatcher, notifyPool, machine_para, timeout));
    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem =  HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello";
    s32 msg_len = SalStrLen(msg) + 1;

    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        link->RxAck(stream);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = outputMem;

        link->TxAck(stream);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool = nullptr;
    /*让dispatcher在link之后析构*/
    link = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId);

}

TEST_F(MPI_Pcie_Test, st_mpi_pcie_dst_copy)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 ipAddr[size];
    if (rank == 0)
    {
        for (int i = 0; i < size; i++ )
        {
            (void)rt_get_dev_ip(0, i, &ipAddr[i]);
        }
    }
    MPI_Bcast(ipAddr, sizeof(ipAddr), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    std::string inter_collective_id = commId.internal;
    std::string port_name = "mlx5_0";
    const s32 ndev = 2;
    s32 dev_list[ndev] = {1, 0};
    std::vector<s32> device_ids(dev_list, dev_list+ndev);
    const u32 rank_list[ndev] = {0, 1};
    std::vector<u32> user_ranks(rank_list, rank_list+ndev);

    // set device
    s32 device_id = dev_list[rank];
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)device_id, devicePhyId);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, devicePhyId, devicePhyId, HcclIpAddress(devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(inter_collective_id, portCtx, device_id, devicePhyId, rank_list[rank], ndev,
        device_ids, user_ranks, true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//V80
    machine_para.linkAttribute = 0x2;//目的端发起拷贝
    machine_para.tag = "default";
    machine_para.supportDataReceivedAck = true;

    machine_para.deviceLogicId = device_id;
    
    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localUserrank = 0;
        machine_para.remoteUserrank = 1;
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localUserrank = 1;
        machine_para.remoteUserrank = 0;
    }

    auto item = exchanger.socketsMap.find(rank_list[machine_para.remoteUserrank]);
    auto socketLists = item->second;
    for (u32 i = 0; i < socketLists.size(); i++) {
        machine_para.sockets.push_back(socketLists[i]);
    }

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    const std::string tag = "default";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    /*创建link*/
    machine_para.collectiveId = inter_collective_id;
    std::shared_ptr<TransportP2p> link = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

    link.reset(new TransportP2p(dispatcher, notifyPool, machine_para, timeout));
    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem =  HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello";
    s32 msg_len = SalStrLen(msg) + 1;

    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem &tx_buf = inputMem;

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        link->RxAck(stream);
        link->TxAsync(UserMemType::INPUT_MEM, 0, nullptr, 0, stream);
        link->RxAsync(UserMemType::INPUT_MEM, 0, nullptr, 0, stream);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;

        link->TxAck(stream);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        link->TxAsync(UserMemType::INPUT_MEM, 0, nullptr, 0, stream);
        link->RxAsync(UserMemType::INPUT_MEM, 0, nullptr, 0, stream);
        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool = nullptr;
    /*让dispatcher在link之后析构*/
    link = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId);
}

TEST_F(MPI_Pcie_Test, st_mpi_pcie_write_read)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 ipAddr[size];
    if (rank == 0)
    {
        for (int i = 0; i < size; i++ )
        {
            (void)rt_get_dev_ip(0, i, &ipAddr[i]);
        }
    }
    MPI_Bcast(ipAddr, sizeof(ipAddr), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    std::string inter_collective_id = commId.internal;
    std::string port_name = "mlx5_0";
    const s32 ndev = 2;
    s32 dev_list[ndev] = {1, 0};
    std::vector<s32> device_ids(dev_list, dev_list+ndev);
    const u32 rank_list[ndev] = {0, 1};
    std::vector<u32> user_ranks(rank_list, rank_list+ndev);

    // set device
    s32 device_id = dev_list[rank];
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)device_id, devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, devicePhyId, devicePhyId, HcclIpAddress(devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(inter_collective_id, portCtx, device_id, devicePhyId, rank_list[rank], ndev,
        device_ids, user_ranks, true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    u32 notifyNum = 8;
    machine_para.localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;//V80
    machine_para.tag = "default";
    machine_para.notifyNum = notifyNum;
    machine_para.supportDataReceivedAck = true;

    machine_para.deviceLogicId = device_id;
    
    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localUserrank = 0;
        machine_para.remoteUserrank = 1;
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localUserrank = 1;
        machine_para.remoteUserrank = 0;
    }

    auto item = exchanger.socketsMap.find(rank_list[machine_para.remoteUserrank]);
    auto socketLists = item->second;
    for (u32 i = 0; i < socketLists.size(); i++) {
        machine_para.sockets.push_back(socketLists[i]);
    }

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    const std::string tag = "default";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    /*创建link*/
    machine_para.collectiveId = inter_collective_id;
    std::shared_ptr<TransportP2p> link = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

    link.reset(new TransportP2p(dispatcher, notifyPool, machine_para, timeout));
    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem =  HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello";
    s32 msg_len = SalStrLen(msg) + 1;

    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem &tx_buf = inputMem;

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        link->WaitReady(stream);
        void *remoteAddr = nullptr;
        ret = link->GetRemoteMem(hccl::UserMemType::INPUT_MEM, &remoteAddr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        struct Transport::Buffer remoteBuf(remoteAddr, msg_len);
        struct Transport::Buffer localBuf(tx_buf.ptr(), msg_len);
        ret = link->WriteSync(remoteBuf, localBuf, stream);
        link->PostFin(stream);
        link->Post(1, stream);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;

        link->PostReady(stream);

        link->WaitFin(stream);
        link->Wait(1, stream);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem &tx_buf = inputMem;

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        link->PostReady(stream);
        link->WaitFin(stream);
        link->Wait(1, stream);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = inputMem;

        link->WaitReady(stream);
        void *remoteAddr = nullptr;
        link->GetRemoteMem(hccl::UserMemType::INPUT_MEM, &remoteAddr);
        struct Transport::Buffer remoteBuf(remoteAddr, 0);
        struct Transport::Buffer localBuf(rx_buf.ptr(), 0);    
        link->ReadSync(localBuf, remoteBuf, stream);

        link->PostFin(stream);
        link->Post(1, stream);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);
        link->ReadReduceSync(localBuf, remoteBuf,
            HcclDataType::HCCL_DATA_TYPE_INT8,
            HcclReduceOp::HCCL_REDUCE_SUM, stream);


        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool = nullptr;
    /*让dispatcher在link之后析构*/
    link = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId);
}

TEST_F(MPI_Pcie_Test, st_mpi_pcie_create_link)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string inter_collective_id = commId.internal;

    u32 ipAddr[size];
    if (rank == 0)
    {
        for (int i = 0; i < size; i++ )
        {
            (void)rt_get_dev_ip(0, i, &ipAddr[i]);
        }
    }
    MPI_Bcast(ipAddr, sizeof(ipAddr), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    const s32 ndev = 2;
    s32 dev_list[ndev] = {1, 0};
    std::vector<s32> device_ids(dev_list, dev_list+ndev);
    const u32 rank_list[ndev] = {0, 1};
    std::vector<u32> user_ranks(rank_list, rank_list+ndev);

    // set device
    s32 device_id = dev_list[rank];
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)device_id, devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, devicePhyId, devicePhyId, HcclIpAddress(devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(inter_collective_id, portCtx, device_id, devicePhyId, rank_list[rank], ndev,
        device_ids, user_ranks, true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    sal_memset(inputMem.ptr(), sizeof(mem_size), 0, sizeof(mem_size));
    sal_memset(outputMem.ptr(), sizeof(mem_size), 0, sizeof(mem_size));

    MachinePara machine_para;
    machine_para. localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.linkAttribute = 0x1;
    machine_para.tag = "default";
    machine_para.deviceLogicId = device_id;

    if (0 == rank)
    {
        machine_para.localUserrank = 0;
        machine_para.remoteUserrank = 1;
    }
    else if (1 == rank)
    {
        machine_para.localUserrank = 1;
        machine_para.remoteUserrank = 0;
    }
    auto item = exchanger.socketsMap.find(rank_list[machine_para.remoteUserrank]);
    auto socketLists = item->second;
    for (u32 i = 0; i < socketLists.size(); i++) {
        machine_para.sockets.push_back(socketLists[i]);
    }

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    const std::string tag = "default";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    machine_para.collectiveId = inter_collective_id;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

    std::shared_ptr<Transport> link;

    /*创建link*/
    link.reset(new (std::nothrow) Transport(new (std::nothrow) TransportP2p(
        dispatcher, notifyPool, machine_para, timeout)));

    EXPECT_NE(link, nullptr);

    MPI_Barrier(MPI_COMM_WORLD);
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool = nullptr;

    link = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId);
}

TEST_F(MPI_Pcie_Test, st_mpi_mc2aicpu_pcie_tx_rx)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlTdtFunction::GetInstance().DlTdtFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 ipAddr[size];
    if (rank == 0) {
        for (int i = 0; i < size; i++ ) {
            (void)rt_get_dev_ip(0, i, &ipAddr[i]);
        }
    }
    MPI_Bcast(ipAddr, sizeof(ipAddr), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    std::string inter_collective_id = commId.internal;
    std::string port_name = "mlx5_0";
    const s32 ndev = 2;
    s32 dev_list[ndev] = {1, 0};
    std::vector<s32> device_ids(dev_list, dev_list+ndev);
    const u32 rank_list[ndev] = {0, 1};
    std::vector<u32> user_ranks(rank_list, rank_list+ndev);

    // set device
    s32 device_id = dev_list[rank];
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)device_id, devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, devicePhyId, devicePhyId, HcclIpAddress(devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(inter_collective_id, portCtx, device_id, devicePhyId, rank_list[rank], ndev,
        device_ids, user_ranks, true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    s32 host_mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(host_mem_size);
    DeviceMem outputMem = DeviceMem::alloc(host_mem_size);
    sal_memset(inputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));
    sal_memset(outputMem.ptr(), sizeof(host_mem_size), 0, sizeof(host_mem_size));

    MachinePara machine_para;
    machine_para. localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.linkAttribute = 0x1;
    machine_para.deviceLogicId = device_id;
    machine_para.tag = "default";
    machine_para.isAicpuModeEn = true;

    if (0 == rank) {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localUserrank = 0;
        machine_para.remoteUserrank = 1;
    } else if (1 == rank) {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localUserrank = 1;
        machine_para.remoteUserrank = 0;
    }

    auto item = exchanger.socketsMap.find(rank_list[machine_para.remoteUserrank]);
    auto socketLists = item->second;
    for (u32 i = 0; i < socketLists.size(); i++) {
        machine_para.sockets.push_back(socketLists[i]);
    }

    /*创建dispatcher*/
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    const std::string tag = "default";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    /*创建link*/
    machine_para.collectiveId = inter_collective_id;
    std::shared_ptr<TransportP2p> link = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

    link.reset(new TransportP2p(dispatcher, notifyPool, machine_para, timeout));
    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    HostMem host_mem =  HostMem::alloc(host_mem_size);
    sal_memset(host_mem.ptr(), host_mem_size, 0 , host_mem_size);

    const char* msg = "hello";
    s32 msg_len = SalStrLen(msg) + 1;

    if (0 == rank) {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    } else if (1 == rank) {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = outputMem;

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = aclrtSynchronizeStream(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool = nullptr;
    /*让dispatcher在link之后析构*/
    link = nullptr;
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId);

}

TEST_F(MPI_Pcie_Test, st_function_for_sendrecv_p2p)
{
    std::string collectiveId = "test_collective";
    ResetInitState();
    SetFftsSwitch(false);
    InitExternalInput();

    s32 device_id = 0;
    // 创建dispatcher
    void *dispatcherPtr = nullptr;
    HcclResult ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    std::unique_ptr<NotifyPool> notifyPool = get_notifyPool(0);
    EXPECT_NE(notifyPool, nullptr);

    MachinePara machine_para;
    machine_para.deviceLogicId = device_id;
    machine_para.supportDataReceivedAck = true;
    machine_para.linkAttribute = 0x1;

    MOCKER_CPP(&DispatcherPub::SignalRecord, HcclResult(DispatcherPub:: *)(HcclRtNotify, HcclRtStream, u32, u64, s32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DispatcherPub::SignalWait, HcclResult(DispatcherPub:: *)(HcclRtNotify, HcclRtStream, u32, u32, s32, u32, bool))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    DeviceMem input =  DeviceMem::alloc(1);
    DeviceMem output =  DeviceMem::alloc(1);

    std::shared_ptr<TransportP2p> linktmp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10);
    linktmp.reset(new TransportP2p(dispatcher, notifyPool, machine_para, timeout));

    linktmp->localSendReadyNotify_ = localNotify;
    linktmp->localSendDoneNotify_ = localNotify;
    linktmp->remoteSendReadyNotify_ = remoteNotify;
    linktmp->remoteSendDoneNotify_ = remoteNotify;

    Stream streamObj(StreamType::STREAM_TYPE_OFFLINE);
    ret = linktmp->TxPrepare(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = linktmp->RxPrepare(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = linktmp->TxData(UserMemType::OUTPUT_MEM, 0, input.ptr(), 0, streamObj);
    
    ret = linktmp->RxData(UserMemType::OUTPUT_MEM, 0, output.ptr(), 0, streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = linktmp->TxDone(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = linktmp->RxDone(streamObj);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclDispatcherDestroy(dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();

    GlobalMockObject::verify();
}

u32 NOTIFY_TIMEOUT = 1;
HcclResult LocalCopyWithReadReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank)
{
    std::string key = "LocalCopyWithReadReduce";
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
    uint64_t len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
    if (rank == 0) {
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommLocalCopy(thread, outputMem.ptr() , inputMem.ptr(), len));
        CHK_RET(CommReadReduce(thread, reinterpret_cast<uint64_t>(link.get()), const_cast<void*>(outputMem.ptr()), remoteInputPtr, count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ReadWithLocalReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank)
{
    std::string key = "ReadWithLocalReduce";
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
    uint64_t len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
    if (rank == 0) {
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 1));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommRead(thread, reinterpret_cast<uint64_t>(link.get()), const_cast<void*>(outputMem.ptr()), remoteInputPtr, len));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 1));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommLocalReduce(thread, static_cast<void *>(outputMem.ptr()), static_cast<void *>(inputMem.ptr()), count,
            HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult RemoteWriteWithLocalReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank)
{
    std::string key = "RemoteWriteWithLocalReduce";
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
    uint64_t len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
    if (rank == 0) {
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
        CHK_RET(CommWrite(thread, reinterpret_cast<uint64_t>(link.get()), remoteOutputPtr, inputMem.ptr(), len));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 1));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 2));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 2, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 1));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 2));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 2, NOTIFY_TIMEOUT));
        CHK_RET(CommLocalReduce(thread, static_cast<void *>(outputMem.ptr()), static_cast<void *>(inputMem.ptr()), count,
            HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult RemoteWriteWithReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank, bool isWithReduce)
{
    std::string key = "RemoteWriteWithReduce";
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
    uint64_t len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
    if (rank == 0) {
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
        if (!isWithReduce) {
            CHK_RET(CommWrite(thread, reinterpret_cast<uint64_t>(link.get()), remoteOutputPtr, inputMem.ptr(), len));
        } else {
            CHK_RET(CommWriteReduce(thread, reinterpret_cast<uint64_t>(link.get()), remoteOutputPtr, inputMem.ptr(), count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        }
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 1));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 2));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 2, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        if (isWithReduce) {
            CHK_RET(CommLocalCopy(thread, outputMem.ptr() , inputMem.ptr(), len));
        }
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 0));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 1));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, reinterpret_cast<uint64_t>(link.get()), 2));
        CHK_RET(CommNotifyWait(thread, reinterpret_cast<uint64_t>(link.get()), 2, NOTIFY_TIMEOUT));
        if (!isWithReduce) {
            CHK_RET(CommLocalReduce(thread, static_cast<void *>(outputMem.ptr()), static_cast<void *>(inputMem.ptr()), count,
                HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        }
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult PrimitiveLocalCopyWithReadReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank)
{
    std::string key = "PrimitiveLocalCopyWithReadReduce";
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
    uint64_t len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
    if (rank == 0) {
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        u64 len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
        HcclBuf dst{outputMem.ptr(), len, nullptr};
        HcclBuf src{inputMem.ptr(), len, nullptr};
        // CHK_RET(HcclLocalCopy(&stream, &dst, &src));
        CHK_RET(CommLocalCopy(thread, outputMem.ptr() , inputMem.ptr(), len));
        HcclBuf rmtBuf{remoteInputPtr, len, nullptr};
        HcclReduceInfo reduceInfo{HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM};
        CHK_RET(HcclRemoteReadReduce(&stream, link.get(), &dst, &rmtBuf, reduceInfo));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult PrimitiReadWithLocalReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank)
{
    std::string key = "PrimitiReadWithLocalReduce";
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
    if (rank == 0) {
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        u64 len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
        HcclBuf locBuf{outputMem.ptr(), len, nullptr};
        HcclBuf rmtBuf{remoteInputPtr, len, nullptr};
        CHK_RET(HcclRemoteRead(&stream, link.get(), &locBuf, &rmtBuf));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommLocalReduce(thread, static_cast<void *>(outputMem.ptr()), static_cast<void *>(inputMem.ptr()), count,
            HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult PrimitiRemoteWriteWithLocalReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank)
{
    std::string key = "PrimitiRemoteWriteWithLocalReduce";
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
    if (rank == 0) {
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        u64 len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
        HcclBuf locBuf{inputMem.ptr(), len, nullptr};
        HcclBuf rmtBuf{remoteOutputPtr, len, nullptr};
        CHK_RET(HcclRemoteWrite(&stream, link.get(), &rmtBuf, &locBuf));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 2));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 2, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 2));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 2, NOTIFY_TIMEOUT));
        CHK_RET(CommLocalReduce(thread, static_cast<void *>(outputMem.ptr()), static_cast<void *>(inputMem.ptr()), count,
            HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult PrimitiRemoteWriteWithReduce(HcclDispatcher dispatcher, DeviceMem &inputMem, DeviceMem &outputMem, Stream &stream,
    ThreadHandle thread, std::shared_ptr<hccl::Transport> &link, u32 rank, bool isWithReduce)
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
    CHK_RET(CommTaskPrepare(nullptr, key.length()));
    uint64_t len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
    if (rank == 0) {
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 0));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 0, NOTIFY_TIMEOUT));
        u64 len = count * SIZE_TABLE[HCCL_DATA_TYPE_FP32];
        HcclBuf locBuf{inputMem.ptr(), len, nullptr};
        HcclBuf rmtBuf{remoteOutputPtr, len, nullptr};
        if (!isWithReduce) {
            CHK_RET(HcclRemoteWrite(&stream, link.get(), &rmtBuf, &locBuf));
        } else {
            HcclReduceInfo reduceInfo{HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM};
            CHK_RET(HcclRemoteWriteReduce(&stream, link.get(), &rmtBuf, &locBuf, reduceInfo));
        }
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 1));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(HcclRemoteNotifyRecord(&stream, link.get(), 2));
        CHK_RET(HcclRemoteNotifyWait(&stream, link.get(), 2, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        if (isWithReduce) {
            CHK_RET(CommLocalCopy(thread, outputMem.ptr(), inputMem.ptr(), len));
        }
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
        if (!isWithReduce) {
            CHK_RET(CommLocalReduce(thread, static_cast<void *>(outputMem.ptr()), static_cast<void *>(inputMem.ptr()), count,
                HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        }
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
            if (ptr[i] != 2) {
                printf("hostMem[%d] = %f\n", i, ptr[i]);
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

TEST_F(MPI_Pcie_Test, st_data_plane_interface_p2p)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo commId;

    ret = hcclComm::GetUniqueId(&commId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = DlTdtFunction::GetInstance().DlTdtFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);

    std::string inter_collective_id = commId.internal;

    std::string port_name = "mlx5_0";
    u32 ipAddr[size];
    if (rank == 0)
    {
        for (int i = 0; i < size; i++ )
        {
            (void)rt_get_dev_ip(0, i, &ipAddr[i]);
        }
    }
    MPI_Bcast(ipAddr, sizeof(ipAddr), MPI_BYTE, 0, MPI_COMM_WORLD);
    ra_set_work_mode(1);
    const s32 ndev = 2;
    s32 dev_list[ndev] = {1, 0};
    std::vector<s32> device_ids(dev_list, dev_list+ndev);
    const u32 rank_list[ndev] = {0, 1};
    std::vector<u32> user_ranks(rank_list, rank_list+ndev);

    // set device
    s32 device_id = dev_list[rank];
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 devicePhyId;
    ret = hrtGetDevicePhyIdByIndex((u32)device_id, devicePhyId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetDevCtx portCtx;
    ret = HcclNetOpenDev(&portCtx, NicType::VNIC_TYPE, devicePhyId, devicePhyId, HcclIpAddress(devicePhyId));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    netDevCtxMap.insert(make_pair(HcclIpAddress(devicePhyId), portCtx));

    IntraExchanger exchanger{};
    ret = CreateIntraExchanger(inter_collective_id, portCtx, device_id, devicePhyId, rank_list[rank], ndev,
        device_ids, user_ranks, true, exchanger);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    s32 mem_size = 256;
    DeviceMem inputMem = DeviceMem::alloc(mem_size);
    DeviceMem outputMem = DeviceMem::alloc(mem_size);
    sal_memset(inputMem.ptr(), sizeof(mem_size), 0, sizeof(mem_size));
    sal_memset(outputMem.ptr(), sizeof(mem_size), 0, sizeof(mem_size));
    u32 notifyNum = 3;
    MachinePara machine_para;
    machine_para. localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_DUPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910B;
    machine_para.linkAttribute = 0x1;
    machine_para.deviceLogicId = device_id;
    machine_para.tag = "default";
    machine_para.notifyNum = notifyNum;
    machine_para.isAicpuModeEn = true;

    if (0 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machine_para.localUserrank = 0;
        machine_para.remoteUserrank = 1;
    }
    else if (1 == rank)
    {
        machine_para.machineType = MachineType::MACHINE_SERVER_TYPE;
        machine_para.localUserrank = 1;
        machine_para.remoteUserrank = 0;
    }

    auto item = exchanger.socketsMap.find(rank_list[machine_para.remoteUserrank]);
    auto socketLists = item->second;
    for (u32 i = 0; i < socketLists.size(); i++) {
        machine_para.sockets.push_back(socketLists[i]);
    }

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

    const std::string tag = "default";
    ret = notifyPool->RegisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    /*创建link*/
    machine_para.collectiveId = inter_collective_id;
    std::shared_ptr<Transport> link = nullptr;
    TransportPara para = {};
    link.reset(new Transport(TransportType::TRANS_TYPE_P2P, para, dispatcher, notifyPool, machine_para));
    ret = link->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步
    HcclThread mianThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY);
    ret = mianThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    Stream *stream = mianThread.GetStream();
    EXPECT_NE(stream, nullptr);

    ret = LocalCopyWithReadReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiveLocalCopyWithReadReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = ReadWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiReadWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, reinterpret_cast<uint64_t>(&mianThread), link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclStreamSynchronize(stream->ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*让dispatcher在link之后析构*/
    ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool = nullptr;
    link = nullptr;
    if (ctx != nullptr) {
        ret = DestoryDispatcherCtx(ctx);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    HcclNetCloseDev(portCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, devicePhyId, devicePhyId);

    SetFftsSwitch(ffts);
    GlobalMockObject::verify();
}
