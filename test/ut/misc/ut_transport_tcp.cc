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
#include "dlra_function.h"
#include <sys/epoll.h>

#define private public
#define protected public
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
#include "tcp_recv_task.h"
#include "hccl_communicator.h"
#include "transport_heterog.h"
#include "transport_heterog_event_tcp_pub.h"
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
#include "dlra_function.h"
#include "adapter_hccp.h"
#include "externalinput.h"
using namespace std;
using namespace hccl;

class MPI_TRANSPORT_TCP_TEST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_TRANSPORT_TCP_TEST SetUP" << std::endl;
        pMsgInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclMessageInfo>(100));
        if (pMsgInfosMem == nullptr) return;
        pMsgInfosMem->Init();

        pReqInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclRequestInfo>(100));
        if (pReqInfosMem == nullptr) return;
        pReqInfosMem->Init();
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_TRANSPORT_TCP_TEST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
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
    static std::unique_ptr<LocklessRingMemoryAllocate<HcclMessageInfo>> pMsgInfosMem;
    static std::unique_ptr<LocklessRingMemoryAllocate<HcclRequestInfo>> pReqInfosMem;
    TransportResourceInfo transportResourceInfo = TransportResourceInfo(nullptr, pMsgInfosMem, pReqInfosMem,
        nullptr, nullptr);
};
std::unique_ptr<LocklessRingMemoryAllocate<HcclMessageInfo>> MPI_TRANSPORT_TCP_TEST::pMsgInfosMem = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<HcclRequestInfo>> MPI_TRANSPORT_TCP_TEST::pReqInfosMem = nullptr;

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_EschedAckCallback)
{
    unsigned int devId = 0;
    unsigned int subeventId = 0;
    u8 *msg = nullptr;
    unsigned int msgLen = 0;
    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    TransportHeterogEventTcp::EschedAckCallbackRecvRequest(devId, subeventId, msg, msgLen);
    TransportHeterogEventTcp::EschedAckCallbackSendCompletion(devId, subeventId, msg, msgLen);
    TransportHeterogEventTcp::EschedAckCallbackRecvCompletion(devId, subeventId, msg, msgLen);
}

int stub_LoopStateProcess(TransportHeterogEventTcp* transportHeterogTcp)
{
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_Init)
{
    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::SocketClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventTcp::GetNetworkResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventTcp::RegisterEschedAckCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventTcp::ConnectAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventTcp::DelEpollEvents)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclIpAddress invalidIp;
    TransportHeterogEventTcp transportHetTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo);
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    HcclResult ret = transportHeterogTcp->Init();
    ret = HCCL_SUCCESS;
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&TransportHeterog::CheckRecvMsgAndRequestBuffer)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventTcp::GetNetworkResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventTcp::RegisterEschedAckCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    // MOCKER_CPP(&TransportHeterog::InitTransportConnect)
    // .stubs()
    // .with(any())
    // .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transportHetTcp, &TransportHeterogEventTcp::LoopStateProcess)
    .stubs()
    .with(any())
    .will(invoke(stub_LoopStateProcess));

    SocketInfoT socket;
    strcpy(socket.tag, "tag");
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socket);
    transportHeterogTcp->needRepoEvent_ = true;
    ret = transportHeterogTcp->Init();
    ret = HCCL_SUCCESS;
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_RegisterEschedAckCallback)
{
    MOCKER_CPP(&DlHalFunction::DlHalFunctionInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtHalEschedRegisterAckFunc)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    HcclResult ret = transportHeterogTcp->RegisterEschedAckCallback();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_ReportSendComp)
{
    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclRequestInfo request;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    HcclResult ret = transportHeterogTcp->ReportSendComp(&request);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHeterogTcp->needRepoEvent_ = false;
    ret = transportHeterogTcp->ReportSendComp(&request);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHeterogTcp->needRepoEvent_ = true;
    transportHeterogTcp->devId_ = 0;
    transportHeterogTcp->gCompCounterEvent[transportHeterogTcp->devId_][HCCL_EVENT_SEND_COMPLETION_MSG].counter++;
    ret = transportHeterogTcp->ReportSendComp(&request);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_ReportEnvelpComp)
{
    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclEnvelopeSummary envelopeSummary;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    HcclResult ret = transportHeterogTcp->ReportEnvelpComp(envelopeSummary);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHeterogTcp->needRepoEvent_ = false;
    ret = transportHeterogTcp->ReportEnvelpComp(envelopeSummary);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHeterogTcp->needRepoEvent_ = true;
    transportHeterogTcp->devId_ = 0;
    transportHeterogTcp->gCompCounterEvent[transportHeterogTcp->devId_][HCCL_EVENT_RECV_REQUEST_MSG].counter++;
    ret = transportHeterogTcp->ReportEnvelpComp(envelopeSummary);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_ReportRecvComp)
{
    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclRequestInfo request;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    HcclResult ret = transportHeterogTcp->ReportRecvComp(&request);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHeterogTcp->needRepoEvent_ = false;
    ret = transportHeterogTcp->ReportRecvComp(&request);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHeterogTcp->needRepoEvent_ = true;
    transportHeterogTcp->devId_ = 0;
    transportHeterogTcp->gCompCounterEvent[transportHeterogTcp->devId_][HCCL_EVENT_RECV_COMPLETION_MSG].counter++;
    ret = transportHeterogTcp->ReportRecvComp(&request);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_EnterStateProcess)
{
    SocketInfoT socketInfo;
    ConnState nextState = ConnState::CONN_STATE_CONNECT_CHECK_SOCKET;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    HcclResult ret = transportHeterogTcp->EnterStateProcess(nextState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nextState = ConnState::CONN_STATE_GET_CHECK_SOCKET;
    ret = transportHeterogTcp->EnterStateProcess(nextState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nextState = ConnState::CONN_STATE_SEND_CF;
    ret = transportHeterogTcp->EnterStateProcess(nextState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nextState = ConnState::CONN_STATE_RECV_CF;
    ret = transportHeterogTcp->EnterStateProcess(nextState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&TransportHeterog::CheckConsistentFrame)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterog::TryTransition)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    nextState = ConnState::CONN_STATE_CHECK_CF;
    ret = transportHeterogTcp->EnterStateProcess(nextState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nextState = ConnState::CONN_STATE_COMPLETE;
    ret = transportHeterogTcp->EnterStateProcess(nextState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    nextState = ConnState::CONN_STATE_GET_QP;
    ret = transportHeterogTcp->EnterStateProcess(nextState);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_LoopStateProcess)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    transportHeterogTcp->connState_ = ConnState::CONN_STATE_CONNECT_CHECK_SOCKET;
    SocketInfoT socketInfo;
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterog::ConnectSocket)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = transportHeterogTcp->LoopStateProcess();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&TransportHeterog::GetSocket)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    transportHeterogTcp->connState_ = ConnState::CONN_STATE_GET_CHECK_SOCKET;
    ret = transportHeterogTcp->LoopStateProcess();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&TransportHeterog::SocketSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    transportHeterogTcp->connState_ = ConnState::CONN_STATE_SEND_CF;
    ret = transportHeterogTcp->LoopStateProcess();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.clear();
    ret = transportHeterogTcp->LoopStateProcess();
    EXPECT_EQ(ret, HCCL_E_PARA);

    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    MOCKER_CPP(&TransportHeterog::SocketRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    transportHeterogTcp->connState_ = ConnState::CONN_STATE_RECV_CF;
    ret = transportHeterogTcp->LoopStateProcess();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.clear();
    ret = transportHeterogTcp->LoopStateProcess();
    EXPECT_EQ(ret, HCCL_E_PARA);

    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    transportHeterogTcp->connState_ = ConnState::CONN_STATE_CHECK_CF;
    ret = transportHeterogTcp->LoopStateProcess();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

HcclResult stub_complete_hrtRaSocketNonBlockSend(const FdHandle fdHandle, const void *data, u64 size, u64 *sent_size)
{
    *sent_size = size;
    return HCCL_SUCCESS;
}
HcclResult stub_TCPMode_hrtRaSocketNonBlockRecv(const FdHandle fdHandle, void *data, u64 size, u64 *recvSize)
{
    *recvSize = size;
    return HCCL_SUCCESS;
}

HcclResult stub_TCPMode_hrtRaSocketNonBlockSendComplete(const FdHandle fdHandle, const void *data, u64 size, u64 *sentSize)
{
    *sentSize = size;
    return HCCL_SUCCESS;
}

HcclResult stub_TCPMode_hrtRaSocketNonBlockSendNoComplete(const FdHandle fdHandle, const void *data, u64 size, u64 *sentSize)
{
    *sentSize = 0;
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_SendNoBlock)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));

    SocketInfoT socketInfo;
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    TransData sendData;
    TransportEndPointParam epParam;
    u64 srcBuf = 0;
    sendData.dataType = HCCL_DATA_TYPE_FP32;
    sendData.srcBuf = reinterpret_cast<u64>(&srcBuf);
    sendData.count = 10;
    u64 envoffset = 0;
    u64 dataTranoffset = 0;
    bool envCompleted = true;
    bool tranCompleted = true;

    MOCKER(hrtRaSocketNonBlockSendHeterog)
    .stubs()
    .will(invoke(stub_TCPMode_hrtRaSocketNonBlockSendComplete));
    HcclResult ret = transportHeterogTcp->SendNoBlock(sendData, epParam, envoffset, dataTranoffset, envCompleted, tranCompleted);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(envCompleted, true);
    EXPECT_EQ(tranCompleted, true);
    GlobalMockObject::verify();

    MOCKER(hrtRaSocketNonBlockSendHeterog)
    .stubs()
    .will(invoke(stub_TCPMode_hrtRaSocketNonBlockSendNoComplete));
    envCompleted = true;
    tranCompleted = true;
    envoffset = 0;
    dataTranoffset = 0;
    ret = transportHeterogTcp->SendNoBlock(sendData, epParam, envoffset, dataTranoffset, envCompleted, tranCompleted);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(envCompleted, false);
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.clear();
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_epoll_adapters)
{
    int event_handle;
    FdHandle fdHandle;
    int opcode;
    HcclEpollEvent event;
    int timeout;
    unsigned int maxevents;
    unsigned int events_num;
    std::vector<SocketEventInfo> eventInfos;

    HcclResult ret = hrtRaCreateEventHandle(event_handle);
    ret = hrtRaCtlEventHandle(event_handle, fdHandle, opcode, event);
    ret = hrtRaDestroyEventHandle(event_handle);
    ret = hrtRaWaitEventHandle(event_handle, eventInfos, 100, 100, events_num);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

int stub_Isend(TransportHeterogEventTcp* transportHeterogTcp, const TransData &sendData, const TransportEndPointParam &epParam,
    HcclRequestInfo *&request)
{
    static HcclRequestInfo hcclRequest;
    request = &hcclRequest;
    return HCCL_SUCCESS;
}

int stub_Test(TransportHeterogEventTcp* transportHeterogTcp, HcclRequestInfo &request, s32 &flag, HcclStatus &compState)
{
    flag = 1;
    compState.error = 0;
    return HCCL_SUCCESS;
}

int stub_Improbe(TransportHeterogEventTcp* transportHeterogTcp, const TransportEndPointParam &epParam, s32 &matched, HcclMessageInfo *&msg,
    HcclStatus &status)
{
    static HcclMessageInfo message;
    msg = &message;
    matched = 1;
    return HCCL_SUCCESS;
}

int stub_Imrecv(TransportHeterogEventTcp* transportHeterogTcp, const TransData &recvData, HcclMessageInfo &msg, HcclRequestInfo *&request)
{
    static HcclRequestInfo hcclRequest;
    request = &hcclRequest;
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_Connect)
{
    HcclIpAddress invalidIp;
    TransportHeterogEventTcp transportHeterogTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo);

    MOCKER_CPP_VIRTUAL(transportHeterogTcp, &TransportHeterogEventTcp::Isend)
    .stubs()
    .with(any())
    .will(invoke(stub_Isend));

    MOCKER_CPP_VIRTUAL(transportHeterogTcp, &TransportHeterogEventTcp::Test)
    .stubs()
    .with(any())
    .will(invoke(stub_Test));

    HcclMessageInfo msg;
    MOCKER_CPP_VIRTUAL(transportHeterogTcp, &TransportHeterogEventTcp::Improbe)
    .stubs()
    .with(any())
    .will(invoke(stub_Improbe));

    MOCKER_CPP_VIRTUAL(transportHeterogTcp, &TransportHeterogEventTcp::Imrecv)
    .stubs()
    .with(any())
    .will(invoke(stub_Imrecv));

    u32 localUserRank = 0;
    u32 remoteUserRank = 1;
    HcclResult ret = transportHeterogTcp.Connect(localUserRank, remoteUserRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    localUserRank = 1;
    remoteUserRank = 0;
    ret = transportHeterogTcp.Connect(localUserRank, remoteUserRank);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_infer_DelEpollEvents)
{
    MOCKER(hrtRaCtlEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    s32 LINK_NUM = 1;
    SocketInfoT socketInfo;
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);

    HcclResult ret = transportHeterogTcp->DelEpollEvents();
    ret = HCCL_SUCCESS;
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_infer_AddEpollEvents)
{
    MOCKER(hrtRaCtlEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    s32 LINK_NUM = 1;
    SocketInfoT socketInfo;
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);

    HcclResult ret = transportHeterogTcp->AddEpollEvents();
    ret = HCCL_SUCCESS;
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_infer_CreateEventHandle)
{
    MOCKER(hrtRaCreateEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    SocketInfoT socketInfo;
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);

    transportHeterogTcp->sendEpollEventInfo_.epollEventFd = -1;
    transportHeterogTcp->gRecvEpollEventInfo.epollEventFd = -1;

    HcclResult ret = transportHeterogTcp->CreateEventHandle();
    ret = HCCL_SUCCESS;
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER(hrtRaCreateEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TCP_TRANSFER))
    .then(returnValue(HCCL_SUCCESS))
    .then(returnValue(HCCL_E_TCP_TRANSFER));
    ret = transportHeterogTcp->CreateEventHandle();
    ret = transportHeterogTcp->CreateEventHandle();
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_infer_AddFdMapping)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    socket_peer_info spInfo;
    SocketInfoT socketInfo;
    socketInfo.fdHandle = reinterpret_cast<void *>(&spInfo);
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_infer_DestroyEventHandle)
{
    MOCKER(hrtRaDestroyEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    SocketInfoT socketInfo;
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);

    transportHeterogTcp->sendEpollEventInfo_.epollEventFd = 10;
    transportHeterogTcp->gRecvEpollEventInfo.epollEventFd = 10;

    transportHeterogTcp->DestroyEventHandle();
    GlobalMockObject::verify();

    MOCKER(hrtRaDestroyEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TCP_TRANSFER))
    .then(returnValue(HCCL_SUCCESS))
    .then(returnValue(HCCL_E_TCP_TRANSFER));
    transportHeterogTcp->sendEpollEventInfo_.epollEventFd = 10;
    transportHeterogTcp->gRecvEpollEventInfo.epollEventFd = 10;
    transportHeterogTcp->DestroyEventHandle();
    transportHeterogTcp->sendEpollEventInfo_.epollEventFd = 10;
    transportHeterogTcp->gRecvEpollEventInfo.epollEventFd = 10;
    transportHeterogTcp->DestroyEventHandle();
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_infer_WaitEvents)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    hccl::TransportHeterogEventTcp::EpollEventInfo epollEventInfo;
    vector<SocketEventInfo> eventInfos;
    hccl::TransportHeterogEventTcp::EventStatus eventStatus;
    eventStatus.event = EPOLLOUT;
    FdHandle fdHandle = (void*)0x12;
    s32 timeout = 10;
    MOCKER(hrtRaWaitEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = transportHeterogTcp->WaitEvents(epollEventInfo, eventInfos, eventStatus, fdHandle, timeout);
    EXPECT_EQ(ret, HCCL_E_AGAIN);
    GlobalMockObject::verify();

    u32 stub = 1;
    MOCKER(hrtRaWaitEventHandle)
    .stubs()
    .with(any(), any(), any(), any(), outBound(stub))
    .will(returnValue(HCCL_SUCCESS));
    SocketEventInfo seInfo{};
    seInfo.fdHandle = fdHandle;
    seInfo.event = EPOLLOUT;
    eventInfos.push_back(seInfo);

    ret = transportHeterogTcp->WaitEvents(epollEventInfo, eventInfos, eventStatus, fdHandle, timeout);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

HcclRequestInfo* stub_HcclRequestInfo_Alloc_tcp()
{
    static HcclRequestInfo requestInfo;
    return &requestInfo;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_BlockSend)
{
    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocklessRingMemoryAllocate<HcclRequestInfo>::Alloc)
    .stubs()
    .with(any())
    .will(invoke(stub_HcclRequestInfo_Alloc_tcp));
    bool envCompleted = true;
    bool tranCompleted = true;
    MOCKER_CPP(&TransportHeterogEventTcp::SendNoBlock)
    .stubs()
    .with(any(), any(), any(), any(), outBound(envCompleted), outBound(tranCompleted))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogEventTcp::WaitEvents)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    TransData sendData;
    TransportEndPointParam epParam;
    u64 srcBuf = 0;
    sendData.dataType = HCCL_DATA_TYPE_FP32;
    sendData.srcBuf = reinterpret_cast<u64>(&srcBuf);
    sendData.count = 10;
    HcclRequestInfo* request;
    s32 waitTimeOut = 10;
    SocketInfoT socketInfo;
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);

    HcclResult ret = transportHeterogTcp->BlockSend(sendData, epParam, request, waitTimeOut);
    ret = HCCL_SUCCESS;
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHeterogTcp->TransportHeterog::BlockSend(sendData, epParam, request, waitTimeOut);
}

s32 hrtRaSocketRecvStub(const FdHandle fdHandle, void *data, u64 size, u64 *recvSize)
{
    *recvSize = 10;
    return 0;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_NoBlockRecv)
{
    s32 res = 0;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with(any())
    .will(invoke(hrtRaSocketRecvStub));
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    FdHandle fdHandle = 0;
    void *recvBuffer = nullptr;
    u64 byteSize = 10;
    u64 recvSize = 0;
    HcclResult ret = transportHeterogTcp->NoBlockRecv(fdHandle, recvBuffer, byteSize, recvSize);
    if (ret == HCCL_SUCCESS) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    GlobalMockObject::verify();

    res = 0;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with(any())
    .will(returnValue(res));
    ret = transportHeterogTcp->NoBlockRecv(fdHandle, recvBuffer, byteSize, recvSize);
    if (ret == HCCL_E_TCP_TRANSFER) {
        EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);
    }
    GlobalMockObject::verify();

    byteSize = 5;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with()
    .will(returnValue(SOCK_EAGAIN));
    ret = transportHeterogTcp->NoBlockRecv(fdHandle, recvBuffer, byteSize, recvSize);
    if (ret == HCCL_SUCCESS) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    GlobalMockObject::verify();

    res = -1;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with(any())
    .will(returnValue(res));
    ret = transportHeterogTcp->NoBlockRecv(fdHandle, recvBuffer, byteSize, recvSize);
    if (ret == HCCL_E_TCP_TRANSFER) {
        EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);
    }
    GlobalMockObject::verify();

}

HcclResult hrtRaWaitEventHandleStub(s32 eventHandle, std::vector<SocketEventInfo> &eventInfos, s32 timeOut,
    u32 maxEvents, u32 &eventsNum)
{
    eventsNum = 1;

    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_WaitEvents)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    hccl::TransportHeterogEventTcp::EpollEventInfo epollEventInfo;
    vector<SocketEventInfo> eventInfos;
    hccl::TransportHeterogEventTcp::EventStatus eventStatus;
    eventStatus.matched = true;
    eventStatus.event = 1;
    socket_peer_info socketInfo;
    FdHandle fdHandle = reinterpret_cast<FdHandle>(&socketInfo);
    s32 timeout = 10;
    MOCKER(hrtRaWaitEventHandle)
    .stubs()
    .with(any())
    .will(invoke(hrtRaWaitEventHandleStub));

    SocketEventInfo seInfo{};
    seInfo.fdHandle  = fdHandle;
    seInfo.event = 1;
    eventInfos.push_back(seInfo);

    HcclResult ret = transportHeterogTcp->WaitEvents(epollEventInfo, eventInfos, eventStatus, fdHandle, timeout);
    if (ret == HCCL_SUCCESS) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    eventStatus.event = 0;
    eventInfos[0].event = static_cast<u32>(EPOLLRDHUP);
    transportHeterogTcp->WaitEvents(epollEventInfo, eventInfos, eventStatus, fdHandle, timeout);

    GlobalMockObject::verify();

    MOCKER(hrtRaWaitEventHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_NOT_SUPPORT));
    ret = transportHeterogTcp->WaitEvents(epollEventInfo, eventInfos, eventStatus, fdHandle, timeout);
    if (ret == HCCL_SUCCESS) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}

HcclResult hrtRaSocketBlockRecvFalse(const FdHandle fdHandle, void *data, u64 size)
{
    reinterpret_cast<HcclEnvelope *>(data)->transData.count = -1;
    reinterpret_cast<HcclEnvelope *>(data)->transData.dataType = 0;

    return HCCL_SUCCESS;
}

HcclResult hrtRaSocketBlockRecvStub(const FdHandle fdHandle, void *data, u64 size)
{
    reinterpret_cast<HcclEnvelope *>(data)->transData.count = 1;
    reinterpret_cast<HcclEnvelope *>(data)->transData.dataType = 0;

    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_BlockRecv)
{
    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    TransData recvData;
    u64 srcBuf = 0;
    recvData.dataType = HCCL_DATA_TYPE_FP32;
    recvData.srcBuf = reinterpret_cast<u64>(&srcBuf);
    recvData.count = 0;
    bool matched = true;
    socket_peer_info spInfo;
    SocketInfoT socketInfo;
    socketInfo.fdHandle = reinterpret_cast<void *>(&spInfo);
    transportHeterogTcp->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    s32 waitTimeOut = 10;
    s32 waitPayloadTimeOut = 10000;
    FdHandle fdHandle{};

    hrtRaSocketRecv(fdHandle, nullptr, 0, nullptr);

    TransportHeterog *transport{};

    MOCKER_CPP(&TransportHeterogEventTcp::WaitEvents)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));
    transportHeterogTcp->BlockRecv(recvData, matched, transport, waitTimeOut, waitPayloadTimeOut);
    GlobalMockObject::verify();

    MOCKER_CPP(&TransportHeterogEventTcp::WaitEvents)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_AGAIN));
    transportHeterogTcp->BlockRecv(recvData, matched, transport, waitTimeOut, waitPayloadTimeOut);
    GlobalMockObject::verify();

    MOCKER_CPP(&TransportHeterogEventTcp::WaitEvents)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    transportHeterogTcp->BlockRecv(recvData, false, transport, waitTimeOut, waitPayloadTimeOut);

    transportHeterogTcp->BlockRecv(recvData, true, transport, waitTimeOut, waitPayloadTimeOut);
    GlobalMockObject::verify();

    MOCKER_CPP(&TransportHeterogEventTcp::NoBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TCP_TRANSFER));
    MOCKER_CPP(&TransportHeterogEventTcp::WaitEvents)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(invoke(hrtRaSocketBlockRecvStub));

    transportHeterogTcp->BlockRecv(recvData, true, transport, waitTimeOut, waitPayloadTimeOut);
    GlobalMockObject::verify();

    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogEventTcp::NoBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_TCP_TRANSFER));
    MOCKER_CPP(&TransportHeterogEventTcp::WaitEvents)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(invoke(hrtRaSocketBlockRecvFalse));
    HcclResult ret = transportHeterogTcp->BlockRecv(recvData, true, transport, waitTimeOut, waitPayloadTimeOut);
    EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);

    transportHeterogTcp->TransportHeterog::BlockRecv(recvData, true, transport, waitTimeOut, waitPayloadTimeOut);

    MOCKER(hrtGetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterog::SetDeviceIndex)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterog::CheckRecvMsgAndRequestBuffer)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogEventTcp::GetNetworkResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogEventTcp::RegisterEschedAckCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    // MOCKER_CPP(&TransportHeterog::InitTransportConnect)
    // .stubs()
    // .with(any())
    // .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterog::ConnectAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogEventTcp::Connect)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    transportHeterogTcp->Init(0, 0);

}

HcclResult stub_complete_Error_hrtRaSocketNonBlockSend(const FdHandle fdHandle, const void *data, u64 size, u64 *sent_size)
{
    *sent_size = size;
    return HCCL_E_PARA;
}

HcclResult stub_complete_Error1_hrtRaSocketNonBlockSend(const FdHandle fdHandle, const void *data, u64 size, u64 *sent_size)
{
    *sent_size = size + 1;
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_SocketSend)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    bool completed = false;
    u64 sentSize;
    u64 size = 0;
    void *data;
    FdHandle fdHandle;
    MOCKER(hrtRaSocketNonBlockSendHeterog)
    .stubs()
    .will(invoke(stub_complete_hrtRaSocketNonBlockSend));
    HcclResult ret = transportHeterogTcp->SocketSend(fdHandle, data, size, sentSize, completed);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER(hrtRaSocketNonBlockSendHeterog)
    .stubs()
    .will(invoke(stub_complete_Error_hrtRaSocketNonBlockSend));
    ret = transportHeterogTcp->SocketSend(fdHandle, data, size, sentSize, completed);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();

    MOCKER(hrtRaSocketNonBlockSendHeterog)
    .stubs()
    .will(invoke(stub_complete_Error1_hrtRaSocketNonBlockSend));
    ret = transportHeterogTcp->SocketSend(fdHandle, data, size, sentSize, completed);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}

HcclResult stub_Error_TCPMode_hrtRaSocketNonBlockRecv(const FdHandle fdHandle, void *data, u64 size, u64 *recvSize)
{
    *recvSize = size;
    return HCCL_E_PARA;
}

HcclResult stub_Error1_TCPMode_hrtRaSocketNonBlockRecv(const FdHandle fdHandle, void *data, u64 size, u64 *recvSize)
{
    *recvSize = size + 1;
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_TCP_TEST, ut_transportTcp_SocketRecv)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventTcp> transportHeterogTcp(new (nothrow) TransportHeterogEventTcp("test_collective", invalidIp, invalidIp, 18000, 0, 0, transportResourceInfo));
    bool completed = false;
    u64 sentSize;
    u64 size = 0;
    void *data;
    FdHandle fdHandle;
    MOCKER(hrtRaSocketNonBlockRecvHeterog)
    .stubs()
    .will(invoke(stub_TCPMode_hrtRaSocketNonBlockRecv));
    HcclResult ret = transportHeterogTcp->SocketRecv(fdHandle, data, size, sentSize, completed);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER(hrtRaSocketNonBlockRecvHeterog)
    .stubs()
    .will(invoke(stub_Error_TCPMode_hrtRaSocketNonBlockRecv));
    ret = transportHeterogTcp->SocketRecv(fdHandle, data, size, sentSize, completed);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();

    MOCKER(hrtRaSocketNonBlockRecvHeterog)
    .stubs()
    .will(invoke(stub_Error1_TCPMode_hrtRaSocketNonBlockRecv));
    ret = transportHeterogTcp->SocketRecv(fdHandle, data, size, sentSize, completed);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}