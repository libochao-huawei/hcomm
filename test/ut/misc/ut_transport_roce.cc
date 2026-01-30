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

#define private public
#define protected public
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
#include "hccl_communicator.h"
#include "transport_heterog.h"
#include "transport_heterog_roce_pub.h"
#include "transport_roce_pub.h"
#include "mr_manager.h"
#include "network_manager_pub.h"
#undef private

#include <hccl/hccl_comm.h>
#include <hccl/hccl_inner.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "adapter_rts.h"
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
#include "ffts_common_pub.h"
using namespace std;
using namespace hccl;

class MPI_TRANSPORT_ROCE_TEST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        SetFftsSwitch(false);
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "MPI_TRANSPORT_ROCE_TEST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "MPI_TRANSPORT_ROCE_TEST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        hrtSetDevice(0);
        ResetInitState();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        ClearHalEvent();
        HcclOpMetaInfo meta;
        bool hasMassTasks = true;
        hccl::Stream stream;
        ::InitTask(dispatcherPtr, stream, meta.isEnableCache, meta.GetCacheKey(), false);
        if (hasMassTasks) {
            SetNormalMode(dispatcherPtr);
        }
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A TestCase TearDown" << std::endl;
    }
    TransportResourceInfo transportResourceInfo = TransportResourceInfo();

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher MPI_TRANSPORT_ROCE_TEST::dispatcherPtr = nullptr;
DispatcherPub *MPI_TRANSPORT_ROCE_TEST::dispatcher = nullptr;

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_SendAsync)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Isend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    int ret = roce.SendAsync(sendParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER_CPP(&TransportHeterog::WaitBuildLinkComplete)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    roce.isESMode_ = true;
    ret = roce.Connect();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    roce.Break();
    ret = roce.IsProcessStop();
    EXPECT_EQ(ret, HCCL_E_AGAIN);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_Send_Success)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    HcclRequestInfo request;
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Isend)
    .stubs()
    .with(any(), any(), outBound(&request))
    .will(returnValue(HCCL_SUCCESS));

    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    int ret = roce.Send(sendParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_Send_Fail)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Isend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::WaitCompletion)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);
    roce.isESMode_ = true;
    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    int ret = roce.Send(sendParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_WaitSendAsyncComplete)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::WaitCompletion)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    HcclRequestInfo sendRequest;
    sendParam.sendRequest = &sendRequest;
    int ret = roce.WaitSendAsyncComplete(sendParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_Recv_Success)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce transport("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    HcclMessageInfo msg;
    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Improbe, HcclResult (TransportHeterogRoce::*)(const TransportEndPointParam&, s32&, HcclMessageInfo*&, HcclStatus&))
    .stubs()
    .with(any(), outBound(1), outBound(&msg), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Imrecv,
        HcclResult (TransportHeterogRoce::*)(const TransData&, HcclMessageInfo&, HcclRequestInfo*&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::WaitCompletion)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);
    roce.isESMode_ = true;
    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    int ret = roce.Recv(sendParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_Recv_Fail)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce transport("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    HcclMessageInfo msg;
    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Improbe, HcclResult (TransportHeterogRoce::*)(const TransportEndPointParam&, s32&, HcclMessageInfo*&, HcclStatus&))
    .stubs()
    .with(any(), outBound(1), outBound(&msg), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Imrecv,
        HcclResult (TransportHeterogRoce::*)(const TransData&, HcclMessageInfo&, HcclRequestInfo*&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::WaitCompletion)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);
    roce.isESMode_ = true;
    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    int ret = roce.Recv(sendParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_SendRecv_Success)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce transport("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    HcclRequestInfo request;
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Isend)
    .stubs()
    .with(any(), any(), outBound(&request))
    .will(returnValue(HCCL_SUCCESS));

    HcclMessageInfo msg;
    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Improbe, HcclResult (TransportHeterogRoce::*)(const TransportEndPointParam&, s32&, HcclMessageInfo*&, HcclStatus&))
    .stubs()
    .with(any(), outBound(1), outBound(&msg), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Imrecv,
        HcclResult (TransportHeterogRoce::*)(const TransData&, HcclMessageInfo&, HcclRequestInfo*&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    SendRecvParam recvParam(&sendData, len, streamId, &roce);
    int ret = roce.WaitSendAsyncCompleteAndRecv(sendParam, recvParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_SendRecv_Fail)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce transport("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    HcclRequestInfo request;
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Isend)
    .stubs()
    .with(any(), any(), outBound(&request))
    .will(returnValue(HCCL_SUCCESS));

    HcclMessageInfo msg;
    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Improbe, HcclResult (TransportHeterogRoce::*)(const TransportEndPointParam&, s32&, HcclMessageInfo*&, HcclStatus&))
    .stubs()
    .with(any(), outBound(1), outBound(&msg), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Imrecv,
        HcclResult (TransportHeterogRoce::*)(const TransData&, HcclMessageInfo&, HcclRequestInfo*&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::WaitCompletion)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    u32 sendData = 0;
    u64 len = 4;
    s32 streamId = 0;
    SendRecvParam sendParam(&sendData, len, streamId, &roce);
    SendRecvParam recvParam(&sendData, len, streamId, &roce);
    int ret = roce.WaitSendAsyncCompleteAndRecv(sendParam, recvParam);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_WaitCompletion)
{
    struct ibv_cq cq;
    struct ibv_cq *evCq = &cq;
    void *cqContext;
    MOCKER(hrtIbvReqNotifyCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtIbvGetCqEvent)
    .stubs()
    .with(any(), outBoundP(&evCq, sizeof(evCq)), outBoundP(&cqContext, sizeof(cqContext)))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtIbvAckCqEvent)
    .stubs()
    .with(any());

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    struct ibv_cq* notifyCq;
    struct ibv_comp_channel *channel;
    int ret = roce.WaitCompletion(notifyCq, channel);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TaskExec)
{
    MOCKER_CPP(&TransportRoce::Send)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::Recv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::WaitSendAsyncCompleteAndRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::SendAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    s32 streamId = 0;
    s32 queIndex = 1;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    u32 sendData = 0;
    u64 len = 4;
    SendRecvParam param(&sendData, len, streamId, &roce);

    int ret = roce.TaskExec(streamId, queIndex);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    queIndex = 1;
    SendRecvParam tempParam(streamId, &roce, queIndex);
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_SEND, param));
    ret = roce.TaskExec(streamId, queIndex);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    queIndex = 2;
    roce.taskOrchestration_.clear();
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_SEND, param));
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_WAIT_DONE, tempParam));
    ret = roce.TaskExec(streamId, queIndex);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    queIndex = 1;
    roce.taskOrchestration_.clear();
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_RECV, param));
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_WAIT_DONE, tempParam));
    ret = roce.TaskExec(streamId, queIndex);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    queIndex = 2;
    roce.taskOrchestration_.clear();
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_SEND, param));
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_RECV, param));
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_WAIT_DONE, tempParam));
    ret = roce.TaskExec(streamId, queIndex);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_isSupportTransportWithReduce)
{
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    bool ret = roce.IsSupportTransportWithReduce();
    EXPECT_EQ(ret, true);
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_RxWithReduce)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    UserMemType recvSrcMemType = UserMemType::INPUT_MEM;
    u32 recvData = 0;
    Stream stream;

    int ret = roce.RxWithReduce(recvSrcMemType, 0, &recvData, 4, &recvData, &recvData, 4,
        HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream, INLINE_REDUCE_BIT);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportBase* rocePtr = new (std::nothrow) TransportRoce(nullptr, nullptr,
        machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);
    EXPECT_NE(rocePtr, nullptr);

    Transport link_base(rocePtr);
    link_base.RxWithReduce(recvSrcMemType, 0, &recvData, 4, &recvData, &recvData, 4,
        HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream, INLINE_REDUCE_BIT);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TxWithReduce)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtCallbackLaunch)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    UserMemType dstMemType = UserMemType::OUTPUT_MEM;
    u32 srcData = 0;
    Stream stream;

    int ret = roce.TxWithReduce(dstMemType, 0, &srcData, 4, HCCL_DATA_TYPE_INT8, HCCL_REDUCE_SUM, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_GetRemoteMem)
{
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(nullptr, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    u32 memory = 0;
    roce.remoteMemMsg_[static_cast<u32>(UserMemType::INPUT_MEM)].addr = &memory;
    roce.remoteMemMsg_[static_cast<u32>(UserMemType::OUTPUT_MEM)].addr = &memory;
    roce.remoteMemMsg_[static_cast<u32>(UserMemType::MEM_RESERVED)].addr = nullptr;

    void *remotePtr = &memory;
    int ret = roce.GetRemoteMem(UserMemType::INPUT_MEM, &remotePtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = roce.GetRemoteMem(UserMemType::MEM_RESERVED, &remotePtr);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_RxWaitDone)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtCallbackLaunch)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    DispatcherPub dispatcherDummy(0);
    MOCKER_CPP_VIRTUAL(dispatcherDummy, &DispatcherPub::ReduceAsync, HcclResult(DispatcherPub::*)(const void *, void *, u64, const HcclDataType,
        HcclReduceOp, Stream&, HcclReduceType))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    s32 streamId = 0;
    s32 queIndex = 0;
    SendRecvParam tempParam(streamId, &roce, queIndex);
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_SEND, tempParam));

    Stream stream;
    roce.recvWithReduceParam_.stream = &stream;
    roce.recvWithReduceParam_.reduceOp = HCCL_REDUCE_SUM;
    roce.recvWithReduceParam_.datatype = HCCL_DATA_TYPE_INT8;
    int ret = roce.RxWaitDone(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TxWaitDone)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtCallbackLaunch)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    DispatcherPub dispatcherDummy(0);
    MOCKER_CPP_VIRTUAL(dispatcherDummy, &DispatcherPub::ReduceAsync, HcclResult(DispatcherPub::*)(const void *, void *, u64, const HcclDataType,
        HcclReduceOp, Stream&, HcclReduceType))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    s32 streamId = 0;
    s32 queIndex = 0;
    SendRecvParam tempParam(streamId, &roce, queIndex);
    roce.taskOrchestration_[streamId].push_back(std::make_pair(OperationType::OP_SEND, tempParam));

    Stream stream;
    roce.recvWithReduceParam_.stream = &stream;
    roce.recvWithReduceParam_.reduceOp = HCCL_REDUCE_SUM;
    roce.recvWithReduceParam_.datatype = HCCL_DATA_TYPE_INT8;
    int ret = roce.TxWaitDone(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TaskExecCallback)
{
    MOCKER(hrtSetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::TaskExec)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    SendRecvParam param;
    param.transportRocePtr = &roce;
    // TaskExecCallback(&param);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_RxAck)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    int ret = roce.RxAck(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TxAck)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    int ret = roce.TxAck(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_RxAsync)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    int ret = roce.RxAsync(UserMemType::INPUT_MEM, 0, nullptr, 1024, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_RxAsync_alltoallv)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    std::vector<RxMemoryInfo> rxMems;
    int ret = roce.RxAsync(rxMems, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TxAsync_alltoallv)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    std::vector<TxMemoryInfo> txMems;
    int ret = roce.TxAsync(txMems, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TxAsync)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtCallbackLaunch)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    int ret = roce.TxAsync(UserMemType::OUTPUT_MEM, 0, nullptr, 1024, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_RxDataSignal)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    int ret = roce.RxDataSignal(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_TxDataSignal)
{
    MOCKER(hrtGetStreamId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    Stream stream;
    int ret = roce.TxDataSignal(stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_DeInit)
{
    MOCKER_CPP(&MrManager::DeRegGlobalMr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Deinit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    void *rdmaHandle = (void *)0xabcd;
    std::unique_ptr<MrManager> mrManager;
    mrManager.reset(new (std::nothrow) MrManager());
    mrManager->Init(rdmaHandle);
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, TransportResourceInfo(mrManager, nullptr, nullptr, nullptr, nullptr));

    roce.isInited_ = true;
    roce.isHdcMode_ = true;
    u64 inputAddr = 0;
    u64 outputAddr = 0;
    DeviceMem inputMem = DeviceMem::create(&inputAddr, sizeof(inputAddr));
    DeviceMem outputMem = DeviceMem::create(&outputAddr, sizeof(outputAddr));
    roce.machinePara_.inputMem = inputMem;
    roce.machinePara_.outputMem = outputMem;
    int ret = roce.DeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_RegUserMem)
{
    MOCKER_CPP(&MrManager::RegGlobalMr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBlockSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    s32 fd = 0;
    void *fdHandle = &fd;
    roce.socketFdHandles_.push_back(fdHandle);

    int ret = roce.RegUserMem(MemType::USER_INPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = roce.RegUserMem(MemType::USER_OUTPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = roce.RegUserMem(MemType::DATA_NOTIFY_MEM);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_GetRemoteAddr)
{
    MemMsg mrMsg;
    void *dataPtr = &mrMsg;
    mrMsg.memType = MemType::USER_INPUT_MEM;
    u64 size = 1024;
    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any(), outBoundP(dataPtr, sizeof(dataPtr)), outBound(size))
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    s32 fd = 0;
    void *fdHandle = &fd;
    roce.socketFdHandles_.push_back(fdHandle);

    int ret = roce.GetRemoteAddr(MemType::USER_INPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = roce.GetRemoteAddr(MemType::USER_OUTPUT_MEM);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_InitMem)
{
    MOCKER_CPP(&TransportRoce::RegUserMem)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::GetRemoteAddr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    int ret = roce.InitMem();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_CreateCqAndQp)
{
    MOCKER(hrtRaCreateCompChannel)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CreateQpWithCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    int ret = roce.CreateCqAndQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    roce.isESMode_ = true;
    ret = roce.CreateCqAndQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_DestroyCqAndQp)
{
    MOCKER(DestroyQpWithCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaDestroyCompChannel)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    int ret = roce.DestroyCqAndQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    roce.isESMode_ = true;
    ret = roce.DestroyCqAndQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_GetNicHandle)
{
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    HcclIpAddress invalidIp;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);


    roce.machinePara_.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    IpSocket socket;
    s32 nic = 0;
    socket.nicRdmaHandle = &nic;
    socket.nicSocketHandle = &nic;
    roce.machinePara_.deviceLogicId = 0;
    RaResourceInfo &raResourceInfo = NetworkManager::GetInstance(roce.machinePara_.deviceLogicId).raResourceInfo_;
    raResourceInfo.nicSocketMap[HcclIpAddress(0)] = socket;
    raResourceInfo.nicSocketMap[HcclIpAddress(1)] = socket;
    roce.machinePara_.localIpAddr = HcclIpAddress(0);
    roce.machinePara_.localIpAddr = HcclIpAddress(1);

    std::vector<HcclSocketInfo> socketInfos;
    HcclSocketInfo socketInfo;
    socketInfo.socketHandle = &nic;
    socketInfos.push_back(socketInfo);
    socketInfos.push_back(socketInfo);

    int ret = roce.GetNicHandle();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    raResourceInfo.nicSocketMap.clear();
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_Init)
{
    MOCKER(hrtGetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::GetNicHandle)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::WaitCompletion)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce transport("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Init, HcclResult (TransportHeterogRoce::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::InitMem)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportRoce::GetSocketInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclRequestInfo request;
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Isend)
    .stubs()
    .with(any(), any(), outBound(&request))
    .will(returnValue(HCCL_SUCCESS));

    HcclStatus compState = {0};
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Test)
    .stubs()
    .with(any(), outBound(1), outBound(compState))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Improbe, HcclResult (TransportHeterogRoce::*)(const TransportEndPointParam&, s32&, HcclMessageInfo*&, HcclStatus&))
    .stubs()
    .with(any(), outBound(1), any(), any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Imrecv,
        HcclResult (TransportHeterogRoce::*)(const TransData&, HcclMessageInfo&, HcclRequestInfo*&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    DeviceMem inputMem = DeviceMem::alloc(sizeof(s32));
    DeviceMem outputMem = DeviceMem::alloc(sizeof(s32));

    roce.machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
    roce.machinePara_.serverId = "name";
    roce.machinePara_.localDeviceId = 1;
    roce.machinePara_.remoteDeviceId = 0;
    roce.machinePara_.localUserrank = 0;
    roce.machinePara_.localWorldRank = 0;
    roce.machinePara_.remoteUserrank = 1;
    roce.machinePara_.remoteWorldRank = 1;

    roce.machinePara_.deviceType = DevType::DEV_TYPE_310P3;
    roce.machinePara_.inputMem = inputMem;
    roce.machinePara_.outputMem = outputMem;
    roce.machinePara_.localIpAddr = HcclIpAddress(0);
    int ret = roce.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    roce.machinePara_.localUserrank = 1;
    roce.machinePara_.remoteUserrank = 0;
    ret = roce.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_hrtIbvGetCqEvent_err)
{
    struct ibv_comp_channel *channel = nullptr;
    struct ibv_cq **cq = nullptr;
    void **cq_context = nullptr;

    HcclResult ret = hrtIbvGetCqEvent(channel, cq, cq_context);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

TEST_F(MPI_TRANSPORT_ROCE_TEST, ut_TransportRoce_DeInit_tmp)
{
    MOCKER_CPP(&MrManager::DeRegGlobalMr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Deinit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    TransportRoce roce(dispatcher, nullptr, machinePara, timeout, invalidIp, invalidIp, 18000, 18000, transportResourceInfo);

    roce.isInited_ = true;
    int ret = roce.DeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}