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
#include "rt_external.h"
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

#include <hccl/hccl.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h" //delete here?
#include <iostream>
#include <fstream>

#define private public
#define protected public
#include "dlra_function.h"
#include "mr_manager.h"
#include "hccl_comm_conn.h"
#include "hccl_comm_conn_mgr.h"
#include "transport_heterog_raw_roce.h"
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "dltdt_function.h"
#include "hccl_types.h"
#include "transport_heterog_roce.h"
#include "externalinput.h"
#include "env_config.h"
#undef protected
#undef private

using namespace std;
using namespace hccl;


std::unique_ptr<LocklessRingMemoryAllocate<HcclMessageInfo>> pMsgInfosMem;
std::unique_ptr<LocklessRingMemoryAllocate<HcclRequestInfo>> pReqInfosMem;
std::unique_ptr<HeterogMemBlocksManager> memBlocksManager;
std::unique_ptr<LocklessRingMemoryAllocate<RecvWrInfo>> pRecvWrInfosMem;
TransportResourceInfo transportResourceInfoBackUp(nullptr,
    pMsgInfosMem, pReqInfosMem, memBlocksManager, pRecvWrInfosMem);

class MPI_RAW_SENDRECV_TEST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        hrtSetDevice(0);
        // ResetInitState();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        ClearHalEvent();
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {

        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};

HcclConn g_connPtr = nullptr;

// pass
TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclRawOpen_test)
{
    HcclResult ret = HcclRawOpen(&g_connPtr);
    if (g_connPtr != nullptr) {
        HCCL_ERROR("%p success", g_connPtr);
    }
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

// pass
TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclRawBind_test)
{
    HcclAddr bindAddr;
    bindAddr.type=HCCL_ADDR_TYPE_ROCE;
    bindAddr.info.tcp.ipv4Addr = 289669838;
    bindAddr.info.tcp.port = 60999;

    HcclResult ret = HcclRawBind(g_connPtr, &bindAddr);
    HCCL_ERROR("%p success", g_connPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

// pass
TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclRawGetCount_test)
{
    HcclStatus status;
    int count;

    status.error=0;
    status.count=1;

    HcclResult ret = HcclRawGetCount(&status, HCCL_DATA_TYPE_UINT64, &count);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

// pass
TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclRegisterGlobalMemory_test)
{

    int count=1;
    int*buffer=(int*)malloc(count*sizeof(int));
    HcclResult ret = HcclRegisterGlobalMemory(buffer,count * sizeof(int));
    HCCL_ERROR("ret = %d", ret);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret=HcclUnregisterGlobalMemory(buffer);
    HCCL_ERROR("ret = %d", ret);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    free(buffer);

    buffer=nullptr;

    GlobalMockObject::verify();
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclCommConn_InitMemBlocksAndRecvWrMem_test)
{
    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclCommConn conn;
    HcclResult ret = conn.InitMemBlocksAndRecvWrMem();
    GlobalMockObject::verify();
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_accept_error_test)
{
    MOCKER_CPP(&HcclCommConn::GetSocket)
    .stubs()
    .will(returnValue(HCCL_E_TCP_CONNECT));

    HcclAddr acceptAddr;
    HcclCommConn conn;
    HcclResult ret = HCCL_SUCCESS;
    HcclCommConn *connP = &conn;
    HcclCommConn **newConn = &connP;
    ret = conn.Accept(acceptAddr, *newConn);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_InitTransport_error_test)
{
    HcclCommConn::AcceptCommConn acceptComConn;

    socket_info_t tmp;
    tmp.status = CONNECT_OK;
    tmp.fd_handle = nullptr;
    MOCKER_CPP(&HcclCommConn::GetSocket)
    .stubs()
    .with(outBound(tmp))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommConn::InitTransport)
    .stubs()
    .will(returnValue(HCCL_E_TCP_CONNECT));

    HcclAddr acceptAddr;
    HcclCommConn conn;
    acceptComConn.newCommConn = &conn;

    HcclResult ret = HCCL_SUCCESS;
    HcclCommConn *connP = &conn;
    HcclCommConn **newConn = &connP;
    ret = conn.Accept(acceptAddr, *newConn);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();
}

HcclResult stub_complete_hrtRaNonBlockGetSockets(u32 role, struct socket_info_t conn[], u32 num, u32 *connectedNum)
{
    static std::vector<int> fdHandle;
    for (int i = 0; i < num; i++) {
        fdHandle.push_back(0);
        conn[i].fd_handle = &fdHandle[fdHandle.size() - 1];
        conn[i].status = CONNECT_OK;
    }
    *connectedNum = 2;
    return HCCL_SUCCESS;
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_getsocket_error_test)
{
    MOCKER(hrtRaNonBlockGetSockets)
    .stubs()
    .will(invoke(stub_complete_hrtRaNonBlockGetSockets));

    HcclCommConn conn;
    socket_info_t socketInfo;
    HcclResult ret = HCCL_SUCCESS;
    ret = conn.GetSocket(socketInfo);
    EXPECT_EQ(ret, HCCL_E_TCP_CONNECT);
    GlobalMockObject::verify();
}

// pass
TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclRawClose_test)
{
    MOCKER_CPP(&TransportHeterog::SocketClose)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcclRawClose(g_connPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_UninitRa_test)
{
    MOCKER(HrtRaDeInit)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    HcclCommConnMgr mgr;
    mgr.raInited_ = true;
    HcclResult ret = mgr.UninitRa();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclCommConn_Bind_test)
{
    MOCKER(HrtRaRdmaInitRef).stubs().will(returnValue(HCCL_SUCCESS));
    HcclCommConn conn;
    SocketHandle socketHandle;
    HcclAddr bindAddr;
    bindAddr.type = HCCL_ADDR_TYPE_ROCE;
    bindAddr.info.tcp.ipv4Addr = 289669838;
    bindAddr.info.tcp.port = 60999;

    conn.socketHandle_ = socketHandle;
    HcclResult ret = conn.Bind(bindAddr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclCommConn_connect_failed)
{
    MOCKER(hrtRaSocketNonBlockBatchAbort).stubs().will(returnValue(HCCL_E_TCP_CONNECT));
    HcclCommConn *conn = new HcclCommConn();
    conn->role_ = CLIENT_ROLE_SOCKET;
    delete conn;
    GlobalMockObject::verify();

    socket_connect_info_t connInfos{};
    MOCKER(ra_socket_batch_abort).stubs()
        .will(returnValue(0))
        .then(returnValue(SOCK_EAGAIN))
        .then(returnValue(SOCK_ESOCKCLOSED));
    EXPECT_EQ(hrtRaSocketNonBlockBatchAbort(&connInfos, 1), HCCL_SUCCESS);
    EXPECT_EQ(hrtRaSocketNonBlockBatchAbort(&connInfos, 1), HCCL_E_AGAIN);
    EXPECT_EQ(hrtRaSocketNonBlockBatchAbort(&connInfos, 1), HCCL_E_TCP_CONNECT);
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_HcclCommConn_ResetConnection)
{
    HcclCommConn conn;
    HcclCommConn *newCommConn{ nullptr };
    HcclIpAddress invalidIp;

    MOCKER_CPP(&TransportHeterog::SetForceClose)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    newCommConn = new(nothrow) HcclCommConn();
    conn.transport_ = nullptr;
    HcclResult ret = conn.ResetCurrentErrorConnection(newCommConn);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

HcclResult stub_TCPMode_hrtRaSocketNonBlockSendComplete(const FdHandle fdHandle, const void *data, u64 size, u64 *sentSize)
{
    *sentSize = size;
    return HCCL_SUCCESS;
}

HcclResult stub_TCPMode_hrtRaSocketNonBlockRecv(const FdHandle fdHandle, void *data, u64 size, u64 *recvSize)
{
    *recvSize = size;
    return HCCL_SUCCESS;
}

#if 1
TEST_F(MPI_RAW_SENDRECV_TEST, st_pd_force_and_concurrency_link01)
{
    s32 nnode, noderank = 0;
    socket_info_t tmp;
    u64 buf = 0;
    HcclRequest request = nullptr;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);

    MOCKER_CPP(&MrManager::InitMrManager, HcclResult (MrManager::*)(void *))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtIbvPostRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaNormalQpCreate)
    .stubs()
    .with(any())
    .will(returnValue(0));

    tmp.status = CONNECT_OK;
    tmp.fd_handle = &tmp;
    tmp.socket_handle = &tmp;
    MOCKER_CPP(&HcclCommConn::GetSocket)
    .stubs()
    .with(outBound(tmp))
    .will(returnValue(HCCL_SUCCESS))
    .then(returnValue(HCCL_E_AGAIN));

    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaGetQpAttr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketNonBlockSendHeterog)
    .stubs()
    .will(invoke(stub_TCPMode_hrtRaSocketNonBlockSendComplete));
    
    MOCKER(hrtRaSocketNonBlockRecvHeterog)
    .stubs()
    .will(invoke(stub_TCPMode_hrtRaSocketNonBlockRecv));

    MOCKER(hrtRaGetQpAttr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaTypicalQpModify)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::FreeMemBlock)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::FreeRecvWrId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    UseRealPortAndName(true);
    if (noderank == 0) {
        HcclConn connPtr = nullptr;
        HcclConn acceptConn = nullptr;
        HcclAddr acceptAddr;
        int flag = 0;
        HcclStatus status;
        HcclMessageInfo msgInfo; 
        HcclMessage msg = &msgInfo;

        // open
        HcclResult ret = HcclRawOpen(&connPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawOpen success. connPtr[%p]", connPtr);

        // bind
        HcclAddr bindAddr;
        bindAddr.type= HCCL_ADDR_TYPE_ROCE;
        bindAddr.info.tcp.ipv4Addr = 3232300033;
        bindAddr.info.tcp.port = 70999;
        ret = HcclRawBind(connPtr, &bindAddr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawBind success. connPtr[%p]", connPtr);

        // listen
        int backLog = 0;
        ret = HcclRawListen(connPtr, backLog);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawListen success. connPtr[%p]", connPtr);

        auto startTime = chrono::steady_clock::now();
        auto timeout = chrono::seconds(10);
        while (acceptConn == nullptr) {
            ret = HcclRawAccept(connPtr, &acceptAddr, &acceptConn);
            if ((chrono::steady_clock::now() - startTime) > timeout) {
                HCCL_ERROR("HcclRawAccept timeout");
                break;
            }
            SalSleep(1);
        }
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawAccept success. connPtr[%p]", acceptConn);

        HcclEnvelope envelop;
        HcclIpAddress invalidIp;
        HcclIpAddress invalidIp1;
        struct ibv_wc wc[1];
        TransportHeterogRawRoce transport1("", invalidIp, invalidIp1, 0, 0, transportResourceInfoBackUp);
        RecvWrInfo data;
        data.buf = &envelop;
        data.transportHandle = reinterpret_cast<void *>(&transport1);
        wc[0].status = IBV_WC_SUCCESS;
        wc[0].wr_id = reinterpret_cast<uint64_t>(&data);
        wc[0].opcode = IBV_WC_SEND;
        s32 num = 1;

        MOCKER(hrtIbvPostSend)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS))
        .then(returnValue(HCCL_E_AGAIN))
        .then(returnValue(HCCL_E_NETWORK))
        .then(returnValue(HCCL_SUCCESS));

        MOCKER(hrtIbvPollCq)
        .stubs()
        .with(any(), any(), outBoundP(wc, sizeof(ibv_wc)), outBound(num))
        .will(returnValue(HCCL_SUCCESS));

        ret = HcclRawImprobe(acceptConn, &flag, &msg, &status);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawImprobe success. connPtr[%p]", acceptConn);
        msgInfo.commHandle = acceptConn;
        msgInfo.envelope.envelope.transData.dataType = HCCL_DATA_TYPE_UINT64;
        msg = &msgInfo;
        ret = HcclRawImrecv(&buf, 1, HCCL_DATA_TYPE_UINT64, &msg, &request);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawImrecv success. connPtr[%p]", acceptConn);
        msgInfo.commHandle = acceptConn;
        msgInfo.envelope.envelope.transData.count = 1;
        msgInfo.envelope.envelope.transData.dataType = HCCL_DATA_TYPE_UINT64;
        msg = &msgInfo;
        request = nullptr;
        void *buf1[1];
        int count1[1];
        buf1[0] = &msgInfo;
        count1[0] = 1;
        ret = HcclRawImrecvScatter(buf1, count1, 1, HCCL_DATA_TYPE_UINT64, &msg, &request);
        EXPECT_EQ(ret, HCCL_E_AGAIN);
        ret = HcclRawImrecvScatter(buf1, count1, 1, HCCL_DATA_TYPE_UINT64, &msg, &request);
        EXPECT_EQ(ret, HCCL_E_NETWORK);
        ret = HcclRawImrecvScatter(buf1, count1, 1, HCCL_DATA_TYPE_UINT64, &msg, &request);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawImrecvScatter success. connPtr[%p]", acceptConn);

        ret = HcclRawClose(acceptConn);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawClose success. acceptConn[%p]", acceptConn);
        ret = HcclRawClose(connPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawClose success. connPtr[%p]", connPtr);
    } else if (noderank == 1) {
        HcclConn connPtr = nullptr;
        // open
        HcclResult ret = HcclRawOpen(&connPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawOpen success. connPtr[%p]", connPtr);

        // bind
        HcclAddr bindAddr;
        bindAddr.type=HCCL_ADDR_TYPE_ROCE;
        bindAddr.info.tcp.ipv4Addr = 16777343;
        bindAddr.info.tcp.port = 60999;
        ret = HcclRawBind(connPtr, &bindAddr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawBind success. connPtr[%p]", connPtr);

        HcclAddr connectAddr;
        connectAddr.type=HCCL_ADDR_TYPE_ROCE;//RDMA
        connectAddr.info.tcp.ipv4Addr = 289669838;
        connectAddr.info.tcp.port = 70999;
        ret = HCCL_E_AGAIN;
        auto startTime = chrono::steady_clock::now();
        auto timeout = chrono::seconds(10);
        while (ret != HCCL_SUCCESS) {
            ret = HcclRawConnect(connPtr, &connectAddr);
            if ((chrono::steady_clock::now() - startTime) > timeout) {
                HCCL_ERROR("HcclRawConnect timeout");
                break;
            }
            SaluSleep(100);
        }
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawConnect success. connPtr[%p]", connPtr);

        MOCKER(hrtIbvPostSend)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        ret = HcclRawIsend(&buf, 1, HCCL_DATA_TYPE_UINT64, connPtr, &request);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawIsend success. connPtr[%p]", connPtr);
        int count = 1;
        int compCount = 0;
        int compIndices[10];
        HcclStatus compStatus[10];
        HcclIpAddress invalidIp;
        HcclIpAddress invalidIp1;
        HcclRequestInfo requestInfo;
        TransportHeterogRawRoce transport1("", invalidIp, invalidIp1, 0, 0, transportResourceInfoBackUp);
        requestInfo.commHandle = connPtr;
        requestInfo.transportHandle = reinterpret_cast<void *>(&transport1);
        requestInfo.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_RECV;
        requestInfo.transportRequest.transData.dataType = HCCL_DATA_TYPE_UINT64;
        struct ibv_wc wc[1];
        wc[0].status = IBV_WC_SUCCESS;
        wc[0].wr_id = reinterpret_cast<uint64_t>(&requestInfo);
        wc[0].opcode = IBV_WC_RDMA_WRITE;
        s32 num = 1;

        MOCKER(hrtIbvPollCq)
        .stubs()
        .with(any(), any(), outBoundP(wc, sizeof(ibv_wc)), outBound(num))
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&TransportHeterogRoce::DeregMr)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        HcclRequestInfo *hcclReq = reinterpret_cast<HcclRequestInfo *>(request);
        hcclReq->transportRequest.requestType = HcclRequestType::HCCL_REQUEST_RECV;
        ret = HcclRawTestSome(count, &request, &compCount, compIndices, compStatus);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawTestSome success. connPtr[%p]", connPtr);

        ret = HcclRawClose(connPtr);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("HcclRawClose success. connPtr[%p]", connPtr);
    }

    GlobalMockObject::verify();

    UseRealPortAndName(false);
    MPI_Barrier(MPI_COMM_WORLD);
}

TEST_F(MPI_RAW_SENDRECV_TEST, st_TypicalQpModify)
{
    HcclEnvelope envelop;
    HcclIpAddress invalidIp;
    TransportHeterogRawRoce transport("", invalidIp, invalidIp, 0, 0, transportResourceInfoBackUp);

    MOCKER(ra_typical_qp_modify)
    .stubs()
    .will(returnValue(528101));

    QpHandle qpHandle;
    struct typical_qp localQpInfo;
    struct typical_qp remoteQpInfo;
    bool completed = false;
    HcclResult ret = transport.TypicalQpModify(qpHandle, &localQpInfo, &remoteQpInfo, completed);
    EXPECT_EQ(ret, HCCL_E_AGAIN);
    GlobalMockObject::verify();
}
#endif