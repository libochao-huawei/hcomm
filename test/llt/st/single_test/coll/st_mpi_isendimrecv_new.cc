/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
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
#include "hccd/hccd_comm.h"
#include "hccd/hccd_impl_pml.h"
#include "hccd_comm.h"
#include "hccd_impl_pml.h"
#include "hccl_communicator.h"
#include "transport_heterog.h"
#include "transport_heterog_event_tcp_pub.h"
#include "transport_heterog_roce_pub.h"
#include "tcp_send_thread_pool.h"
#include "transport_heterog_event_roce_pub.h"
#include "network_manager_pub.h"
#undef protected
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
#include "transport_base_pub.h"
using namespace std;
using namespace hccl;

class MPI_ISENDIRECV_TEST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_ISENDIRECV_TEST SetUP" << std::endl;
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
        TestConstructParam(params, rankTable);
        NetworkManager::GetInstance(0).hostNicInitRef_.Clear();
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_ISENDIRECV_TEST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        hrtSetDevice(0);
        ResetInitState();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        ClearHalEvent();
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
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

    static void TestConstructParam(HcclCommParams &params, RankTable_t &rankTable)
    {
        string commId = "comm ";
        memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
        params.rank = 0;
        params.totalRanks = 2;
        params.isHeterogComm = false;
        params.logicDevId = 0;
        params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
        params.deviceType = DevType::DEV_TYPE_910;

        rankTable.collectiveId = "192.168.0.101-8000-8001";
        vector<RankInfo_t> rankVec(2);
        rankVec[0].rankId = 0;
        rankVec[0].deviceInfo.devicePhyId = 0;
        HcclIpAddress ipAddr1(1811982528);
        rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 108.0.168.192
        rankVec[0].deviceInfo.port = HETEROG_CCL_PORT;
        rankVec[0].serverIdx = 0;
        rankVec[0].serverId = "192.168.0.108";
        rankVec[1].rankId = 1;
        rankVec[1].deviceInfo.devicePhyId = 0;
        HcclIpAddress ipAddr2(1711319232);
        rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 109.0.168.192
        rankVec[0].deviceInfo.port = HETEROG_CCL_PORT;
        rankVec[1].serverIdx = 1;
        rankVec[1].serverId = "192.168.0.109";
        rankTable.rankList.assign(rankVec.begin(), rankVec.end());
        rankTable.deviceNum = 2;
        rankTable.serverNum = 2;
    }
    static HcclCommParams params;
    static RankTable_t rankTable;
};

HcclCommParams MPI_ISENDIRECV_TEST::params;
RankTable_t MPI_ISENDIRECV_TEST::rankTable;
std::unique_ptr<LocklessRingMemoryAllocate<HcclMessageInfo>> MPI_ISENDIRECV_TEST::pMsgInfosMem = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<HcclRequestInfo>> MPI_ISENDIRECV_TEST::pReqInfosMem = nullptr;
std::unique_ptr<HeterogMemBlocksManager> MPI_ISENDIRECV_TEST::memBlocksManager = nullptr;
std::unique_ptr<LocklessRingMemoryAllocate<RecvWrInfo>> MPI_ISENDIRECV_TEST::pRecvWrInfosMem = nullptr;

TEST_F(MPI_ISENDIRECV_TEST, st_comm_manager_isend)
{
    int ret;

    hccl::HccdComm* pComm = nullptr;
    pComm = new hccl::HccdComm("test");
    pComm->impl_.reset(new (std::nothrow) HccdImplPml());
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    u64 count = 1;
    s8 buffer;
    u32 rank = 0;
    u32 peerRank = 1;
    u32 tag = 0;

    HcclRequestInfo *request;
    HcclRequest* hcclRequest = reinterpret_cast<HcclRequest *>(&request);
    MOCKER_CPP(&HccdImplPml::Isend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = HcclIsend(&buffer, count, dataType, peerRank, tag, (HcclComm)pComm, hcclRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    delete pComm;
    pComm = nullptr;
}

TEST_F(MPI_ISENDIRECV_TEST, st_comm_manager_improbe)
{
    int ret;

    hccl::HccdComm* pComm = nullptr;
    pComm = new hccl::HccdComm("test");
    pComm->impl_.reset(new (std::nothrow) HccdImplPml());
    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = pComm->impl_->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 peerRank = 1;
    u32 tag = 0;

    int flag = HCCL_IMPROBE_COMPLETED;

    HcclMessageInfo msgInfo;
    msgInfo.commHandle = pComm;
    HcclMessage msg = &msgInfo;

    HcclStatus status;
    status.error = 0;
    status.count = 1;

    MOCKER_CPP(&HccdImplPml::Improbe)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = HcclImprobe(peerRank, tag, (HcclComm)pComm, &flag, &msg, &status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    delete pComm;
    pComm = nullptr;
}

TEST_F(MPI_ISENDIRECV_TEST, st_comm_manager_getCount)
{
    int ret;

    HcclStatus status;
    status.error = 0;
    status.count = 1;
    HcclDataType datatype = HCCL_DATA_TYPE_INT8;
    int count = 0;

    ret = HcclGetCount(&status, datatype, &count);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    status.error = -1;
    ret = HcclGetCount(&status, datatype, &count);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(MPI_ISENDIRECV_TEST, st_comm_manager_testsome)
{
    int ret;

    hccl::HccdComm* pComm = nullptr;
    pComm = new hccl::HccdComm("test");
    pComm->impl_.reset(new (std::nothrow) HccdImplPml());
    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = pComm->impl_->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    HcclRequestInfo requestInfo;
    requestInfo.commHandle = pComm;
    HcclRequest request = &requestInfo;

    int count = 1;
    HcclRequest* requestArray = &request;
    int compCount = 0;
    int compIndices[1];

    HcclStatus status;
    status.error = 0;
    status.count = 1;

    HcclStatus* compStatus = &status;


    MOCKER_CPP(&HccdImplPml::HcclTest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = HcclTestSome(count, requestArray, &compCount, compIndices, compStatus);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    delete pComm;
    pComm = nullptr;
}

TEST_F(MPI_ISENDIRECV_TEST, st_comm_manager_error_testsome)
{
    int ret;

    hccl::HccdComm* pComm = nullptr;
    pComm = new hccl::HccdComm("test");
    pComm->impl_.reset(new (std::nothrow) HccdImplPml());
    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = pComm->impl_->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    HcclRequestInfo requestInfo;
    requestInfo.commHandle = pComm;
    HcclRequest request = &requestInfo;

    int count = 1;
    HcclRequest* requestArray = &request;
    int compCount = 0;
    int compIndices[1];

    HcclStatus status;
    status.error = 0;
    status.count = 1;

    HcclStatus* compStatus = &status;

    MOCKER_CPP(&HccdImplPml::HcclTest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    ret = HcclTestSome(count, requestArray, &compCount, compIndices, compStatus);
    EXPECT_EQ(ret, 1041);
    GlobalMockObject::verify();

    delete pComm;
    pComm = nullptr;
}

HcclRequestInfo* stub_HcclRequestInfo_Alloc()
{
    static HcclRequestInfo requestInfo;
    return &requestInfo;
}

HcclMessageInfo* stub_HcclMessageInfo_Alloc()
{
    static HcclMessageInfo msgInfo;
    return &msgInfo;
}

HcclResult stub_HccdBuildHeterogeneousTransport(HccdImplPml* impl, u32 commId, u32 peerRank, s32 tag, void* &transportHandle)
{
    HcclIpAddress invalidIp;
    if (GetExternalInputHcclIsTcpMode()) {
        static unique_ptr<TransportHeterogEventTcp> link(new (nothrow) TransportHeterogEventTcp("test_collective",
            invalidIp, invalidIp, 18000, 0, 0, TransportResourceInfo()));
        transportHandle = link.get();
    } else {
        static unique_ptr<TransportHeterogRoce> link(new (nothrow) TransportHeterogRoce("test_collective",
            invalidIp, invalidIp, 18000, 0, TransportResourceInfo()));
        transportHandle = link.get();
    }

    return HCCL_SUCCESS;
}

TEST_F(MPI_ISENDIRECV_TEST, st_hccd_impl_roce_send)
{
    SetTcpMode(false);
    MOCKER_CPP(&LocklessRingMemoryAllocate<HcclRequestInfo>::Alloc)
    .stubs()
    .with(any())
    .will(invoke(stub_HcclRequestInfo_Alloc));
    MOCKER_CPP(&HccdImplPml::BuildHeterogeneousTransport)
    .stubs()
    .with(any())
    .will(invoke(stub_HccdBuildHeterogeneousTransport));
    MOCKER_CPP(&TransportHeterog::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));
    MOCKER_CPP(&MrManager::GetKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&MrManager::ReleaseKey)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::SendFlowControl)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtIbvPostSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    int ret;
    std::unique_ptr<HccdImplPml> impl;
    impl.reset(new (std::nothrow) HccdImplPml());
    impl->userRank_ = 0;
    impl->userRankSize_ = 2;
    HcclRequest requestHandle = nullptr;
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    u64 count = 1;
    s8 buffer;
    u32 rank = 0;
    u32 peerRank = 1;
    u32 tag = 0;
    u32 userRequire = 0;

    RankInfo usrRankInfo;
    usrRankInfo.nicIp.push_back(HcclIpAddress(1));
    RankInfo peerRankInfo;
    peerRankInfo.nicIp.push_back(HcclIpAddress(1));
    impl->rankInfoList_.push_back(usrRankInfo);
    impl->rankInfoList_.push_back(peerRankInfo);

    ret = impl->Isend(&buffer, count, dataType, peerRank, tag, requestHandle, userRequire);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_ISENDIRECV_TEST, st_hccd_impl_roce_improbe)
{
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::Init, HcclResult (TransportHeterogRoce::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocklessRingMemoryAllocate<HcclMessageInfo>::Alloc)
    .stubs()
    .with(any())
    .will(invoke(stub_HcclMessageInfo_Alloc));
    MOCKER_CPP(&HccdImplPml::BuildHeterogeneousTransport)
    .stubs()
    .with(any())
    .will(invoke(stub_HccdBuildHeterogeneousTransport));
    MOCKER_CPP(&TransportHeterog::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));
    MOCKER(hrtIbvPollCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtIbvReqNotifyCq)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(transport, &TransportHeterogRoce::ParseTagRqes)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    SetTcpMode(false);
    int ret;
    std::unique_ptr<HccdImplPml> impl;
    impl.reset(new (std::nothrow) HccdImplPml());
    impl->userRankSize_ = 2;
    impl->userRank_ = 0;
    u32 peerRank = 1;

    s32 tag = 0;
    s32 flag = HCCL_IMPROBE_COMPLETED;
    HcclMessageInfo msgInfo;
    HcclMessage msg = &msgInfo;
    HcclStatus status;
    status.error = 0;
    status.count = 1;

    TransportEndPointInfo commRankTagKey(0, peerRank, tag);
    std::unique_lock<SpinMutex> transportMapLock(impl->transportMapSpinMutex_);
    impl->transportStorage_[commRankTagKey] = nullptr;
    transportMapLock.unlock();

    ret = impl->Improbe(peerRank, tag, flag, msg, status);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclEnvelopeSummary envelopInfo;
    TransportEndPointInfo dst(0, impl->userRank_, tag);
    std::unique_lock<std::mutex> Queuelock(TransportHeterogEventRoce::gReceivedEnvelopesMutex);
    TransportHeterogEventRoce::gReceivedEnvelopes[dst].push(envelopInfo);
    Queuelock.unlock();
    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_RECV_REQUEST_MSG]++;

    ret = impl->Improbe(peerRank, tag, flag, msg, status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_ISENDIRECV_TEST, st_hccd_impl_roce_Imrecv)
{
    MOCKER_CPP(&LocklessRingMemoryAllocate<HcclRequestInfo>::Alloc)
    .stubs()
    .with(any())
    .will(invoke(stub_HcclRequestInfo_Alloc));
    MOCKER_CPP(&LocklessRingMemoryAllocate<HcclMessageInfo>::Free)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterog::CheckRecvEnvelope)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogRoce::RegMr)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtIbvPostSend)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    int ret;
    std::unique_ptr<HccdImplPml> impl;
    impl.reset(new (std::nothrow) HccdImplPml());
    impl->userRankSize_ = 2;

    u64 buffer = 0;
    s32 count = 1;
    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> link(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    // unique_ptr<TransportHeterogRoce> link(new (nothrow) TransportHeterogRoce("test_collective", 0, 1, 18000, 0, transportResourceInfo));
    HcclMessageInfo msgHandle;
    msgHandle.transportHandle = link.get();
    msgHandle.envelope.status = 0;
    msgHandle.envelope.envelope.transData.count = 1;
    msgHandle.envelope.envelope.transData.srcBuf = buffer;
    msgHandle.envelope.envelope.transData.dataType = dataType;
    msgHandle.envelope.envelope.protocol = 1;
    HcclMessage msg = &msgHandle;
    HcclRequest request = nullptr;

    ret = impl->Imrecv(reinterpret_cast<void *>(buffer), count, dataType, msg, request);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_ISENDIRECV_TEST, st_hccd_impl_roce_HcclTest)
{
    MOCKER_CPP(&TransportHeterog::ConnectAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclIpAddress invalidIp;
    TransportHeterogRoce transport("test_ta", invalidIp, invalidIp, 18000, 0, transportResourceInfo);
    int ret;
    std::unique_ptr<HccdImplPml> impl;
    impl.reset(new (std::nothrow) HccdImplPml());
    impl->userRankSize_ = 2;
    unique_ptr<TransportHeterogEventRoce> link(new (nothrow) TransportHeterogEventRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));

    s32 flag;
    HcclStatus compState;
    HcclRequestInfo requestInfo;
    requestInfo.transportHandle = link.get();
    requestInfo.transportRequest.epParam.src.rank = 1;
    requestInfo.transportRequest.status = 0;

    requestInfo.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_SEND;
    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_SEND_COMPLETION_MSG] = 0;
    ret = impl->HcclTest(&requestInfo, flag, compState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    requestInfo.transportHandle = link.get();
    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_SEND_COMPLETION_MSG] = 1;
    ret = impl->HcclTest(&requestInfo, flag, compState);

    requestInfo.transportHandle = link.get();
    requestInfo.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_RECV;
    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_RECV_COMPLETION_MSG] = 0;
    ret = impl->HcclTest(&requestInfo, flag, compState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    requestInfo.transportHandle = link.get();
    requestInfo.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_RECV;
    TransportHeterogEventRoce::gCqeCounterPerEvent[HCCL_EVENT_RECV_COMPLETION_MSG] = 1;
    ret = impl->HcclTest(&requestInfo, flag, compState);

    requestInfo.transportHandle = link.get();
    requestInfo.transportRequest.requestType = HcclRequestType::HCCL_REQUEST_INVAIL;
    ret = impl->HcclTest(&requestInfo, flag, compState);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();
}

TEST_F(MPI_ISENDIRECV_TEST, st_hccd_impl_BuildHeterogeneousTransport)
{
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

    SetTcpMode(true);
    std::unique_ptr<HccdImplPml> impl;
    impl.reset(new (std::nothrow) HccdImplPml());
    HccdImplPml *implPtr = impl.get();

    RankInfo usrRankInfo;
    usrRankInfo.nicIp.push_back(HcclIpAddress(1));
    RankInfo peerRankInfo;
    peerRankInfo.nicIp.push_back(HcclIpAddress(1));
    implPtr->rankInfoList_.push_back(usrRankInfo);
    implPtr->rankInfoList_.push_back(peerRankInfo);

    implPtr->ranksPort_.push_back(0);
    implPtr->ranksPort_.push_back(1);
    implPtr->devicePhyId_ = 0;
    void *transportHandle = nullptr;
    implPtr->userRank_ = 0;
    int ret = implPtr->InitRecvMsgAndRequestBuffer();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = implPtr->BuildHeterogeneousTransport(0, 1, 0, transportHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    SetTcpMode(false);
    GlobalMockObject::verify();
}

std::queue<int *> test;
std::mutex testMutex;
LocklessRingMemoryAllocate<int> ringQueue(100);

void AllocFun()
{
    int * temp = ringQueue.Alloc();
    EXPECT_NE(temp, nullptr);
    std::unique_lock<std::mutex> lock(testMutex);
    test.push(temp);
    return;
}

void FreeFun()
{
    while (true) {
        std::unique_lock<std::mutex> lock(testMutex);
        if (!test.empty()) {
            int * temp = test.front();
            test.pop();
            lock.unlock();
            auto ret = ringQueue.Free(temp);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            return;
        }
    }
    return;
}

TEST_F(MPI_ISENDIRECV_TEST, st_lockless_ring_memory_allocate)
{
    ringQueue.Init();
    std::vector<std::thread> threads;
    for (int i = 0; i < 1000; i++) {
        if (i >= 500) {
            threads.push_back(std::move(std::thread(FreeFun)));
        } else {
            threads.push_back(std::move(std::thread(AllocFun)));
        }
    }

    for (auto &iter : threads) {
        if (iter.joinable()) {
            iter.join();
        }
    }
}

TEST_F(MPI_ISENDIRECV_TEST, st_TransportHeterogRoce_LoopStateProcess)
{
    bool completed = true;

    MOCKER_CPP(&TransportHeterogRoce::CheckConsistentFrame)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::GetSocket)
    .stubs()
    .with(any(), any(), any(), any(), outBound(completed))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::SocketSend)
    .stubs()
    .with(any(), any(), any(), any(), outBound(completed))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::SocketRecv)
    .stubs()
    .with(any(), any(), any(), any(), outBound(completed))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::CreatSignalMesg)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::QpConnect)
    .stubs()
    .with(outBound(completed))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::GetQpStatus)
    .stubs()
    .with(outBound(completed))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::ExchangeSignalMesg)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogRoce::FlushSendQueue)
    .stubs()
    .with(outBound(completed))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSocketBatchClose)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    HcclIpAddress invalidIp;
    unique_ptr<TransportHeterogRoce> transport(new (nothrow) TransportHeterogRoce("test_collective", invalidIp, invalidIp, 18000, 0, transportResourceInfo));
    TransportHeterogRoce *transportHandle = transport.get();
    SocketConnectInfoT socketConnectInfo;
    SocketInfoT socketInfo;
    transportHandle->connState_ = ConnState::CONN_STATE_CONNECT_CHECK_SOCKET;
    transportHandle->initSM_.locInitInfo.socketConnInfo.push_back(socketConnectInfo);
    transportHandle->initSM_.locInitInfo.socketConnInfo.push_back(socketConnectInfo);
    transportHandle->initSM_.locInitInfo.socketConnInfo.push_back(socketConnectInfo);
    transportHandle->initSM_.locInitInfo.socketConnInfo.push_back(socketConnectInfo);
    transportHandle->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    transportHandle->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    transportHandle->initSM_.locInitInfo.socketInfo.push_back(socketInfo);
    transportHandle->initSM_.locInitInfo.socketInfo.push_back(socketInfo);

    int ret;
    while(transportHandle->GetState() != ConnState::CONN_STATE_COMPLETE) {
        ret = transportHandle->LoopStateProcess();
    }
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
