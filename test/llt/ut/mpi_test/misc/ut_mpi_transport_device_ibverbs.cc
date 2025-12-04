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
#include "transport_direct_npu_pub.h"
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

TEST_F(MPI_TRANSPORT_DEVICE_IBVERBS_TEST, ut_transport_direct_npu_test)
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
    machinePara.inputMem = mem;
    machinePara.isAicpuModeEn = true;
    HcclIpAddress remoteIp("192.168.0.24");
    HcclIpAddress localIp("192.168.0.23");
    machinePara.remoteIpAddr = remoteIp;
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test",
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportDirectNpu transportdirects(dispatcher, notifyPool, machinePara, timeout);

    MOCKER_CPP_VIRTUAL(transportdirects, &TransportDirectNpu::RegUserMem)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<TransportDirectNpu> linktmp = nullptr;
    linktmp.reset(new TransportDirectNpu(dispatcher, notifyPool, machinePara, timeout));

    MOCKER_CPP(&TransportDirectNpu::GetNicHandle, HcclResult(TransportDirectNpu::*)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    linktmp->Init();
    linktmp->TxPrepare(stream);
    linktmp->RxPrepare(stream);
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
    linktmp->TxDataSignal(stream);
    linktmp->RxDataSignal(stream);
    linktmp->TxWaitDone(stream);
    linktmp->DataReceivedAck(stream);
    linktmp->PostFinAck(stream);
    linktmp->WaitFinAck(stream);
    linktmp->ConnectSingleQp();
    linktmp->ConstructExchangeForSend();
    linktmp->ParseReceivedExchangeData();
    linktmp->TxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->RxData(UserMemType::INPUT_MEM, 0, mem.ptr(), mem_size, stream);
    linktmp->GetRemoteIp();
    u32 id = 0;
    linktmp->GetTransportId(id);
    TxMemoryInfo info;
    info.dstMemType = UserMemType::OUTPUT_MEM;
    info.dstOffset = 0;
    info.src = nullptr;
    info.len = 1024;
    std::vector<TxMemoryInfo> txMems;
    txMems.push_back(info);
    linktmp->TxAsync(txMems, stream);
    RxMemoryInfo info1;
    info1.srcMemType = UserMemType::INPUT_MEM;
    info1.srcOffset = 0;
    info1.dst = nullptr;
    info1.len = 1024;
    std::vector<RxMemoryInfo> rxMems;
    rxMems.push_back(info1);
    linktmp->RxAsync(rxMems, stream);
    u32 dst = 4;
    void *dstMemPtr = nullptr;
    u64 dstKey = 20;

    UserMemType memType = UserMemType::INPUT_MEM;
    linktmp->GetMemInfo(memType, &dstMemPtr, &dstKey);

    memType =UserMemType::MEM_RESERVED;
    linktmp->GetMemInfo(memType, &dstMemPtr, &dstKey);

    memType =UserMemType::OUTPUT_MEM;
    linktmp->GetMemInfo(memType, &dstMemPtr, &dstKey);

    QpHandle qpHandle;
    AiQpInfo qpInfo;
    linktmp->CreateOneQp(2, 2, qpHandle, qpInfo, true);
    MemType memType1 = MemType::DATA_NOTIFY_MEM;
    u8* exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 2;
    linktmp->RegUserMem(memType1, exchangeDataPtr, exchangeDataBlankSize);

    HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    HcclNetDevCtx nicPortCtx;
    HcclNetOpenDev(&nicPortCtx, NicType::DEVICE_NIC_TYPE, 0, 0, localIp);

    std::shared_ptr<HcclSocketManager> socketManager = nullptr;
    socketManager.reset(new (std::nothrow) HcclSocketManager(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, 0));
    u32 port  = 16666;
    ret = socketManager->ServerInit(nicPortCtx, port);

    std::vector<std::pair<TransportBase*, CqeInfo>> infos;
    u32 cqeNum = 1;

    const u64 a = 1;
    linktmp->g_qpn2IbversLinkMap_.Emplace(a, linktmp.get());
    linktmp->g_flag = true;
    linktmp->g_isSupCqeErrInfoListConfig = true;
    linktmp->GetTransportErrorCqe(nicPortCtx, infos, cqeNum);
    linktmp->g_isSupCqeErrInfoListConfig = false;
    linktmp->GetTransportErrorCqe(nicPortCtx, infos, cqeNum);
    linktmp->DestroyQP();
    MemMsg memMsg;
    linktmp->DeRegMRForQPhandles(memMsg);
    if (dispatcherPtr != nullptr) {
        ret = HcclDispatcherDestroy(dispatcherPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcherPtr = nullptr;
    }

    HcclNetCloseDev(nicPortCtx);
    HcclNetDeInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0);
}