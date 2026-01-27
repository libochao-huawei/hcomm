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


#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>


#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"

#define private public
#define protected public
#include "transport_ibverbs_pub.h"
#include "multi_qpInfo_manager.h"
#include "hccl_nslbdp.h"
#undef protected
#undef private
#include "rank_consistentcy_checker.h"
#include "dlra_function.h"
#include "tsd/tsd_client.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"
#include "externalinput.h"
using namespace std;
using namespace hccl;

class MPI_Link_Exp_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_Link_Exp_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Link_Exp_Test TearDown" << std::endl;
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
        std::cout << "MPI_Link_Exp_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "MPI_Link_Exp_Test TearDown" << std::endl;
    }

const char *ibv_exp_server_ip = "192.168.11.100";
const char *ibv_exp_client_ip = "192.168.11.101";
};

// 注：socket 与 rdma返回相同的值
extern void* InitRaNicResource(u32 deviceId, const char *deviceIp, void *rdmaHandle);

extern std::unique_ptr<NotifyPool> get_notifyPool(s32 rank);

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_init)
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
    machine_para. localDeviceId = 0;
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
    hrtSetDevice(rank);
    char strTag[100] = "my tag";
    char strGroup[100] = "my group";
    RankConsistentcyChecker::GetInstance().RecordOpPara(HcclCMDType::HCCL_CMD_ALLREDUCE, strTag, 100, HCCL_DATA_TYPE_FP32,
        HCCL_REDUCE_MAX, 0, 0, strGroup, 0);

    char strVersion[100] = "current Version 1";
    RankConsistentcyChecker::GetInstance().RecordVerInfo(strVersion);
    HCCL_INFO(" RankConsistentcyChecker information set SUCCESS[INFO]");

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
}

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_client_tx)
{
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

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
    machine_para.notifyNum = 8;


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
    // hrtSetDevice(rank);

    std::shared_ptr<TransportIbverbs> link_exp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
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

    s32 host_mem_size_temp = 128;
    HostMem host_mem_temp = HostMem::alloc(host_mem_size_temp);
    sal_memset(host_mem_temp.ptr(), host_mem_size_temp, 0 , host_mem_size_temp);
    const char* tempmsg = "hello client from server temp";
    s32 tempmsg_len = SalStrLen(tempmsg) + 1;
    /*rank 0(client)，发送数据给rank 1(server)*/
    if (0 == rank)
    {
        /*rank 0发送消息*/
        DeviceMem tx_buf = inputMem.range(1, msg_len);

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream); // D H
        EXPECT_EQ(ret, HCCL_E_PTR);

        /* 异常参数覆盖 */
        ret = sal_memcpy(host_mem_temp.ptr(), tempmsg_len, tempmsg , tempmsg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        /* 异常参数覆盖 */
        ret = dispatcher->MemcpyAsync(host_mem_temp, inputMem, stream); //H D
        EXPECT_EQ(ret, HCCL_E_PTR);

        /* 异常参数覆盖 */
        ret = dispatcher->MemcpyAsync(host_mem_temp, host_mem, stream); // H H
        EXPECT_EQ(ret, HCCL_E_PTR);

        /* 异常参数覆盖 */
        ret = dispatcher->MemcpyAsync(tx_buf, inputMem, stream); // D D
        EXPECT_EQ(ret, HCCL_E_PTR);

        ret =  link_exp->RxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        for(u32 i = 0; i < 8; i++) {
            ret =  link_exp->Post(i, stream);
            EXPECT_EQ(ret, HCCL_SUCCESS);
        }

        std::vector<HcclSignalInfo> localNotify;
        ret =  link_exp->GetLocalNotify(localNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        std::vector<AddrKey> remoteNotify;
        ret =  link_exp->GetRemoteRdmaNotifyAddrKey(remoteNotify);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = rtStreamSynchronize(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = outputMem;

        ret = link_exp->TxAck(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        for(u32 i = 0; i < 8; i++) {
            ret =  link_exp->Wait(i, stream);
            EXPECT_EQ(ret, HCCL_SUCCESS);
        }

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = rtStreamSynchronize(stream.ptr());
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

}

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_client_tx_81)
{
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
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
    machine_para.deviceType = DevType::DEV_TYPE_910B;//V81
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
    // hrtSetDevice(rank);

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

        ret = rtStreamSynchronize(stream.ptr());
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

        ret = rtStreamSynchronize(stream.ptr());
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

}

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_client_writereduce_81)
{
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
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
    machine_para.deviceType = DevType::DEV_TYPE_910B;//V81
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
    // hrtSetDevice(rank);

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

        void *remoteAddr = nullptr;
        ret = link_exp->GetRemoteMem(hccl::UserMemType::OUTPUT_MEM, &remoteAddr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        struct Transport::Buffer remoteBuf(remoteAddr, 1);
        struct Transport::Buffer localBuf(tx_buf.ptr(), 1); 
        ret = link_exp->WriteReduceAsync(remoteBuf, localBuf,
                                     HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = rtStreamSynchronize(stream.ptr());
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

        ret = rtStreamSynchronize(stream.ptr());
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

}

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_write_read)
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
    machine_para.deviceType = DevType::DEV_TYPE_910B;//V81
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
    // hrtSetDevice(rank);

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
        DeviceMem &tx_buf = inputMem;

        ret = sal_memcpy(host_mem.ptr(), msg_len, msg , msg_len);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(tx_buf, host_mem, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret =  link_exp->WaitReady(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        void *remoteAddr = nullptr;
        ret = link_exp->GetRemoteMem(hccl::UserMemType::OUTPUT_MEM, &remoteAddr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        struct Transport::Buffer remoteBuf(remoteAddr, msg_len);
        struct Transport::Buffer localBuf(tx_buf.ptr(), msg_len);
        ret = link_exp->WriteAsync(remoteBuf, localBuf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret =  link_exp->PostFin(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = rtStreamSynchronize(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("Tx Msg:[%s], tx_buf[%p]", host_mem.ptr(), tx_buf.ptr());
    }
    else if (1 == rank)
    {
        /*rank 1接收消息*/
        DeviceMem &rx_buf = outputMem;

        ret = link_exp->PostReady(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret =  link_exp->WaitFin(stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = dispatcher->MemcpyAsync(host_mem, rx_buf, stream);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = rtStreamSynchronize(stream.ptr());
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

}

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_op_base)
{
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 设置op base 模式
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

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
    // hrtSetDevice(rank);

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

        ret = rtStreamSynchronize(stream.ptr());
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

        ret = rtStreamSynchronize(stream.ptr());
        EXPECT_EQ(ret, HCCL_SUCCESS);

        HCCL_INFO("rx Msg:[%s],rx_buf[%p]", host_mem.ptr(), rx_buf.ptr());
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // 设置op base 模式
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

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
}

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_server_tx)
{
    RankConsistentcyChecker::GetInstance().RecordProtocolType(ProtocolType::RESERVED);
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
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
    //hrtSetDevice(rank);

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

        ret = rtStreamSynchronize(stream.ptr());
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

        ret = rtStreamSynchronize(stream.ptr());
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

    dispatcher = nullptr;
}

HcclResult HrtRaGetHccnCfgDevNotConfig(s32 networkMode, u32 devicePhyId, enum HccnCfgKeyT key, std::string &value)
{
    value = "";
    return HcclResult::HCCL_SUCCESS;
}

TEST_F(MPI_Link_Exp_Test, st_mpi_link_exp_init_multi_qp)
{
    s32 ret;
    s32 size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ResetInitState();

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    TsdOpen(1, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtSetDevice(rank);

    ///////////////////////初始化QP////////////////////////
    u32 qpsPerConnection = 1;
    setenv("HCCL_RDMA_QPS_PER_CONNECTION", "8", 1);
    setenv("HCCL_MULTI_QP_THRESHOLD", "1", 1);
    HcclWorkflowMode oldMode = GetWorkflowMode();
    ret = SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = InitEnvVarParam();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    qpsPerConnection = GetExternalInputQpsPerConnection();
    EXPECT_EQ(qpsPerConnection, 8);
    ///////////////////////////////////////////////////////

    /*创建dispatcher*/
    SetFftsSwitch(false);
    void *dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);

    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    ResetInitState();
    InitExternalInput();

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    /*创建link*/
    std::string port_name = "mlx5_0";

    s32 host_mem_size = 256*1024;
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
    machine_para.nicDeploy == NICDeployment::NIC_DEPLOYMENT_DEVICE;


    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket);

    std::shared_ptr<HcclSocket> newSocket1(new (std::nothrow)HcclSocket("my tag", nullptr,
        remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machine_para.sockets.push_back(newSocket1);

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

    MOCKER(HrtRaGetHccnCfg).stubs().will(invoke(HrtRaGetHccnCfgDevNotConfig));
    hccl::MulQpInfo mulQpInfoManager;
    hccl::InitParams initParams(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    EXPECT_EQ(mulQpInfoManager.Init(initParams), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQp = false;
    EXPECT_EQ(mulQpInfoManager.IsEnableMulQp(isEnableMulQp), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(isEnableMulQp);

    MUL_QP_FROM mulQpFrom;
    EXPECT_EQ(mulQpInfoManager.GetMulQpFromType(mulQpFrom), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(mulQpFrom == MUL_QP_FROM::MUL_QP_FROM_ENV_PER_CONNECTION);

    PortNum portNum = 0;
    EXPECT_EQ(
        mulQpInfoManager.GetPortsNumByIpPair(portNum),
        HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(portNum == 8);
    EXPECT_EQ(mulQpInfoManager.GetSpecialSourcePortsByIpPair(machine_para.srcPorts), HcclResult::HCCL_SUCCESS);
    EXPECT_TRUE(machine_para.srcPorts.size() == 8);

    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));

    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    link_exp->nicRdmaHandle_ = nicRdmaHandle;

    ret = link_exp->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // alloc stream
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    void *remoteAddr = nullptr;
    ret = link_exp->GetRemoteMem(hccl::UserMemType::INPUT_MEM, &remoteAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    struct Transport::Buffer remoteBuf(remoteAddr, host_mem_size);
    struct Transport::Buffer localBuf(inputMem.ptr(), host_mem_size);
    ret = link_exp->WriteAsync(remoteBuf, localBuf, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = link_exp->GetRemoteMem(hccl::UserMemType::OUTPUT_MEM, &remoteAddr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    struct Transport::Buffer remoteOutputBuf(remoteAddr, host_mem_size);
    struct Transport::Buffer localOutputBuf(outputMem.ptr(), host_mem_size); 
    ret = link_exp->WriteReduceAsync(
        remoteOutputBuf, localOutputBuf, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Barrier(MPI_COMM_WORLD);

    hrtRaSocketDeInit(nicRdmaHandle);
	ret = notifyPool->UnregisterOp(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 测试图模式不开启多QP begin
    ret = SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hccl::MulQpInfo mulQpInfoManagerGraph;
    hccl::InitParams initParamsGraph(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910_93);
    EXPECT_EQ(mulQpInfoManagerGraph.Init(initParamsGraph), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQpGraph = true;
    EXPECT_EQ(mulQpInfoManagerGraph.IsEnableMulQp(isEnableMulQpGraph), HcclResult::HCCL_SUCCESS);
    EXPECT_FALSE(isEnableMulQpGraph);
    ret = SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // end

    /* 让dispatcher在link之后析构 */
    link_exp = nullptr;

    // 测试 910A 下的功能不启动
    machine_para.deviceType = DevType::DEV_TYPE_910;
    hccl::MulQpInfo mulQpInfoManager910A;
    hccl::InitParams initParams910A(
        NICDeployment::NIC_DEPLOYMENT_HOST, 0, DevType::DEV_TYPE_910);
    EXPECT_EQ(mulQpInfoManager910A.Init(initParams910A), HcclResult::HCCL_SUCCESS);
    bool isEnableMulQp910A = true;
    EXPECT_EQ(mulQpInfoManager910A.IsEnableMulQp(isEnableMulQp910A), HcclResult::HCCL_SUCCESS);
    EXPECT_FALSE(isEnableMulQp910A);
    machine_para.srcPorts.resize(HCCL_QPS_PER_CONNECTION_DEFAULT, 0);
    link_exp.reset(new TransportIbverbs(dispatcher, notifyPool, machine_para, timeout));
    auto qpsNum = link_exp->GetQpsPerConnection();
    EXPECT_EQ(qpsNum, HCCL_QPS_PER_CONNECTION_DEFAULT);
    link_exp = nullptr;

    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();
    TsdClose(1);

    /// 重置 多QP配置
    ret = SetWorkflowMode(oldMode);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    unsetenv("HCCL_RDMA_QPS_PER_CONNECTION");
    unsetenv("HCCL_MULTI_QP_THRESHOLD");
    ResetInitState();
}