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

#include <stdio.h>
#include <stdlib.h>
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
#define private public
#define protected public
#include "dlra_function.h"
#include "adapter_rts.h"
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
#include "tcp_recv_task.h"
#include "hccl_communicator.h"
#include "hccd_impl_pml.h"
#include "transport_heterog.h"
#include "transport_heterog_pub.h"
#include "local_rdma_rma_buffer_impl.h"
#include "local_ipc_rma_buffer_impl.h"
#include "transport_mem.h"
#include "transport_roce_mem.h"
#include "transport_device_roce_mem.h"
#include "transport_ipc_mem.h"
#include "tcp_send_thread_pool.h"
#undef private

#include <hccl/hccl_comm.h>
#include <hccl/hccl_inner.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "hccl/base.h"
#include "hccl/hcom.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "rank_consistentcy_checker.h"
#include "dlhal_function.h"
#include "externalinput.h"
#include "hccl_network.h"

using namespace std;
using namespace hccl;

constexpr u32 RECV_WQE_BATCH_NUM = 192;
constexpr u32 RECV_WQE_NUM_THRESHOLD = 96;
constexpr u32 RECV_WQE_BATCH_SUPPLEMENT = 96;

class ST_MPI_TRANSPORT_MEM_TEST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "ST_MPI_TRANSPORT_MEM_TEST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "ST_MPI_TRANSPORT_MEM_TEST TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        hrtSetDevice(0);
        ResetInitState();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        ClearHalEvent();
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A TestCase TearDown" << std::endl;
    }
};


HcclResult stub_CreateNormalQp1(RdmaHandle rdmaHandle, QpInfo &qp)
{
    static struct ibv_qp sqp = {0};
    qp.qp = &sqp;
    return HCCL_SUCCESS;
}

HcclResult stub_CreateQp1(RdmaHandle rdmaHandle, int &flag, s32 &qpMode, QpInfo &qp)
{
    static struct ibv_qp sqp = {0};
    qp.qp = &sqp;
    return HCCL_SUCCESS;
}

s32 hrtGetRaQpStatusStub(QpHandle handle, int *status)
{
    *status = 1;
    return 0;
}

s32 hrtRaPollCqStub(QpHandle handle, bool is_send_cq, unsigned int num, void *wc)
{
    CHK_PTR_NULL(handle);
    CHK_PTR_NULL(wc);
    auto ww = reinterpret_cast<struct ibv_wc *>(wc);
    ww->status = ibv_wc_status::IBV_WC_SUCCESS;
    ww->wr_id = 0;
    return 1;
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_TransportMem_Init_Roce)
{
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    s32 rank = 0;
    std::string commTag = "SocketManagerTest";
    MOCKER(hrtRaCreateCq)
        .stubs()
        .with(any())
        .will(returnValue(0));

    MOCKER(hrtRaNormalQpCreate)
        .stubs()
        .with(any())
        .will(returnValue(0));

    MOCKER(HrtRaQpCreate)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::DeInit, HcclResult (MrManager::*)(const void *))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::InitMrManager, HcclResult (MrManager::*)(void *))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaMrReg)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockRecv)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoceMem::RecoverNotifyMsg)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaQpNonBlockConnectAsync)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBatchClose)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(DestroyQpWithCq)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    int qpStatus = 1;
    MOCKER(hrtGetRaQpStatus)
        .stubs()
        .with(any())
        .will(invoke(hrtGetRaQpStatusStub));

    MOCKER(CreateQp)
        .stubs()
        .with(any())
        .will(invoke(stub_CreateQp1));

    MOCKER(CreateNormalQp, HcclResult (*)(RdmaHandle, QpInfo&))
        .stubs()
        .with(any())
        .will(invoke(stub_CreateNormalQp1));

    MOCKER(CreateQpWithCq)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::GetDevVirAddr)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaSendWrV2)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRDMASend)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRDMADBSend)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtCtxSetCurrent)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaGetNotifyBaseAddr)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaPollCq)
        .stubs()
        .with(any())
        .will(invoke(hrtRaPollCqStub));

    MOCKER(LocalIpcNotify::Wait)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DispatcherPub::SignalWait,
            HcclResult(DispatcherPub:: *)(HcclRtNotify, HcclRtStream, u32, u32, s32, u32, bool))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaMrDereg)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NotifyPool::RegisterOp)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NotifyPool::UnregisterOp)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NotifyPool::Alloc)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalIpcNotify::Grant)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalIpcNotify::GetNotifyOffset)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalIpcNotify::GetNotifyData).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(RaQpConnectAsync)
        .stubs()
        .with(any())
        .will(returnValue(EOK));

    MOCKER_CPP(&HcclSocket::DeInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Close).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaDeRegGlobalMr).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaRegGlobalMr).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NetworkManager::StopHostNet).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NetDevContext::Deinit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclNetCloseDev).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    //init dispatcher


    SetFftsSwitch(false);
    HcclDispatcher dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    dispatcher->AddRetryPreamble(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //init notifyPool
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool->RegisterOp(commTag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //init Net
    u32 socketsPerLink = 1;
    NicType socketType = NicType::DEVICE_NIC_TYPE;
    HcclSocketRole localRole = HcclSocketRole::SOCKET_ROLE_RESERVED;
    HcclIpAddress localIp;
    ret = localIp.SetReadableAddress("127.0.0.1");
    RaResourceInfo raResourceInfo;
    IpSocket ipSocket;
    u64 nicSocketHandle = 0;
    rdevInfo_t nicRdmaHandle = {0};
    ipSocket.nicSocketHandle = reinterpret_cast<void *>(&nicSocketHandle);
    ipSocket.nicRdmaHandle = reinterpret_cast<void *>(&nicRdmaHandle);
    raResourceInfo.nicSocketMap[localIp] = ipSocket;
    MOCKER_CPP(&NetworkManager::GetRaResourceInfo)
        .stubs()
        .with(outBound(raResourceInfo))
        .will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hccl::NetDevContext nicPortCtx;
    nicPortCtx.Init(socketType, 0, 0, localIp);
    //init socket
    std::shared_ptr<HcclSocket> tempSocket = nullptr;
    tempSocket.reset(new (std::nothrow) HcclSocket(reinterpret_cast<HcclNetDevCtx>(&nicPortCtx), 6000));
    tempSocket->Init();
    tempSocket->localRole_ = HcclSocketRole::SOCKET_ROLE_SERVER;
    tempSocket->tag_ = commTag;
    tempSocket->fdHandle_ = (void *)0x01;
    tempSocket->status_ = HcclSocketStatus::SOCKET_OK;

    hrtSetDevice(rank);
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 0;
    attrInfo.sdid = 0;
    attrInfo.serverId = 0;
    unique_ptr<TransportRoceMem> transport(new (nothrow) TransportRoceMem(notifyPool,
        reinterpret_cast<HcclNetDevCtx>(&nicPortCtx),dispatcherPtr, attrInfo));
    ret = transport->SetSocket(tempSocket);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));
    struct ibv_qp qp= {0};
    transport->dataQpInfo_.qp = &qp;
    transport->dataQpInfo_.qpHandle = (void *)0x1000000;
    transport->rdmaSignalMrHandle_ = (void*)0x2000000;
    transport->notifyValueMemMrHandle_ = (void *)0x3000000;
    transport->trafficClass_ = 0;
    transport->serviceLevel_ = 8; // HCCL_RDMA_SL_MAX 为7，异常分支校验
    ret = transport->Connect(1);
    EXPECT_EQ(ret, HCCL_E_PARA);

    transport->trafficClass_ = 256; // HCCL_RDMA_TC_MAX 为255，异常分支校验
    transport->serviceLevel_ = 0;
    ret = transport->Connect(1);
    EXPECT_EQ(ret, HCCL_E_PARA);

    transport->trafficClass_ = 0;
    transport->serviceLevel_ = 0;
    ret = transport->Connect(1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s8* localbuf;
    s8* remotebuf;
    s32 count = 1024;
    localbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(localbuf, count * sizeof(s8), 0, count * sizeof(s8));
    remotebuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(remotebuf, count * sizeof(s8), 0, count * sizeof(s8));
    u64 dev = 0;
    HcclNetDevCtx devCtx = reinterpret_cast<HcclNetDevCtx>(&dev);
    RmaBufferSlice localSlice, remoteSlice;
    std::shared_ptr<LocalRdmaRmaBuffer> tempLocalRdmaBufferPtr = make_shared<LocalRdmaRmaBuffer>(devCtx, localbuf, count * sizeof(s8));
    localSlice.addr = localbuf;
    localSlice.len = count * sizeof(s8);
    localSlice.rmaBuffer = tempLocalRdmaBufferPtr;
    std::shared_ptr<RemoteRdmaRmaBuffer> tempRemoteRdmaBufferPtr = make_shared<RemoteRdmaRmaBuffer>();
    tempRemoteRdmaBufferPtr->addr = remotebuf;
    tempRemoteRdmaBufferPtr->size = count * sizeof(s8);
    tempRemoteRdmaBufferPtr->devAddr = remotebuf;
    remoteSlice.addr = remotebuf;
    remoteSlice.len = count * sizeof(s8);
    remoteSlice.rmaBuffer = tempRemoteRdmaBufferPtr;

    ret = transport->TransportRdmaWithType(localSlice, remoteSlice, stream.ptr(), TransportRoceMem::RdmaOp::OP_WRITE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->TransportRdmaWithType(localSlice, remoteSlice, stream.ptr(), TransportRoceMem::RdmaOp::OP_READ);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->AddOpFence(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    localSlice.len = 1024;
    ret = transport->TransportRdmaWithType(localSlice, remoteSlice, stream.ptr(), TransportRoceMem::RdmaOp::OP_WRITE);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 qpn = 0;
    u32 wqeIndex = 0;
    struct SendWr wr;
    u32 userRank = 0;
    u64 offset = 0;
    ret = dispatcher->RdmaSend(qpn, wqeIndex, wr, stream, userRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dispatcher->RdmaSend(qpn, wqeIndex, wr, stream, userRank, offset);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transport.reset();
    transport = nullptr;
    notifyPool->UnregisterOp(commTag);
    notifyPool->Destroy();
    HcclDispatcherDestroy(dispatcherPtr);
    sal_free(localbuf);
    sal_free(remotebuf);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_TransportMem_Init_Ipc)
{
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    s32 rank = 0;
    std::string commTag = "SocketManagerTest";

    MOCKER_CPP(&HcclSocket::DeInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclSocket::Close).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NetworkManager::StopHostNet).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&NetDevContext::Deinit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclNetCloseDev).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtMemAsyncCopy).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    //init dispatcher


    SetFftsSwitch(false);
    HcclDispatcher dispatcherPtr = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, rank, &dispatcherPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcherPtr, nullptr);
    DispatcherPub * dispatcher= reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    dispatcher->AddRetryPreamble(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //init notifyPool
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    ret = notifyPool->Init(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    notifyPool->RegisterOp(commTag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //init Net
    u32 socketsPerLink = 1;
    NicType socketType = NicType::DEVICE_NIC_TYPE;
    HcclSocketRole localRole = HcclSocketRole::SOCKET_ROLE_RESERVED;
    HcclIpAddress localIp;
    ret = localIp.SetReadableAddress("127.0.0.1");
    RaResourceInfo raResourceInfo;
    IpSocket ipSocket;
    u64 nicSocketHandle = 0;
    rdevInfo_t nicRdmaHandle = {0};
    ipSocket.nicSocketHandle = reinterpret_cast<void *>(&nicSocketHandle);
    ipSocket.nicRdmaHandle = reinterpret_cast<void *>(&nicRdmaHandle);
    raResourceInfo.nicSocketMap[localIp] = ipSocket;
    MOCKER_CPP(&NetworkManager::GetRaResourceInfo)
        .stubs()
        .with(outBound(raResourceInfo))
        .will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hccl::NetDevContext nicPortCtx;
    nicPortCtx.Init(socketType, 0, 0, localIp);
    //init socket
    std::shared_ptr<HcclSocket> tempSocket = nullptr;
    tempSocket.reset(new (std::nothrow) HcclSocket(reinterpret_cast<HcclNetDevCtx>(&nicPortCtx), 6000));
    tempSocket->Init();
    tempSocket->localRole_ = HcclSocketRole::SOCKET_ROLE_SERVER;
    tempSocket->tag_ = commTag;
    tempSocket->fdHandle_ = (void *)0x01;
    tempSocket->status_ = HcclSocketStatus::SOCKET_OK;

    hrtSetDevice(rank);
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 0;
    attrInfo.sdid = 0;
    attrInfo.serverId = 0;
    unique_ptr<TransportIpcMem> transport(new (nothrow) TransportIpcMem(notifyPool,
        reinterpret_cast<HcclNetDevCtx>(&nicPortCtx),dispatcherPtr,attrInfo));
    s8* localbuf;
    s8* remotebuf;
    s32 count = 1024;
    localbuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(localbuf, count * sizeof(s8), 0, count * sizeof(s8));
    remotebuf= (s8*)sal_malloc(count * sizeof(s8));
    sal_memset(remotebuf, count * sizeof(s8), 0, count * sizeof(s8));
    HcclNetDevCtx devCtx;
    RmaBufferSlice localSlice, remoteSlice;
    std::shared_ptr<LocalIpcRmaBuffer> tempLocalIpcBufferPtr = make_shared<LocalIpcRmaBuffer>(devCtx, localbuf, count * sizeof(s8));
    localSlice.addr = localbuf;
    localSlice.len = count * sizeof(s8);
    localSlice.rmaBuffer = tempLocalIpcBufferPtr;
    localSlice.memType = RmaMemType::DEVICE;
    std::shared_ptr<RemoteIpcRmaBuffer> tempRemoteIpcBufferPtr = make_shared<RemoteIpcRmaBuffer>(devCtx);
    tempRemoteIpcBufferPtr->addr = remotebuf;
    tempRemoteIpcBufferPtr->size = count * sizeof(s8);
    tempRemoteIpcBufferPtr->devAddr = remotebuf;
    remoteSlice.addr = remotebuf;
    remoteSlice.len = count * sizeof(s8);
    remoteSlice.rmaBuffer = tempRemoteIpcBufferPtr;
    remoteSlice.memType = RmaMemType::DEVICE;

    ret = transport->TransportIpc(remoteSlice, localSlice, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->TransportIpc(localSlice, remoteSlice, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->AddOpFence(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    localSlice.len = 1024;
    ret = transport->TransportIpc(remoteSlice, localSlice, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    localSlice.memType = RmaMemType::HOST;
    ret = transport->TransportIpc(remoteSlice, localSlice, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->TransportIpc(remoteSlice, localSlice, nullptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->TransportIpc(localSlice, remoteSlice, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteSlice.addr = localbuf;
    ret = transport->TransportIpc(remoteSlice, localSlice, nullptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remoteSlice.addr = remotebuf;
    localSlice.len = 0;
    ret = transport->TransportIpc(remoteSlice, localSlice, stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    localSlice.len = HCCL_SDMA_MAX_COUNT_4GB;
    ret = transport->TransportIpc(remoteSlice, localSlice, stream.ptr());
    EXPECT_EQ(ret, HCCL_E_PARA);

    transport.reset();
    transport = nullptr;
    notifyPool->UnregisterOp(commTag);
    notifyPool->Destroy();
    HcclDispatcherDestroy(dispatcherPtr);
    sal_free(localbuf);
    sal_free(remotebuf);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_ipc_mem_exchange)
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportMem> transport = TransportMem::Create(TransportMem::TpType::IPC, notifyPool, netDevCtx, dispatcher, attrInfo);
    TransportIpcMem *transportPtr = dynamic_cast<TransportIpcMem *>(transport.get());
    HcclIpAddress ipAddr;
    std::shared_ptr<HcclSocket> socketPtr = make_shared<HcclSocket>("tagIpc", netDevCtx, ipAddr, 16666, HcclSocketRole::SOCKET_ROLE_SERVER);
    transport->SetDataSocket(socketPtr);

    u64 count = 1024;
    u64 bufSize = 1024 * sizeof(s8);
    s8* localbuf = (s8*)sal_malloc(bufSize);
    sal_memset(localbuf, bufSize, 0, bufSize);

    MOCKER_CPP(&LocalIpcRmaBuffer::Init)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    std::string localdesc = "lRoced";
    MOCKER_CPP(&LocalIpcRmaBuffer::Serialize)
    .stubs()
    .will(returnValue(localdesc));

    TransportMem::RmaMemDesc localMemDesc{};
    localMemDesc.localRankId = 0U;
    localMemDesc.remoteRankId = 1U;
    strcpy(localMemDesc.memDesc, "local");
    TransportMem::RmaMem localMem = {RmaMemType::DEVICE, localbuf, bufSize};

    HcclResult ret;
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .then(returnValue(HCCL_SUCCESS));

    TransportMem::RmaMemDesc remoteMemDescRecv;
    remoteMemDescRecv.localRankId = 1;
    remoteMemDescRecv.remoteRankId = 0;
    strcpy(remoteMemDescRecv.memDesc, "remote");
    u32 numOfRemote = 1;

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32, u32))
    .expects(once())
    .with(outBoundP((void*)&numOfRemote, sizeof(numOfRemote)), eq((u32)sizeof(numOfRemote))) // 接收actualNumOfRemote
    .will(returnValue(HCCL_SUCCESS))
    .id("first");

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32, u32))
    .expects(once())
    .with(outBoundP((void*)&remoteMemDescRecv, sizeof(TransportMem::RmaMemDesc)), eq((u32)sizeof(TransportMem::RmaMemDesc))) // 接收remoteMemDesc
    .after("first")
    .will(returnValue(HCCL_SUCCESS));

    TransportMem::RmaMemDescs localMemDescs = {&localMemDesc, 1};
    TransportMem::RmaMemDesc remoteMemDesc[2];
    TransportMem::RmaMemDescs remoteMemDescs = {remoteMemDesc, 2};
    u32 actualNumOfRemote = 0;
    ret = transport->ExchangeMemDesc(localMemDescs, remoteMemDescs, actualNumOfRemote);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(actualNumOfRemote, 1U);
    sal_free(localbuf);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_ipc_mem_enable_disable_mem_access)
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportMem> transport = TransportMem::Create(TransportMem::TpType::IPC, notifyPool, netDevCtx, dispatcher, attrInfo);
    TransportIpcMem *transportPtr = dynamic_cast<TransportIpcMem *>(transport.get());
    HcclIpAddress ipAddr;
    std::shared_ptr<HcclSocket> socketPtr = make_shared<HcclSocket>("tagIpc", netDevCtx, ipAddr, 16666, HcclSocketRole::SOCKET_ROLE_SERVER);
    transport->SetDataSocket(socketPtr);

    u64 count = 1024;
    u64 bufSize = 1024 * sizeof(s8);
    s8* remotebuf = (s8*)sal_malloc(bufSize);
    sal_memset(remotebuf, bufSize, 0, bufSize);
    shared_ptr<LocalIpcRmaBufferImpl> localbuffer = make_shared<LocalIpcRmaBufferImpl>(netDevCtx, remotebuf, bufSize, RmaMemType::DEVICE);
    LocalIpcRmaBufferImpl* localbufferPtr = localbuffer.get();
    localbufferPtr->devAddr = remotebuf;
    char* ipcName = "ipc";
    strcpy((char*)(localbufferPtr->memName.ipcName), ipcName);
    localbufferPtr->memOffset = 0;
    std::string desc = localbufferPtr->Serialize();

    TransportMem::RmaMemDesc remoteMemDesc;
    remoteMemDesc.localRankId = 1;
    remoteMemDesc.remoteRankId = 0;
    transport->RmaMemDescCopyFromStr(remoteMemDesc, desc);

    MOCKER_CPP(&RemoteIpcRmaBuffer::Open)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&RemoteIpcRmaBuffer::Close)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    // 使能与去使能
    TransportMem::RmaMem remoteMem{};
    HcclResult ret = transport->EnableMemAccess(remoteMemDesc, remoteMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(remoteMem.type, RmaMemType::DEVICE);
    EXPECT_EQ(remoteMem.addr, remotebuf);
    EXPECT_EQ(remoteMem.size, bufSize);
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 同个远端内存两次enable
    ret = transport->EnableMemAccess(remoteMemDesc, remoteMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(remoteMem.type, RmaMemType::DEVICE);
    EXPECT_EQ(remoteMem.addr, remotebuf);
    EXPECT_EQ(remoteMem.size, bufSize);
    ret = transport->EnableMemAccess(remoteMemDesc, remoteMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 同个远端内存两次disable
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // disable未使能的远端内存
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    sal_free(remotebuf);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_ipc_mem_read_write)
{
    HcclResult ret;
    rtStream_t stream;
    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    devContext.localIpcRmaBufferMgr_ = std::make_shared<NetDevContext::LocalIpcRmaBufferMgr>();
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportMem> transport = TransportMem::Create(TransportMem::TpType::IPC, notifyPool, netDevCtx, dispatcher, attrInfo);
    TransportIpcMem *transportPtr = dynamic_cast<TransportIpcMem *>(transport.get());
    HcclIpAddress ipAddr;
    std::shared_ptr<HcclSocket> socketPtr = make_shared<HcclSocket>("tagIpc", netDevCtx, ipAddr, 16666, HcclSocketRole::SOCKET_ROLE_SERVER);
    transport->SetDataSocket(socketPtr);

    u64 count = 1024;
    u64 bufSize = 1024 * sizeof(s8);
    s8* localbuf= (s8*)sal_malloc(bufSize);
    sal_memset(localbuf, bufSize, 0, bufSize);
    BufferKey<uintptr_t, u64> tempLocalKey(reinterpret_cast<uintptr_t>(localbuf), bufSize);
    std::shared_ptr<LocalIpcRmaBuffer> tempLocalIpcBufferPtr = make_shared<LocalIpcRmaBuffer>(netDevCtx, localbuf, bufSize);
    tempLocalIpcBufferPtr->devAddr = localbuf;
    devContext.localIpcRmaBufferMgr_->Add(tempLocalKey, tempLocalIpcBufferPtr);

    s8* remotebuf= (s8*)sal_malloc(bufSize);
    sal_memset(remotebuf, bufSize, 0, bufSize);
    BufferKey<uintptr_t, u64> tempRemoteKey(reinterpret_cast<uintptr_t>(remotebuf), bufSize);
    std::shared_ptr<RemoteIpcRmaBuffer> tempRemoteIpcBufferPtr = make_shared<RemoteIpcRmaBuffer>(netDevCtx);
    tempRemoteIpcBufferPtr->addr = remotebuf;
    tempRemoteIpcBufferPtr->size = bufSize;
    tempRemoteIpcBufferPtr->devAddr = remotebuf;
    tempRemoteIpcBufferPtr->memType = RmaMemType::HOST;
    transportPtr->remoteIpcRmaBufferMgr_.Add(tempRemoteKey, tempRemoteIpcBufferPtr);

    MOCKER_CPP(&DispatcherPub::MemcpyAsyncWithoutCheckKind, HcclResult(DispatcherPub::*)(void*, uint64_t, const void*, u64, HcclRtMemcpyKind, hccl::Stream&, u32, hccl::LinkType))
    .stubs()
    .will(returnValue(HCCL_E_INTERNAL))
    .then(returnValue(HCCL_SUCCESS));

    // Read/Write未注册的内存
    HcclBuf localMem = {localbuf - 1, 1, nullptr};
    HcclBuf remoteMem = {remotebuf, bufSize, tempRemoteIpcBufferPtr.get()};
    ret = transport->Read(localMem, remoteMem, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = transport->Write(remoteMem, localMem, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    TransportMem::RmaOpMem localMemop = {localbuf - 1, 1};
    TransportMem::RmaOpMem remoteMemop = {remotebuf, bufSize};
    ret = transport->Read(localMemop, remoteMemop, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = transport->Write(remoteMemop, localMemop, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    // dispatcher MemcpyAsync失败
    HcclBuf localMem1 = {localbuf + 1, 4, nullptr};
    HcclBuf remoteMem1 = {remotebuf, 4, tempRemoteIpcBufferPtr.get()};
    ret = transport->Read(localMem1, remoteMem1, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    // Read与Write已注册使能的内存中段
    ret = transport->Read(localMem1, remoteMem1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->Write(remoteMem1, localMem1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclBuf localMem2 = {localbuf, bufSize, nullptr};
    HcclBuf remoteMem2 = {remotebuf, bufSize, tempRemoteIpcBufferPtr.get()};
    ret = transport->Read(localMem2, remoteMem2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->Write(remoteMem2, localMem2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportMem::RmaOpMem localMemop2 = {localbuf, bufSize};
    TransportMem::RmaOpMem remoteMemop2 = {remotebuf, bufSize};
    ret = transport->Read(localMemop2, remoteMemop2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->Write(remoteMemop2, localMemop2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    rt_ret = aclrtDestroyStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sal_free(localbuf);
    sal_free(remotebuf);
}

static std::shared_ptr<TransportMem> CreateIpcMemTransport()
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    return TransportMem::Create(TransportMem::TpType::IPC, notifyPool, netDevCtx, dispatcher, attrInfo, true);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_ipc_mem_get_trans_info)
{
    HcclResult ret;
    HcclQpInfoV2 qpInfo;
    std::shared_ptr<TransportMem> transport = CreateIpcMemTransport();
    ret = transport->GetTransInfo(qpInfo, nullptr, nullptr, nullptr, nullptr, 0);   // nullptr
    EXPECT_EQ(ret, HCCL_E_PTR);
    const u32 DESC_NUM = 2;
    u32 lkey[DESC_NUM];
    u32 rkey[DESC_NUM];
    HcclBuf localMem[DESC_NUM];
    memset_s(localMem, sizeof(localMem), 0, sizeof(HcclBuf) * DESC_NUM);
    HcclBuf remoteMem[DESC_NUM];
    memset_s(remoteMem, sizeof(remoteMem), 0, sizeof(HcclBuf) * DESC_NUM);
    ret = transport->GetTransInfo(qpInfo, lkey, rkey, localMem, remoteMem, 0);  // num is 0
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = transport->GetTransInfo(qpInfo, lkey, rkey, localMem, remoteMem, 1);  // num is 1
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->GetTransInfo(qpInfo, lkey, rkey, localMem, remoteMem, DESC_NUM);   // invalid mem
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_ipc_mem_wait_op_frence)
{
    HcclResult ret;
    std::shared_ptr<TransportMem> transport = CreateIpcMemTransport();
    rtStream_t stream;
    ret = transport->WaitOpFence(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_roce_mem_exchange)
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportMem> transport = TransportMem::Create(TransportMem::TpType::ROCE, notifyPool, netDevCtx, dispatcher, attrInfo);
    TransportRoceMem *transportPtr = dynamic_cast<TransportRoceMem *>(transport.get());
    HcclIpAddress ipAddr;
    std::shared_ptr<HcclSocket> socketPtr = make_shared<HcclSocket>("tagRoce", netDevCtx, ipAddr, 16666, HcclSocketRole::SOCKET_ROLE_SERVER);
    transport->SetDataSocket(socketPtr);

    u64 count = 1024;
    u64 bufSize = 1024 * sizeof(s8);
    s8* localbuf = (s8*)sal_malloc(bufSize);
    sal_memset(localbuf, bufSize, 0, bufSize);

    MOCKER_CPP(&LocalRdmaRmaBuffer::Init)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    std::string localdesc = "lRoced";
    MOCKER_CPP(&LocalRdmaRmaBuffer::Serialize)
    .stubs()
    .will(returnValue(localdesc));

    TransportMem::RmaMemDesc localMemDesc{};
    localMemDesc.localRankId = 0U;
    localMemDesc.remoteRankId = 1U;
    strcpy(localMemDesc.memDesc, "local");
    TransportMem::RmaMem localMem = {RmaMemType::DEVICE, localbuf, bufSize};

    HcclResult ret;

    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
    .stubs()
    .with(any())
    .then(returnValue(HCCL_SUCCESS));

    TransportMem::RmaMemDesc remoteMemDescRecv;
    remoteMemDescRecv.localRankId = 1;
    remoteMemDescRecv.remoteRankId = 0;
    strcpy(remoteMemDescRecv.memDesc, "remote");
    u32 numOfRemote = 1;

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32, u32))
    .expects(once())
    .with(outBoundP((void*)&numOfRemote, sizeof(numOfRemote)), eq((u32)sizeof(numOfRemote))) // 接收actualNumOfRemote
    .will(returnValue(HCCL_SUCCESS))
    .id("first");

    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32, u32))
    .expects(once())
    .with(outBoundP((void*)&remoteMemDescRecv, sizeof(TransportMem::RmaMemDesc)), eq((u32)sizeof(TransportMem::RmaMemDesc))) // 接收remoteMemDesc
    .after("first")
    .will(returnValue(HCCL_SUCCESS));

    TransportMem::RmaMemDescs localMemDescs = {&localMemDesc, 1};
    TransportMem::RmaMemDesc remoteMemDesc[2];
    TransportMem::RmaMemDescs remoteMemDescs = {remoteMemDesc, 2};
    u32 actualNumOfRemote = 0;
    ret = transport->ExchangeMemDesc(localMemDescs, remoteMemDescs, actualNumOfRemote);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(actualNumOfRemote, 1U);
    sal_free(localbuf);
}

HcclResult stub_IsSupportRaSendNormalWrlist(bool& isSupportRaSendNormalWrlist)
{
    isSupportRaSendNormalWrlist = false;
    return HCCL_SUCCESS;
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_roce_mem_CheckRaSendNormalWrlistSupport)
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportRoceMem> transport = std::make_unique<TransportRoceMem>(notifyPool, netDevCtx, dispatcher, attrInfo);
    transport->isSupportRaSendNormalWrlist_ = TransportRoceMem::SupportStatus::INIT;

    MOCKER(IsSupportRDMALite)
    .stubs()
    .will(returnValue(true));
    auto ret = transport->CheckRaSendNormalWrlistSupport();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(transport->isSupportRaSendNormalWrlist_, TransportRoceMem::SupportStatus::SUPPORT);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_roce_mem_CheckRaSendNormalWrlistSupport2)
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportRoceMem> transport = std::make_unique<TransportRoceMem>(notifyPool, netDevCtx, dispatcher, attrInfo);

    transport->isSupportRaSendNormalWrlist_ = TransportRoceMem::SupportStatus::INIT;
    MOCKER(IsSupportRDMALite)
    .stubs()
    .will(returnValue(false));
    MOCKER(IsSupportRaSendNormalWrlist)
    .stubs()
    .with(any())
    .will(invoke(stub_IsSupportRaSendNormalWrlist));
    auto ret = transport->CheckRaSendNormalWrlistSupport();
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(transport->isSupportRaSendNormalWrlist_, TransportRoceMem::SupportStatus::NOT_SUPPORT);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_roce_mem_enable_disable_mem_access)
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportMem> transport = TransportMem::Create(TransportMem::TpType::ROCE, notifyPool, netDevCtx, dispatcher, attrInfo);
    TransportRoceMem *transportPtr = dynamic_cast<TransportRoceMem *>(transport.get());
    HcclIpAddress ipAddr;
    std::shared_ptr<HcclSocket> socketPtr = make_shared<HcclSocket>("tagRoce", netDevCtx, ipAddr, 16666, HcclSocketRole::SOCKET_ROLE_SERVER);
    transport->SetDataSocket(socketPtr);

    u64 count = 1024;
    u64 bufSize = 1024 * sizeof(s8);
    s8* remotebuf = (s8*)sal_malloc(bufSize);
    sal_memset(remotebuf, bufSize, 0, bufSize);
    shared_ptr<LocalRdmaRmaBufferImpl> remotebuffer = make_shared<LocalRdmaRmaBufferImpl>(netDevCtx, remotebuf, bufSize, RmaMemType::DEVICE);
    LocalRdmaRmaBufferImpl* remoteufferPtr = remotebuffer.get();
    remoteufferPtr->devAddr = remotebuf;
    remoteufferPtr->lkey = 23;
    std::string desc = remoteufferPtr->Serialize();

    TransportMem::RmaMemDesc remoteMemDesc;
    remoteMemDesc.localRankId = 1;
    remoteMemDesc.remoteRankId = 0;
    transport->RmaMemDescCopyFromStr(remoteMemDesc, desc);

    // 使能与去使能
    TransportMem::RmaMem remoteMem{};
    HcclResult ret = transport->EnableMemAccess(remoteMemDesc, remoteMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(remoteMem.type, RmaMemType::DEVICE);
    EXPECT_EQ(remoteMem.addr, remotebuf);
    EXPECT_EQ(remoteMem.size, bufSize);
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 同个远端内存两次enable
    ret = transport->EnableMemAccess(remoteMemDesc, remoteMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(remoteMem.type, RmaMemType::DEVICE);
    EXPECT_EQ(remoteMem.addr, remotebuf);
    EXPECT_EQ(remoteMem.size, bufSize);
    ret = transport->EnableMemAccess(remoteMemDesc, remoteMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 同个远端内存两次disable
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // disable未使能的远端内存
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    sal_free(remotebuf);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_roce_mem_read_write)
{
    HcclResult ret;
    rtStream_t stream;
    rtError_t rt_ret = aclrtCreateStream(&stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    devContext.localRdmaRmaBufferMgr_ = std::make_shared<NetDevContext::LocalRdmaRmaBufferMgr>();
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    std::shared_ptr<TransportMem> transport = TransportMem::Create(TransportMem::TpType::ROCE, notifyPool, netDevCtx, dispatcher, attrInfo);
    TransportRoceMem *transportPtr = dynamic_cast<TransportRoceMem *>(transport.get());
    HcclIpAddress ipAddr;
    std::shared_ptr<HcclSocket> socketPtr = make_shared<HcclSocket>("tagRoce", netDevCtx, ipAddr, 16666, HcclSocketRole::SOCKET_ROLE_SERVER);
    transport->SetDataSocket(socketPtr);

    u64 count = 1024;
    u64 bufSize = 1024 * sizeof(s8);
    s8* localbuf= (s8*)sal_malloc(bufSize);
    sal_memset(localbuf, bufSize, 0, bufSize);
    BufferKey<uintptr_t, u64> tempLocalKey(reinterpret_cast<uintptr_t>(localbuf), bufSize);
    std::shared_ptr<LocalRdmaRmaBuffer> tempLocalBufferPtr = make_shared<LocalRdmaRmaBuffer>(netDevCtx, localbuf, bufSize);
    tempLocalBufferPtr->devAddr = localbuf;
    devContext.localRdmaRmaBufferMgr_->Add(tempLocalKey, tempLocalBufferPtr);

    s8* remotebuf= (s8*)sal_malloc(bufSize);
    sal_memset(remotebuf, bufSize, 0, bufSize);
    BufferKey<uintptr_t, u64> tempRemoteKey(reinterpret_cast<uintptr_t>(remotebuf), bufSize);
    std::shared_ptr<RemoteRdmaRmaBuffer> tempRemoteBufferPtr = make_shared<RemoteRdmaRmaBuffer>();
    tempRemoteBufferPtr->addr = remotebuf;
    tempRemoteBufferPtr->size = bufSize;
    tempRemoteBufferPtr->devAddr = remotebuf;
    tempRemoteBufferPtr->memType = RmaMemType::HOST;
    transportPtr->remoteRdmaRmaBufferMgr_.Add(tempRemoteKey, tempRemoteBufferPtr);

    MOCKER(HrtRaSendWrV2)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DispatcherPub::RdmaSend, HcclResult(DispatcherPub::*)(u32, u64, const struct SendWr&, HcclRtStream, hccl::RdmaType, u64, u64, bool))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    // Read/Write未注册的内存
    HcclBuf localMem = {localbuf - 1, 1, nullptr};
    HcclBuf remoteMem = {remotebuf, bufSize, tempRemoteBufferPtr.get()};
    ret = transport->Read(localMem, remoteMem, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = transport->Write(remoteMem, localMem, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    TransportMem::RmaOpMem localMemop = {localbuf - 1, 1};
    TransportMem::RmaOpMem remoteMemop = {remotebuf, bufSize};
    ret = transport->Read(localMemop, remoteMemop, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = transport->Write(remoteMemop, localMemop, stream);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    // Read与Write已注册使能的内存中段
    HcclBuf localMem1 = {localbuf + 1, 4, nullptr};
    HcclBuf remoteMem1 = {remotebuf, 4, tempRemoteBufferPtr.get()};
    ret = transport->Read(localMem1, remoteMem1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->Write(remoteMem1, localMem1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclBuf localMem2 = {localbuf, bufSize, nullptr};
    HcclBuf remoteMem2 = {remotebuf, bufSize, tempRemoteBufferPtr.get()};
    ret = transport->Read(localMem2, remoteMem2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->Write(remoteMem2, localMem2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TransportMem::RmaOpMem localMemop1 = {localbuf + 1, 4};
    TransportMem::RmaOpMem remoteMemop1 = {remotebuf, 4};
    ret = transport->Read(localMemop1, remoteMemop1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->Write(remoteMemop1, localMemop1, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportMem::RmaOpMem localMemop2 = {localbuf, bufSize};
    TransportMem::RmaOpMem remoteMemop2 = {remotebuf, bufSize};
    ret = transport->Read(localMemop2, remoteMemop2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = transport->Write(remoteMemop2, localMemop2, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rt_ret = aclrtSynchronizeStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    rt_ret = aclrtDestroyStream(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    sal_free(localbuf);
    sal_free(remotebuf);
}

static std::shared_ptr<TransportRoceMem> CreateRoceMemTransport()
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    return std::make_unique<TransportRoceMem>(notifyPool, netDevCtx, dispatcher, attrInfo, true);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_roce_mem_qp_info_get)
{
    HcclResult ret;
    HcclQpInfoV2 qpInfo;
    std::shared_ptr<TransportRoceMem> transport = CreateRoceMemTransport();
    ret = transport->GetQpInfo(qpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

static std::shared_ptr<TransportDeviceRoceMem> CreateDeviceRoceMemTransport()
{
    std::unique_ptr<NotifyPool> notifyPool = std::make_unique<NotifyPool>();
    NetDevContext devContext;
    HcclNetDevCtx netDevCtx = &devContext;
    DispatcherPub dispatcherPub(0);
    HcclDispatcher dispatcher = &dispatcherPub;
    TransportMem::AttrInfo attrInfo;
    attrInfo.localRankId = 0;
    attrInfo.remoteRankId = 1;
    attrInfo.sdid = 1;
    attrInfo.serverId = 1;
    HcclQpInfoV2 qpInfo{};
    return std::make_unique<TransportDeviceRoceMem>(notifyPool, netDevCtx, dispatcher, attrInfo, false, qpInfo);
}

TEST_F(ST_MPI_TRANSPORT_MEM_TEST, ut_transport_device_roce_mem_qp_info_get)
{
    HcclResult ret;
    rtStream_t stream = nullptr;
    HcclQpInfoV2 qpInfo;
    std::shared_ptr<TransportDeviceRoceMem> transport = CreateDeviceRoceMemTransport();

    TransportMem::RmaMemDescs localMemDescs{};
    TransportMem::RmaMemDescs remoteMemDescs{};
    u32 actualNumOfRemote = 0;
    ret = transport->ExchangeMemDesc(localMemDescs, remoteMemDescs, actualNumOfRemote);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    TransportMem::RmaMemDesc remoteMemDesc{};
    TransportMem::RmaMem remoteMem{};
    ret = transport->EnableMemAccess(remoteMemDesc, remoteMem);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    ret = transport->DisableMemAccess(remoteMemDesc);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    std::shared_ptr<HcclSocket> socket = nullptr;
    ret = transport->SetSocket(socket);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = transport->Connect(0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    HcclBuf hcclRemoteMem{};
    HcclBuf hcclLocalMem{};
    ret = transport->Write(hcclRemoteMem, hcclLocalMem, stream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    ret = transport->Read(hcclLocalMem, hcclRemoteMem, stream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    TransportMem::RmaOpMem opLocalMem{};
    TransportMem::RmaOpMem opRemoteMem{};
    ret = transport->Write(opRemoteMem, opLocalMem, stream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    ret = transport->Read(opLocalMem, opRemoteMem, stream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = transport->AddOpFence(stream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = transport->GetTransInfo(qpInfo, nullptr, nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    ret = transport->WaitOpFence(stream);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}
