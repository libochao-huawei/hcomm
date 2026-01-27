#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dlra_function.h"

#define private public
#define protected public
#include "hccl_communicator.h"
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
#include "transport_heterog_ibv_pub.h"
#undef protected
#undef private

#include <hccl/hccl.h>
#include <hccl/hccl_ex.h>
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../inc/hccl/base.h"
#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "callback_thread_manager.h"

using namespace std;
using namespace hccl;

class MPI_TCP_SENDRECV_TEST : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_TCP_SENDRECV_TEST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_TCP_SENDRECV_TEST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name = std::to_string(call_cnt++) + "_" + __PRETTY_FUNCTION__;
         DlRaFunction::GetInstance().DlRaFunctionInit();
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "MPI_TCP_SENDRECV_TEST");

        HcclResult ret;
        string commId = "heterog_comm ";
        HcclCommParams params;
        memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
        params.rank = 0;
        params.totalRanks = 2;
        params.isHeterogComm = true;
        params.logicDevId = 0;

        RankTable_t rankTable;
        rankTable.collectiveId = "192.168.0.101-8000-8001";
        vector<RankInfo_t> rankVec(2);
        rankVec[0].rankId = 0;
        rankVec[0].deviceInfo.devicePhyId = 0;
        rankVec[0].deviceInfo.deviceIp.push_back(1694542016); // 101.0.168.192
        rankVec[0].serverIdx = 0;
        rankVec[0].serverId = "192.168.0.101";
        rankVec[1].rankId = 1;
        rankVec[1].deviceInfo.devicePhyId = 0;
        rankVec[1].deviceInfo.deviceIp.push_back(1711319232); // 101.0.168.192
        rankVec[1].serverIdx = 1;
        rankVec[1].serverId = "192.168.0.102";
        rankTable.rankList.assign(rankVec.begin(), rankVec.end());

        // unique_ptr<hcclComm> g_comm = nullptr;
        g_comm.reset(new (std::nothrow) hcclComm());
        CommConfig commConfig("hccl_world_group");
        ret = g_comm->init(params, commConfig, rankTable);

        // std::unique_ptr<Dispatcher> dispatcher;
        // string collectiveId = "192.168.3.3-9527-0001";
        // (g_comm->impl_)->Init(params, rankTable);
        // (g_comm->impl_)->commFactory_.reset(new (std::nothrow) CommFactory(collectiveId, 0, 2, dispatcher));
        // (g_comm->impl_)->ranksPort_.push_back(1);
        // (g_comm->impl_)->userRank_ = 0;
        // (g_comm->impl_)->userRankSize_ = 1;
        // (g_comm->impl_)->collectiveId_ = collectiveId;
        // (g_comm->impl_)->pReqInfosMem_.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclRequestInfo>(4096));
        // (g_comm->impl_)->pMsgInfosMem_.reset(new (std::nothrow) LocklessRingMemoryAllocate<HcclMessageInfo>(4096));
        // (g_comm->impl_)->pReqInfosMem_->Init();
        // (g_comm->impl_)->pMsgInfosMem_->Init();

        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }

public:
    unique_ptr<hcclComm> g_comm;
};

TEST_F(MPI_TCP_SENDRECV_TEST, ut_mpi_tcp_send_link)
{
    SetTcpMode(true);
    HcclResult ret;
    string data = "MPI_TCP_SENDRECV_TEST"; 
    string *buffer = &data;
    u32 peerRank = 0;
    u32 tag = 0;
    void *request;
    

    MOCKER_CPP(&TcpSendThreadPool::AddSendTask)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogIbv::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));

    std::string commPair = std::to_string(peerRank) + "_" + std::to_string((g_comm->impl_)->userRank_);
    unique_ptr<TransportHeterogIbv> link(new (nothrow) TransportHeterogIbv("192.168.3.3-9527-0001",
        0, 0, 0, nullptr, nullptr, &(g_comm->impl_)));
    CommRankTagKey commRankTagInfo(0, peerRank, tag);
    std::unique_lock<std::mutex> lock(g_transportStorageMutex);
    g_commRankTagMap[commRankTagInfo].hcclHandle = link.get();
    lock.unlock();

    ret = (g_comm->impl_)->Isend(buffer, sizeof(data), HCCL_DATA_TYPE_INT8, peerRank, tag, request);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    (g_comm->impl_)->pReqInfosMem_->Free(reinterpret_cast<HcclRequestInfo *>(request));
    std::unique_lock<std::mutex> endlock(g_transportStorageMutex);
    g_commRankTagMap.erase(g_commRankTagMap.begin(), g_commRankTagMap.end());
    SetTcpMode(false);
    GlobalMockObject::verify();
}

TEST_F(MPI_TCP_SENDRECV_TEST, ut_mpi_tcp_improbe)
{
    SetTcpMode(true);
    HcclResult ret;
    u32 commId = 0;
    u32 flag = 0;
    u32 peerRank = 0;
    u32 tag = 0;
    string collectiveId = "192.168.3.3-9527-0001";
    HcclMessage* msg;
    HcclStatus* status;

    MOCKER_CPP(&HcclImplBase::parsesEnvelopInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogIbv::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));

    unique_ptr<TransportHeterogIbv> link(new (nothrow) TransportHeterogIbv("192.168.3.3-9527-0001",
        0, 0, 0, nullptr, nullptr, &(g_comm->impl_)));
    std::string commPair = std::to_string(peerRank) + "_" + std::to_string((g_comm->impl_)->userRank_);
    CommRankTagKey commRankTagInfo(0, peerRank, tag);
    std::unique_lock<std::mutex> lock(g_transportStorageMutex);
    g_commRankTagMap[commRankTagInfo].hcclHandle = link.get();
    lock.unlock();
    
    ret = (g_comm->impl_)->Improbe(peerRank, tag, flag, msg, status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    std::unique_lock<std::mutex> endlock(g_transportStorageMutex);
    g_commRankTagMap.erase(g_commRankTagMap.begin(), g_commRankTagMap.end());
    SetTcpMode(false);
}

TEST_F(MPI_TCP_SENDRECV_TEST, ut_mpi_tcp_imrecv)
{
    SetTcpMode(true);
    u32 commId = 0;
    u32 peerRank = 0;
    u32 tag = 0;
    string data = "MPI_TCP_SENDRECV_TEST"; 
    string *buffer = &data;
    void *request = nullptr;
    void **requestPtr = &request;

    EnvelopeStatusInfo enveLope;
    enveLope.envelope.srcAddr = reinterpret_cast<u64>(buffer);
    HcclMessageInfo* msgInfo = (g_comm->impl_)->pMsgInfosMem_->Alloc();
    msgInfo->srcRank = peerRank;
    msgInfo->tag = tag;
    msgInfo->comm = nullptr;
    msgInfo->envelope = enveLope;
    void* msg = msgInfo;

    unique_ptr<TransportHeterogIbv> link(new (nothrow) TransportHeterogIbv("192.168.3.3-9527-0001",
        0, 0, 0, nullptr, nullptr, &(g_comm->impl_)));
    CommRankTagKey commRankTagInfo(commId, peerRank, tag);
    CommRankTagHash channel;
    std::unique_lock<std::mutex> lock(g_transportStorageMutex);
    g_commRankTagMap[commRankTagInfo] = channel;
    g_commRankTagMap[commRankTagInfo].hcclHandle = link.get();
    lock.unlock();

    MOCKER_CPP(&TcpRecvTask::SetRecvEntry)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtEpollCtlMod)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogIbv::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));

    (g_comm->impl_)->Imrecv(buffer, sizeof(data), HCCL_DATA_TYPE_INT8, msg, requestPtr);
    (g_comm->impl_)->pReqInfosMem_->Free(reinterpret_cast<HcclRequestInfo *>(request));
    GlobalMockObject::verify();
    std::unique_lock<std::mutex> endlock(g_transportStorageMutex);
    g_commRankTagMap.erase(g_commRankTagMap.begin(), g_commRankTagMap.end());
    SetTcpMode(false);
}

TEST_F(MPI_TCP_SENDRECV_TEST, ut_mpi_tcp_TcpfdHandleCheck)
{
    HcclResult ret;

    CommRankTagKey commRankTagInfo(0, 0, 0);
    TransportHeterogIbv link("192.168.3.3-9527-0001", 0, 0, 0, nullptr, nullptr, &(g_comm->impl_));
    CommRankTagHash channel;
    channel.hcclHandle = nullptr;
    std::unique_lock<std::mutex> firstLock(g_transportStorageMutex);
    g_commRankTagMap[commRankTagInfo] = channel;
    firstLock.unlock();

    ret = (g_comm->impl_)->TcpfdHandleCheck(commRankTagInfo, reinterpret_cast<void *>(&link));
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportHeterogIbv newLink("192.168.3.3-9527-0001", 0, 0, 0, nullptr, nullptr, &(g_comm->impl_));
    std::unique_lock<std::mutex> secondLock(g_transportStorageMutex);
    g_commRankTagMap[commRankTagInfo].hcclHandle = reinterpret_cast<void *>(&newLink);
    secondLock.unlock();

    ret = (g_comm->impl_)->TcpfdHandleCheck(commRankTagInfo, reinterpret_cast<void *>(&link));
    EXPECT_EQ(ret, HCCL_E_PARA);
    std::unique_lock<std::mutex> endlock(g_transportStorageMutex);
    g_commRankTagMap.erase(g_commRankTagMap.begin(), g_commRankTagMap.end());
}

TEST_F(MPI_TCP_SENDRECV_TEST, ut_mpi_tcp_tcpEschedFinishProcess)
{
    SetTcpMode(true);

    MOCKER(halEschedSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(1));

    MOCKER_CPP(&TransportHeterogIbv::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));

    unsigned int devId = 0;
    unsigned int grpId = 0;
    unsigned int eventId = 0;
    unsigned int subeventId = 0;
    gCompCounterEvent[eventId].counter = 1;

    EschedFinishCallback(devId, grpId, 1, subeventId);
    EschedFinishCallback(devId, grpId, eventId, subeventId);
    SetTcpMode(false);
    GlobalMockObject::verify();
}

TEST_F(MPI_TCP_SENDRECV_TEST, ut_mpi_tcp_sendWork)
{    
    HcclResult ret;

    TcpSendThreadPool tcpThreadPool(&(g_comm->impl_), 0);
    TransportHeterogIbv link("192.168.3.3-9527-0001", 0, 0, 0, nullptr, nullptr, &(g_comm->impl_));

    HcclRequestInfo request;
    SREntry entry;
    int data = 32;
    entry.buffer = &data;
    entry.userHandle = &request;
    entry.hcclHandle = &link;
    gCompCounterEvent[HCCL_EVENT_SEND_COMPLETION_MSG].flag.clear();

    MOCKER_CPP(&TransportHeterogIbv::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));

    MOCKER(halEschedSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(1));

    ret = tcpThreadPool.SendWork(entry);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_TCP_SENDRECV_TEST, ut_mpi_tcp_TcpSearchRequestStatus)
{
    MOCKER_CPP(&TransportHeterogIbv::GetState)
    .stubs()
    .with(any())
    .will(returnValue(ConnState::CONN_STATE_COMPLETE));

    HcclResult ret;
    uint32_t compCount = 0;
    HcclStatus compState;

    gCompCounterEvent[HCCL_EVENT_SEND_COMPLETION_MSG].counter++;
    gCompCounterEvent[HCCL_EVENT_RECV_COMPLETION_MSG].counter++;

    TransportHeterogIbv link("192.168.3.3-9527-0001", 0, 0, 0, nullptr, nullptr, &(g_comm->impl_));

    HcclRequestInfo *request = (g_comm->impl_)->pReqInfosMem_->Alloc();
    request->requestType = HcclRequestType::HCCL_REQUEST_SEND;
    request->status = 0;
    request->transportHandle = &link;
    ret = (g_comm->impl_)->TcpSearchRequestStatus(request, compCount, compState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    request = (g_comm->impl_)->pReqInfosMem_->Alloc();
    request->requestType = HcclRequestType::HCCL_REQUEST_RECV;
    request->status = 0;
    request->transportHandle = &link;
    ret = (g_comm->impl_)->TcpSearchRequestStatus(request, compCount, compState);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    request = (g_comm->impl_)->pReqInfosMem_->Alloc();
    request->requestType = HcclRequestType::HCCL_REQUEST_INVAIL;
    request->status = 0;
    request->transportHandle = &link;
    ret = (g_comm->impl_)->TcpSearchRequestStatus(request, compCount, compState);
    EXPECT_EQ(ret, HCCL_E_PARA);
    (g_comm->impl_)->pReqInfosMem_->Free(request);
    GlobalMockObject::verify();
}
    GlobalMockObject::verify();
}
