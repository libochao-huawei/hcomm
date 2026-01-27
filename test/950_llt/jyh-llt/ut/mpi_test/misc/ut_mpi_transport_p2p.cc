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
#include "transport_pub.h"
#include "dispatcher_pub.h"
#include "transport_ibverbs_pub.h"
#include "transport_direct_npu_pub.h"
#undef protected
#undef private

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>


#include "../inc/hccl/base.h"
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
#include "data_plane_interface.h"
#include "hccl_primitive_local.h"
#include "hccl_primitive_remote.h"
#include "hccl_dispatcher_ctx.h"
#include "dispatcher_ctx.h"
#include "hccl_thread.h"

using namespace std;
using namespace hccl;

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
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        TsdOpen(1, 2);
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
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

TEST_F(MPI_Pcie_Test, ut_aicpuMc2_link_base_test)
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
    u32 notifyNum = 8;
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
    machine_para.notifyNum = notifyNum;

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

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    EXPECT_EQ(link->userLocalNotify_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotify_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotifyAddr_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotifyOffset_.size(), notifyNum);
    if (0 == rank) {
        /*rank 0发送消息*/
        std::vector<HcclSignalInfo> rdmaNotify;
        ret = link->GetLocalNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = link->GetRemoteNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else if (1 == rank) {
        /*rank 1接收消息*/
        std::vector<HcclSignalInfo> rdmaNotify;
        ret = link->GetLocalNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = link->GetRemoteNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

    }

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

TEST_F(MPI_Pcie_Test, ut_single_mode_link_test)
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
    u32 notifyNum = 8;
    MachinePara machine_para;
    machine_para. localDeviceId = 0;
    machine_para.remoteDeviceId = 0;
    machine_para.linkMode = LinkMode::LINK_SIMPLEX_MODE;
    machine_para.inputMem = inputMem;
    machine_para.outputMem = outputMem;
    machine_para.deviceType = DevType::DEV_TYPE_910;
    machine_para.linkAttribute = 0x1;
    machine_para.deviceLogicId = device_id;
    machine_para.tag = "default";
    machine_para.notifyNum = notifyNum;

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
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    EXPECT_EQ(link->userLocalNotify_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotify_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotifyAddr_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotifyOffset_.size(), notifyNum);
    if (0 == rank) {
        /*rank 0发送消息*/
        std::vector<HcclSignalInfo> rdmaNotify;
        ret = link->GetLocalNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = link->GetRemoteNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

    } else if (1 == rank) {
        /*rank 1接收消息*/
        std::vector<HcclSignalInfo> rdmaNotify;
        ret = link->GetLocalNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = link->GetRemoteNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

    }
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

TEST_F(MPI_Pcie_Test, ut_link_notify_mode_test)
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
    u32 notifyNum = 8;
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
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    EXPECT_EQ(link->userLocalNotify_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotify_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotifyAddr_.size(), notifyNum);
    EXPECT_EQ(link->userRemoteNotifyOffset_.size(), notifyNum);
    if (0 == rank) {
        /*rank 0发送消息*/
        std::vector<HcclSignalInfo> rdmaNotify;
        ret = link->GetLocalNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = link->GetRemoteNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

    } else if (1 == rank) {
        /*rank 1接收消息*/
        std::vector<HcclSignalInfo> rdmaNotify;
        ret = link->GetLocalNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = link->GetRemoteNotify(rdmaNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

    }
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
    if (rank == 0) {
        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommLocalCopy(thread, outputMem.ptr() , inputMem.ptr(), count, HCCL_DATA_TYPE_FP32));
        CHK_RET(CommReadReduce(thread, link.get(), const_cast<void*>(outputMem.ptr()), remoteInputPtr, count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 0));
        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
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
    if (rank == 0) {
        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
        CHK_RET(CommNotifyWait(thread, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, link.get(), 1));
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommRead(thread, link.get(), const_cast<void*>(outputMem.ptr()), remoteInputPtr, count, HCCL_DATA_TYPE_FP32, 0));
        CHK_RET(CommNotifyRecord(thread, link.get(), 1));
        CHK_RET(CommNotifyWait(thread, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommLocalReduce(thread, static_cast<void *>(outputMem.ptr()), static_cast<void *>(inputMem.ptr()), count,
            HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
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
    if (rank == 0) {
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));
        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
        CHK_RET(CommWrite(thread, link.get(), remoteOutputPtr, inputMem.ptr(), count, HCCL_DATA_TYPE_FP32, 0));
        CHK_RET(CommNotifyRecord(thread, link.get(), 1));
        CHK_RET(CommNotifyWait(thread, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, link.get(), 2));
        CHK_RET(CommNotifyWait(thread, link.get(), 2, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
        CHK_RET(CommNotifyRecord(thread, link.get(), 1));
        CHK_RET(CommNotifyWait(thread, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, link.get(), 2));
        CHK_RET(CommNotifyWait(thread, link.get(), 2, NOTIFY_TIMEOUT));
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
    if (rank == 0) {
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));
        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
        if (!isWithReduce) {
            CHK_RET(CommWrite(thread, link.get(), remoteOutputPtr, inputMem.ptr(), count, HCCL_DATA_TYPE_FP32, 0));
        } else {
            CHK_RET(CommWriteReduce(thread, link.get(), remoteOutputPtr, inputMem.ptr(), count, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM, 0));
        }
        CHK_RET(CommNotifyRecord(thread, link.get(), 1));
        CHK_RET(CommNotifyWait(thread, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, link.get(), 2));
        CHK_RET(CommNotifyWait(thread, link.get(), 2, NOTIFY_TIMEOUT));
    } else if (rank == 1) {
        if (isWithReduce) {
            CHK_RET(CommLocalCopy(thread, outputMem.ptr() , inputMem.ptr(), count, HCCL_DATA_TYPE_FP32));
        }
        CHK_RET(CommNotifyRecord(thread, link.get(), 0));
        CHK_RET(CommNotifyWait(thread, link.get(), 0, NOTIFY_TIMEOUT));
        void *remoteInputPtr;
        void *remoteOutputPtr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &remoteInputPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &remoteOutputPtr));

        printf("IsSupportSDMAReduce %d\n", IsSupportSDMAReduce(outputMem.ptr(), remoteInputPtr, HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM));
        printf("-----------------------------\n");

        CHK_RET(CommNotifyRecord(thread, link.get(), 1));
        CHK_RET(CommNotifyWait(thread, link.get(), 1, NOTIFY_TIMEOUT));
        CHK_RET(CommNotifyRecord(thread, link.get(), 2));
        CHK_RET(CommNotifyWait(thread, link.get(), 2, NOTIFY_TIMEOUT));
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
        CHK_RET(CommLocalCopy(thread, outputMem.ptr() , inputMem.ptr(), count, HCCL_DATA_TYPE_FP32));
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

    // CHK_RET(CommTaskPrepare(const_cast<char*>(key.c_str()), key.length()));
    CHK_RET(CommTaskPrepare(nullptr, key.length()));
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
            CHK_RET(CommLocalCopy(thread, outputMem.ptr() , inputMem.ptr(), count, HCCL_DATA_TYPE_FP32));
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

TEST_F(MPI_Pcie_Test, ut_data_plane_interface_p2p)
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

    ret = LocalCopyWithReadReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiveLocalCopyWithReadReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = ReadWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiReadWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
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

const char *ibv_exp_server_ip  = "192.168.1.62";
const char *ibv_exp_client_ip  = "192.168.1.63";
// 注：socket 与 rdma返回相同的值
void* InitRaNicResource(u32 deviceId, const char *deviceIp, void *rdmaHandle)
{
    HcclResult ret;
    HcclIpAddress ipaddr(deviceIp);
    CHK_PRT_RET(ipaddr.IsInvalid(), HCCL_ERROR("ip[%s] change failed", deviceIp), nullptr);

    struct rdev nicRdevInfo;
    nicRdevInfo.phyId = deviceId;
    nicRdevInfo.family = ipaddr.IsIPv6() ? AF_INET6 : AF_INET;
    nicRdevInfo.localIp.addr = ipaddr.GetBinaryAddress().addr;
    nicRdevInfo.localIp.addr6 = ipaddr.GetBinaryAddress().addr6;
    ret = hrtRaSocketInit(NETWORK_OFFLINE, nicRdevInfo, rdmaHandle);
    CHK_PRT_RET(ret, HCCL_ERROR("errNo[0x%016llx] deviceId[%d] init nic resource failed", HCCL_ERROR_CODE(ret), deviceId), nullptr);
    return rdmaHandle;
}

TEST_F(MPI_Pcie_Test, ut_data_plane_interface_rdma)
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

    HcclThread subThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY);
    ret = subThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = RemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteWriteWithLocalReduce(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = PrimitiRemoteBatchWriteRead(dispatcherPtr, inputMem, outputMem, *stream, &mianThread, link, rank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclStreamSynchronize(stream->ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = CommLocalRecord(&mianThread, &subThread, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = CommLocalWait(&subThread, 0, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CommLocalRecord(&subThread, &mianThread, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = CommLocalWait(&mianThread, 1, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclStreamSynchronize(stream->ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = CommWriteReduceWithNotify(&mianThread, link.get(), inputMem.ptr(), inputMem.ptr(), inputMem.size(), HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_SUM, 0, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    HcclBuf rmtBuf{inputMem.ptr(), inputMem.size(), nullptr};
    HcclReduceInfo reduceInfo{HCCL_DATA_TYPE_FP32, HCCL_REDUCE_SUM};
    ret = HcclRemoteWriteReduceWithNotify(stream, link.get(), &rmtBuf, &rmtBuf, reduceInfo, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = CommWriteWithNotify(&mianThread, link.get(), inputMem.ptr(), inputMem.ptr(), inputMem.size(), HCCL_DATA_TYPE_FP32, 0, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = HcclRemoteWriteWithNotify(stream, link.get(), &rmtBuf, &rmtBuf, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    HcclBatchTransferInfo info;
    ret = HcclRemoteBatchTransfer(stream, link.get(), &info, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = CommFence(&mianThread, link.get());
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

TEST_F(MPI_Pcie_Test, ut_hrt_socket_recv)
{
    int i = 0;
    FdHandle fdHandle = &i;
    HcclResult ret = HCCL_SUCCESS;

    MOCKER(RaSocketRecv)
    .expects(once())
    .will(returnValue(10));
    ret = hrtRaSocketBlockRecv(fdHandle, &i, sizeof(int));
    EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);
    GlobalMockObject::verify();
}

TEST_F(MPI_Pcie_Test, ut_direct_init)
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

    std::shared_ptr<TransportDirectNpu> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    char strTag[100] = "my tag";
    char strGroup[100] = "my group";
    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";
    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);
    HCCL_INFO(" RankConsistentcyChecker information set SUCCESS[INFO]");

    link_exp.reset(new TransportDirectNpu(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportDirectNpu::GetNicHandle, HcclResult(TransportDirectNpu::*)())
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

TEST_F(MPI_Pcie_Test, ut_direct_error_init)
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

    std::shared_ptr<TransportDirectNpu> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);

    char strTag[100] = "my tag";
    char strGroup[100] = "my group";
    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";
    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);

    link_exp.reset(new TransportDirectNpu(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportDirectNpu::GetNicHandle, HcclResult(TransportDirectNpu::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    ret = link_exp->Init();

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