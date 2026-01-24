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
#include "tcp_recv_task.h"
#include "hccl_communicator.h"
#include "transport_heterog.h"
#include "hccd_impl_pml.h"
#include "transport_heterog_roce_pub.h"
#include "tcp_send_thread_pool.h"
#include "transport_heterog_event_roce_pub.h"
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
using namespace std;
using namespace hccl;
constexpr u32 RECV_WQE_BATCH_NUM = 192;
constexpr u32 RECV_WQE_NUM_THRESHOLD = 96;
constexpr u32 RECV_WQE_BATCH_SUPPLEMENT = 96;
class MPI_TRANSPORT_HETEROG_ROCE_TEST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_TRANSPORT_HETEROG_ROCE_TEST SetUP" << std::endl;
        pMsgInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclMessageInfo>(100));
        if (pMsgInfosMem == nullptr) return;
        pMsgInfosMem->Init();

        pReqInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclRequestInfo>(100));
        if (pReqInfosMem == nullptr) return;
        pReqInfosMem->Init();

        memBlocksManager.reset(new (std::nothrow) HeterogMemBlocksManager());
        if (memBlocksManager == nullptr) return;
        memBlocksManager->Init(MEM_BLOCK_NUM);

        pRecvWrInfosMem.reset(new (std::nothrow) LocklessRingMemoryAllocate<RecvWrInfo>(100));
        if (pRecvWrInfosMem == nullptr) return;
        pRecvWrInfosMem->Init();
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_TRANSPORT_HETEROG_ROCE_TEST TearDown" << std::endl;
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
    static std::unique_ptr<HeterogMemBlocksManager> memBlocksManager;
    static std::unique_ptr<LocklessRingMemoryAllocate<RecvWrInfo>> pRecvWrInfosMem;
    TransportResourceInfo transportResourceInfo = TransportResourceInfo(nullptr, pMsgInfosMem, pReqInfosMem,
        memBlocksManager, pRecvWrInfosMem);
};
std::unique_ptr<LocklessRingMemoryAllocate<HcclMessageInfo>> MPI_TRANSPORT_HETEROG_ROCE_TEST::pMsgInfosMem = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<HcclRequestInfo>> MPI_TRANSPORT_HETEROG_ROCE_TEST::pReqInfosMem = nullptr;
std::unique_ptr<HeterogMemBlocksManager> MPI_TRANSPORT_HETEROG_ROCE_TEST::memBlocksManager = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<RecvWrInfo>> MPI_TRANSPORT_HETEROG_ROCE_TEST::pRecvWrInfosMem = nullptr;

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_AllEschedAckCallback)
{
    MOCKER(TransportHeterogEventRoce::UpdateStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unsigned int devId = 0;
    unsigned int subeventId = 0;
    u8 *msg = nullptr;
    unsigned int msgLen = 0;

    TransportHeterogEventRoce::EschedAckCallbackRecvRequest(devId, subeventId, msg, msgLen);
    TransportHeterogEventRoce::EschedAckCallbackSendCompletion(devId, subeventId, msg, msgLen);
    TransportHeterogEventRoce::EschedAckCallbackRecvCompletion(devId, subeventId, msg, msgLen);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_RegisterEschedAckCallback)
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
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    int ret = transportHandle->DeregisterEschedAckCallback();
    ret = transportHandle->RegisterEschedAckCallback();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    transportHandle->DeregisterEschedAckCallback();
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_InitAllLinkVec)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    int ret = transportHandle->InitAllLinkVec();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_EraseTransportFromAllLinkVec)
{
    u64 transportPtr = 0;
    std::unique_lock<std::mutex> lockRecvReq(TransportHeterogEventRoce::gAllLinkVecRecvReqMutex);
    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_RECV_REQUEST_MSG].push_back(&transportPtr);
    lockRecvReq.unlock();

    std::unique_lock<std::mutex> lockSendComp(TransportHeterogEventRoce::gAllLinkVecSendCompMutex);
    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_SEND_COMPLETION_MSG].push_back(&transportPtr);
    lockSendComp.unlock();

    std::unique_lock<std::mutex> lockRecvComp(TransportHeterogEventRoce::gAllLinkVecRecvCompMutex);
    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_RECV_COMPLETION_MSG].push_back(&transportPtr);
    lockRecvComp.unlock();

        HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int ret = transportHandle->EraseTransportFromAllLinkVec(&transportPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

HcclResult stub_CreateQp(RdmaHandle rdmaHandle, int& flag, s32& qpMode, QpInfo& qp)
{
    static struct ibv_qp sqp = {0};
    qp.qp = &sqp;
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_White_List)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective",
        invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    transport->isHdcMode_ = true;
    transport->deviceLogicId_ = HOST_DEVICE_ID;
    transport->tagQpInfo_.qpMode = OFFLINE_QP_MODE;
    std::string tag = "0000";

    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::InitUnRegMrMap, HcclResult(MrManager::*)(map<MrMapKey, MrInfo>&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::DeInit, HcclResult (MrManager::*)(const void *))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::GetNetworkResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::ConnectAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::PrepareSocketInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&RankConsistentcyChecker::GetCheckFrame)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(DestroyQpWithCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::SocketClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CreateQp)
    .stubs()
    .with(any())
    .will(invoke(stub_CreateQp));

    int ret = transport->AddSocketWhiteList(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport->PreQpConnect();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::list<void *> blockList;
    ret = transport->AllocMemBlocks(blockList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *block;
    ret = transport->FreeMemBlock(block);
    EXPECT_EQ(ret, 4);

    ret = transport->MemBlocksManagerDeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transport->MrManagerDeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

HcclResult stub_CreateNormalQp(RdmaHandle rdmaHandle, QpInfo& qp)
{
    static struct ibv_qp sqp = {0};
    qp.qp = &sqp;
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_Init)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    TransportHeterogEventRoce *transportHandle_1 = transport.get();
    RaResourceInfo raResourceInfo;
    IpSocket ipSocket;
    u64 nicSocketHandle = 0;
    rdevInfo_t nicRdmaHandle = {0};
    ipSocket.nicSocketHandle = reinterpret_cast<void *>(&nicSocketHandle);
    ipSocket.nicRdmaHandle = reinterpret_cast<void *>(&nicRdmaHandle);
    raResourceInfo.nicSocketMap[transportHandle->selfIp_] = ipSocket;

    MOCKER_CPP(&NetworkManager::GetRaResourceInfo)
    .stubs()
    .with(outBound(raResourceInfo))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(CreateQp)
    .stubs()
    .with(any())
    .will(invoke(stub_CreateQp));

    MOCKER(CreateNormalQp, HcclResult (*)(RdmaHandle, QpInfo&))
    .stubs()
    .with(any())
    .will(invoke(stub_CreateNormalQp));

    MOCKER(hrtRaCreateCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::RegisterEschedAckCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::InitAllLinkVec)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::TryTransition)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&RankConsistentcyChecker::GetCheckFrame)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::ConnectAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaNormalQpDestroy)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaDestroyCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    int ret = transportHandle->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transportHandle->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHandle_1->srqInit_ = true;
    ret = transportHandle_1->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transportHandle_1->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_Deinit)
{
    MOCKER_CPP(&TransportHeterogEventRoce::DeregisterEschedAckCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::DeinitAllLinkVec)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    TransportHeterogRoce roce("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce roce("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(roce, &TransportHeterogRoce::Deinit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    int ret = transportHandle->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_Send)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    TransData sendData;
    TransportEndPointParam epParam;
    int ret = transportHandle->Send(sendData, epParam);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_Imrecv)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    struct InitStateMachine initSM;
    SocketInfoT tmpSocketInfo;
    initSM.locInitInfo.socketInfo.push_back(tmpSocketInfo);
    std::string tag = "name";
    strncpy_s(tmpSocketInfo.tag, SOCK_CONN_TAG_SIZE, tag.c_str(), tag.length() + 1);
    transport.initSM_ = initSM;
    TransData recvData;
    HcclMessageInfo msg;
    HcclRequestInfo* request;
    bool flag;
    bool needRecordFlag = true;

    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Imrecv,
        HcclResult (TransportHeterogRoce::*)(const TransData&, HcclMessageInfo&, HcclRequestInfo*&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    transport.deviceLogicId_ = HOST_DEVICE_ID;
    transport.Imrecv(recvData, msg, request, flag, needRecordFlag);
    MOCKER(GetExternalInputHcclLinkTimeOut)
    .stubs()
    .with(any())
    .will(returnValue(1));

    MOCKER_CPP(&TransportHeterog::CheckAndPushBuildLink)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_AGAIN));
    transport.WaitBuildLinkComplete();

    MOCKER_CPP(&TransportHeterog::CheckAndPushBuildLink)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    transport.WaitBuildLinkComplete();

    MOCKER_CPP(&TransportHeterog::CheckAndPushBuildLink)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));
    transport.WaitBuildLinkComplete();
    transport.GetLinkTag(tag);

}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogEventRoce_SendFlowControl)
{
    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtIbvPostRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    TransportHeterogEventRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogEventRoce transportEvent("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogEventRoce::PullSendStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    TransportHeterogEventRoce *transportHandle_1 = transport.get();

    transportHandle->dataRecvWqeNum_ = RECV_WQE_NUM_THRESHOLD;
    transportHandle->dataRecvWqeExpNum_ = RECV_WQE_BATCH_SUPPLEMENT - 1;
    transportHandle_1->dataRecvWqeNum_ = RECV_WQE_NUM_THRESHOLD;
    hccl::TransportHeterogEventRoce::gQpnToSqMaxWrMap[0] = 1;
    struct ibv_qp qp= {0};
    transportHandle->dataQpInfo_.qp = &qp;

    int ret = transportHandle->SendFlowControl();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHandle_1->srqInit_ = true;
    ret = transportHandle_1->SendFlowControl();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_SendFlowControl)
{
    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtIbvPostRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    TransportHeterogRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce transportEvent("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogRoce::PullSendStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();
    transportHandle->dataRecvWqeNum_ = RECV_WQE_NUM_THRESHOLD;
    transportHandle->dataRecvWqeExpNum_ = RECV_WQE_NUM_THRESHOLD;
    struct ibv_qp qp= {0};
    transportHandle->dataQpInfo_.qp = &qp;

    int ret = transportHandle->SendFlowControl();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_ParseErrorTagSqe)
{
    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    u64 buf = 0;
    HcclRequestInfo request;
    request.transportRequest.transData.srcBuf = reinterpret_cast<u64>(&buf);
    request.transportRequest.transData.count = 1;
    request.transportRequest.transData.dataType = HCCL_DATA_TYPE_INT8;
    request.transportHandle = transportHandle;
    struct ibv_wc wc[HCCL_POLL_CQ_DEPTH];
    wc[0].wr_id = reinterpret_cast<u64>(&request);
    wc[0].status = IBV_WC_WR_FLUSH_ERR;
    wc[0].opcode = IBV_WC_SEND;

    int ret = transportHandle->ParseErrorTagSqe(wc, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogEventRoce_ParseTagRqes)
{
    MOCKER_CPP(&TransportHeterogEventRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::FreeMemBlock)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::FreeRecvWrId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogRoce::SaveEnvelope)
    .stubs()
    .with(any())
    .will(returnValue(nullptr));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int num = 1;
    struct ibv_wc wc[HCCL_POLL_CQ_DEPTH] = {0};
    RecvWrInfo info;
    HcclEnvelope envelope;
    info.transportHandle = transport.get();
    info.buf = &envelope;
    wc[0].wr_id = reinterpret_cast<u64>(&info);
    wc[0].status = IBV_WC_SUCCESS;
    wc[0].opcode = IBV_WC_SEND;

    int ret = transportHandle->ParseTagRqes(wc, num);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_ParseTagRqes)
{
    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
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
    MOCKER_CPP(&TransportHeterogRoce::SaveEnvelope)
    .stubs()
    .with(any())
    .will(returnValue(nullptr));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();

    int num = 1;
    struct ibv_wc wc[HCCL_POLL_CQ_DEPTH] = {0};
    RecvWrInfo info;
    HcclEnvelope envelope;
    info.transportHandle = transport.get();
    info.buf = &envelope;
    wc[0].wr_id = reinterpret_cast<u64>(&info);
    wc[0].status = IBV_WC_SUCCESS;
    wc[0].opcode = IBV_WC_SEND;

    int ret = transportHandle->ParseTagRqes(wc, num);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogEventRoce_ParseDataRqes)
{
    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::FreeMemBlock)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::FreeRecvWrId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    TransportHeterogEventRoce *transportHandle_1 = transport.get();
    transportHandle->dataRecvWqeNum_ = RECV_WQE_NUM_THRESHOLD;
    hccl::TransportHeterogEventRoce::gQpnToSqMaxWrMap[0] = 1;
    int num = 1;
    u64 buf = 0;
    struct ibv_wc wcDataRq[HCCL_POLL_CQ_DEPTH] = {0};
    RecvWrInfo info;
    HcclRequestInfo request;
    request.transportRequest.transData.srcBuf = reinterpret_cast<u64>(&buf);
    request.transportRequest.transData.count = 1;
    request.transportRequest.transData.dataType = HCCL_DATA_TYPE_INT8;
    request.transportHandle = transport.get();
    u64 temp = reinterpret_cast<u64>(&request);
    info.buf = &temp;
    wcDataRq[0].wr_id = reinterpret_cast<u64>(&info);
    wcDataRq[0].status = IBV_WC_SUCCESS;
    wcDataRq[0].opcode = IBV_WC_SEND;

    int ret = transportHandle->ParseDataRqes(wcDataRq, num);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wcDataRq[0].status = IBV_WC_WR_FLUSH_ERR;
    ret = transportHandle->ParseDataRqes(wcDataRq, num);
    EXPECT_EQ(ret, HCCL_E_NETWORK);

    transportHandle_1->srqInit_ = true;
    wcDataRq[0].status = IBV_WC_SUCCESS;
    ret = transportHandle_1->ParseDataRqes(wcDataRq, num);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wcDataRq[0].status = IBV_WC_WR_FLUSH_ERR;
    ret = transportHandle_1->ParseDataRqes(wcDataRq, num);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_ParseDataRqes)
{
    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
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

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();

    transportHandle->dataRecvWqeNum_ = RECV_WQE_NUM_THRESHOLD;
    int num = 1;
    u64 buf = 0;
    struct ibv_wc wcDataRq[HCCL_POLL_CQ_DEPTH] = {0};
    RecvWrInfo info;
    HcclRequestInfo request;
    request.transportRequest.transData.srcBuf = reinterpret_cast<u64>(&buf);
    request.transportRequest.transData.count = 1;
    request.transportRequest.transData.dataType = HCCL_DATA_TYPE_INT8;
    request.transportHandle = transport.get();
    u64 temp = reinterpret_cast<u64>(&request);
    info.buf = &temp;
    wcDataRq[0].wr_id = reinterpret_cast<u64>(&info);
    wcDataRq[0].status = IBV_WC_SUCCESS;
    wcDataRq[0].opcode = IBV_WC_SEND;

    int ret = transportHandle->ParseDataRqes(wcDataRq, num);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wcDataRq[0].status = IBV_WC_WR_FLUSH_ERR;
    ret = transportHandle->ParseDataRqes(wcDataRq, num);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_ParseDataSqes)
{
    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int num = 1;
    u64 buf = 0;
    struct ibv_wc wc[HCCL_POLL_CQ_DEPTH];
    HcclRequestInfo request;
    request.transportRequest.transData.srcBuf = reinterpret_cast<u64>(&buf);
    request.transportRequest.transData.count = 1;
    request.transportRequest.transData.dataType = HCCL_DATA_TYPE_INT8;
    request.transportHandle = transport.get();
    wc[0].wr_id = reinterpret_cast<u64>(&request);
    wc[0].status = IBV_WC_SUCCESS;
    wc[0].opcode = IBV_WC_SEND;

    int ret = transportHandle->ParseDataSqes(wc, num);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    wc[0].status = IBV_WC_WR_FLUSH_ERR;
    ret = transportHandle->ParseDataSqes(wc, num);
    EXPECT_EQ(ret, HCCL_E_NETWORK);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_PullRecvRequestStatus)
{
    HcclIpAddress invalidIp;
    TransportHeterogEventRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogEventRoce transportEvent("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogEventRoce::PullRecvRequestStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int ret = TransportHeterogEventRoce::PullRecvRequestStatus(transportHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_PullRecvRequestStatus_1)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogRoce transportEvent("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogRoce::PullRecvRequestStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int ret = transportHandle->PullRecvRequestStatus(true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_PullSendStatus)
{
    HcclIpAddress invalidIp;
    TransportHeterogEventRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogEventRoce transportEvent("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogEventRoce::PullSendStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int ret = TransportHeterogEventRoce::PullSendStatus(transportHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_PullRecvStatus)
{
    HcclIpAddress invalidIp;
    TransportHeterogEventRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogEventRoce transportEvent("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogEventRoce::PullRecvStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int ret = TransportHeterogEventRoce::PullRecvStatus(transportHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_UpdateStatus)
{
    HcclIpAddress invalidIp;
    TransportHeterogEventRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    // TransportHeterogEventRoce transportEvent("test_ta", 0, 1, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogEventRoce::PullRecvRequestStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogEventRoce::PullSendStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogEventRoce::PullRecvStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_RECV_REQUEST_MSG] = 0;
    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_SEND_COMPLETION_MSG] = 0;
    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_RECV_COMPLETION_MSG] = 0;

    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    transportHandle->connState_ = ConnState::CONN_STATE_COMPLETE;

    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_RECV_REQUEST_MSG].clear();
    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_SEND_COMPLETION_MSG].clear();
    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_RECV_COMPLETION_MSG].clear();

    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_RECV_REQUEST_MSG].push_back(transportHandle);
    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_SEND_COMPLETION_MSG].push_back(transportHandle);
    TransportHeterogEventRoce::gAllLinkVec[HCCL_EVENT_RECV_COMPLETION_MSG].push_back(transportHandle);

    int ret = transportHandle->UpdateStatus(HCCL_EVENT_RECV_REQUEST_MSG);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transportHandle->UpdateStatus(HCCL_EVENT_SEND_COMPLETION_MSG);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transportHandle->UpdateStatus(HCCL_EVENT_RECV_COMPLETION_MSG);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_EschedAckCallback)
{
    MOCKER(TransportHeterogEventRoce::UpdateStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_RECV_REQUEST_MSG] = 1;
    TransportHeterogEventRoce::EschedAckCallback(0, HCCL_EVENT_RECV_REQUEST_MSG);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_InitTagRecvWqe)
{
    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();

    int ret = transportHandle->InitDataRecvWqe();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transportHandle->InitTagRecvWqe();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}


TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportEventHeterogRoce_InitTagRecvWqe)
{
    MOCKER_CPP(&TransportHeterogRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventRoce::IssueRecvWqe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    TransportHeterogEventRoce *transportHandle_1 = transport.get();

    int ret = transportHandle->InitDataRecvWqe();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transportHandle->InitTagRecvWqe();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    transportHandle_1->srqInit_ = true;
    ret = transportHandle_1->InitDataRecvWqe();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = transportHandle_1->InitTagRecvWqe();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

HcclResult stub_AllocMemBlocks(TransportHeterogRoce* roce, std::list<void *> &blockList)
{
    static HcclRequestInfo requestInfo;
    blockList.push_front(&requestInfo);
    return HCCL_SUCCESS;
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_IssueRecvWqe)
{
    MOCKER_CPP(&TransportHeterogRoce::AllocMemBlocks)
    .stubs()
    .with(any())
    .will(invoke(stub_AllocMemBlocks));

    MOCKER_CPP(&TransportHeterogEventRoce::GenerateRecvWrId)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtIbvPostSrqRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    struct ibv_srq* srq;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();

    int ret = transportHandle->IssueRecvWqe(srq, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_QpConnect)
{
    MOCKER(HrtRaQpNonBlockConnectAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    SocketInfoT socketInfo1;
    transport->initSM_.locInitInfo.socketInfo.push_back(socketInfo1);
    transport->initSM_.locInitInfo.socketInfo.push_back(socketInfo1);
    transport->initSM_.locInitInfo.socketInfo.push_back(socketInfo1);
    transport->initSM_.locInitInfo.socketInfo.push_back(socketInfo1);
    transport->initSM_.locInitInfo.socketInfo.push_back(socketInfo1);

    bool completed;
    int ret = transportHandle->QpConnect(completed);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogEventRoce_GetQpStatus)
{
    int qpStatus = 0;
    MOCKER(hrtGetRaQpStatus)
    .stubs()
    .with(any(), outBound(&qpStatus))
    .will(returnValue(0));

    bool completed;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();
    int ret = transportHandle->GetQpStatus(completed);
    EXPECT_EQ(ret, HCCL_E_AGAIN);
    GlobalMockObject::verify();

    qpStatus = 1;
    MOCKER(hrtGetRaQpStatus)
    .stubs()
    .with(any(), outBound(&qpStatus))
    .will(returnValue(0));
    ret = transportHandle->GetQpStatus(completed);
    EXPECT_EQ(ret, HCCL_E_AGAIN);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_hccd_TransportHeterogRoce_CreateSrq_success)
{
    std::unique_ptr<HccdImplPml> impl;
    impl.reset(new (std::nothrow) HccdImplPml());

    impl->srqInit_ = true;
    int ret = impl->CreateSrq();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    impl->srqInit_ = false;
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_FlushSendQueue)
{
    MOCKER_CPP(&TransportHeterogEventRoce::SendEnvelope)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclEnvelope tmpEnvelopInfo;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogEventRoce> transport(new (nothrow) TransportHeterogEventRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *transportHandle = transport.get();
    transportHandle->envelopeBacklogQueue_.push(tmpEnvelopInfo);

    bool completed;
    int ret = transportHandle->FlushSendQueue(completed);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_CreateCqAndQp)
{
    MOCKER(CreateQpWithCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();
    int ret = transportHandle->CreateCqAndQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(CreateQpWithSharedCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    unique_ptr<TransportHeterogEventRoce> eventTransport(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    TransportHeterogEventRoce *eventTransportHandle = eventTransport.get();
    TransportHeterogEventRoce::gNeedRepoEvent = false;
    ret = eventTransportHandle->CreateCqAndQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_PullSendOrRecvStatus)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transportEvent("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogRoce::PullSendStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));

    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();
    HcclRequestInfo request;
    request.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_SEND;
    int ret = transportHandle->PullSendOrRecvStatus(request);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP_VIRTUAL(transportEvent, &TransportHeterogRoce::PullRecvStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    request.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_RECV;
    ret = transportHandle->PullSendOrRecvStatus(request);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    request.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_INVAIL;
    ret = transportHandle->PullSendOrRecvStatus(request);
    EXPECT_EQ(ret, HCCL_E_PARA);

    MOCKER_CPP(&TransportHeterog::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_SEND_STATUS));

    ret = transportHandle->PullSendOrRecvStatus(request);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogRoce_Wait)
{
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();

    MOCKER_CPP(&TransportHeterogRoce::PullSendOrRecvStatus)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterog::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_SEND_STATUS));

    MOCKER_CPP(&TransportHeterog::ConnectAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclRequestInfo request;
    s32 flag = HCCL_TEST_INCOMPLETED;
    request.transportRequest.status = 1;
    int ret = transportHandle->Wait(request, flag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    flag = HCCL_TEST_COMPLETED;
    ret = transportHandle->Wait(request, flag);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    GlobalMockObject::verify();
}

TEST_F(MPI_TRANSPORT_HETEROG_ROCE_TEST, ut_TransportHeterogEventRoce_Imrecv)
{
    HcclIpAddress invalidIp;
    TransData recvData;
    HcclMessageInfo msg;
    HcclRequestInfo* request;

    unique_ptr<TransportHeterogEventRoce> transport1(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Imrecv,
        HcclResult (TransportHeterogRoce::*)(const TransData&, HcclMessageInfo&, HcclRequestInfo*&))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    TransportHeterogEventRoce *transportHandle = transport1.get();
    int ret = transportHandle->Imrecv(recvData, msg, request);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}