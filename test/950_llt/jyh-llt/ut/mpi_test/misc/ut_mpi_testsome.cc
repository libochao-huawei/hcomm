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
#include "hccl_impl.h"
#include "hccl_comm_pub.h"
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
#include "externalinput.h"

using namespace std;
using namespace hccl;

class MPI_TESTSOME_Test : public testing::Test
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
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");
        hrtSetDevice(0);
        ResetInitState();
        DlRaFunction::GetInstance().DlRaFunctionInit();
        ClearHalEvent();
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};

#if 0
TEST_F(MPI_TESTSOME_Test, ut_testSome_success)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    nlohmann::json rankTable =
    {
        {"collective_id", "192.168.3.3-9527-0001"},
        {"master_ip", "192.168.0.11"},
        {"master_port", "18000"},
        {"status", "completed"},
        {"version","1.1"},
        {"mode", "rdma"},
        {"node_list", 
            {
                {
                    {"node_addr", "192.168.0.11"},
                    {"ranks", {
                        {
                            {"rank_id", "0"}
                        }
                    }}
                },
                {
                    {"node_addr", "192.168.0.12"},
                    {"ranks", {
                        {
                            {"rank_id", "1"}
                        }
                    }}
                }
            }
        }
    };

    MOCKER_CPP(&HcclImplBase::IsendWithEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclImplBase::IrecvWithEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalSubmitEvent)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    std::string rankTableM = rankTable.dump();
    HcclComm comm;
    CommAttr attr;
    attr.deviceId = 0;
    attr.mode = HCCL_MODE_NORMAL;
    ret = HcclInitComm(rankTableM.c_str(), rank, &attr, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclDataType dataType = HCCL_DATA_TYPE_INT8;
    u64 count = 1024;
    s8* buffer[2];
    aclrtMallocAttrValue moduleIdValue;
    moduleIdValue.moduleId = HCCL;
    aclrtMallocAttribute attrs{.attr = ACL_RT_MEM_ATTR_MODULE_ID, .value = moduleIdValue};
    aclrtMallocConfig cfg{.attrs = &attrs, .numAttrs = 1};
    aclError rtRet = aclrtMallocHostWithCfg((void**)&buffer[rank], count * sizeof(s8), &cfg);
    EXPECT_EQ(rtRet, HCCL_SUCCESS);
    sal_memset(buffer[rank],  count * sizeof(s8), 0,  count * sizeof(s8));

    ret = HcclRegisterBuffer(buffer[rank], count * sizeof(s8));
    EXPECT_EQ(rtRet, HCCL_SUCCESS);

    u32 peerRank;
    if (rank == 0) {
        peerRank = 1;
    } else {
        peerRank = 0;
    }
    TagAttr tagAttribute = {"ut_cluster_isendrecv_2p_success_normal", 0, 1, 0};
    u32 tag;

    ret = HcclAllocTag(comm, peerRank, &tagAttribute, &tag);
    EXPECT_EQ(rtRet, HCCL_SUCCESS);

    if (rank == 0) {
       void* userHandle;

        ret = HcclIsendWithEvent(buffer[rank], count, dataType, peerRank, tag, comm, userHandle);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        // TODO:  wait send completion event
        HcclComm sendComm = comm;
        u32 sendPeerRank = 1;
        u32 sendTag = 0;

        ret = HcclEventContinue(sendComm, sendPeerRank, sendTag, HCCL_EVENT_SEND_COMPLETION);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        // 提交 send complete entry
        hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(sendComm);
        ret = hcclComm->impl_->NotifyIsenderCompletion(sendPeerRank, sendTag);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        void* sendBuf = nullptr;
        uint64_t sendCount = 0;
        HcclDataType sendDataType = HCCL_DATA_TYPE_RESERVED;
        void* sendUserHandle;
        ret = HcclGetSendCompletion(comm, peerRank, tag, &sendBuf, &sendCount, &sendDataType, &sendUserHandle);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        HcclRequestInfo *request;
        HcclRequest* hcclRequest = reinterpret_cast<HcclRequest *>(&request);
        HcclIsend(buffer[rank], count, dataType, peerRank, tag, comm, hcclRequest);

        g_msnToRequest[0] = request;

        struct ibv_wc wc[1];
        for (int i = 0; i < 1; i++) {
            wc[0].wr_id = reinterpret_cast<u64>(request);
            wc[0].status = IBV_WC_SUCCESS;
            wc[0].opcode = IBV_WC_SEND;
            wc[0].imm_data = i;
        }
        s32 num = 1;

        MOCKER(hrtIbvPollCq)
        .stubs()
        .with(any(), any(), outBoundP(wc, sizeof(wc)), outBound(num))
        .will(returnValue(HCCL_SUCCESS));

        // MOCKER_CPP(&TransportHeterogIbv::MaintainNumOfRecvWqes)
        // .stubs()
        // .with(any())
        // .will(returnValue(HCCL_SUCCESS));

        HcclStatus compStatus[1];
        HcclStatus status;
        status.srcRank = 0;
        status.tag = 0;
        status.error = 0;
        status.cancelled = 0;
        status.count = 0;
        compStatus[0] = status;

        int compCount = 0;
        int compIndices[1] = {0};

        int res = HcclTestSome(1, hcclRequest, &compCount, compIndices, compStatus);
        EXPECT_EQ(res, 0);

        res = HcclTestSome(1, hcclRequest, &compCount, compIndices, compStatus);
        EXPECT_EQ(res, 0);
        GlobalMockObject::verify();
    } else {
        // TODO:  wait recv request event
        HcclComm recvComm = comm;
        u32 recvPeerRank = 0;
        u32 recvTag = 0;

        ret = HcclEventContinue(recvComm, recvPeerRank, recvTag, HCCL_EVENT_RECV_REQUEST);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        // 提交 recv request entry
        hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(recvComm);
        ret = hcclComm->impl_->NotifyIrecverRequest(peerRank, recvTag, count, dataType, nullptr);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        uint64_t recvCount = 0;
        HcclDataType recvDataType = HCCL_DATA_TYPE_RESERVED;
        ret = HcclGetRecvRequest(recvComm, recvPeerRank, recvTag, &recvCount, &recvDataType);

        void* recvUserHandle;
        ret = HcclIrecvWithEvent(buffer[rank], recvCount, recvDataType, recvPeerRank, recvTag, recvComm, recvUserHandle);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        // TODO: wait recv completion event
        HcclComm recvComm_1 = comm;
        u32 recvPeerRank_1 = 0;
        u32 recvTag_1 = 0;

        ret = HcclEventContinue(recvComm_1, recvPeerRank_1, recvTag_1, HCCL_EVENT_RECV_COMPLETION);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        // 提交 recv complete entry
        hccl::hcclComm* hcclComm_1 = static_cast<hccl::hcclComm *>(recvComm_1);
        ret = hcclComm_1->impl_->NotifyIrecverCompletion(recvPeerRank_1, recvTag_1);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        void* recvBuf = nullptr;
        uint64_t recvCount_1 = 0;
        HcclDataType recvDataType_1 = HCCL_DATA_TYPE_RESERVED;
        void* recvUserHandle_1;
        ret = HcclGetRecvData(recvComm_1, recvPeerRank_1, recvTag_1, &recvBuf, &recvCount_1, &recvDataType_1, &recvUserHandle_1);
        EXPECT_EQ(rtRet, HCCL_SUCCESS);

        HcclRequestInfo *request;
        HcclRequest* hcclRequest = reinterpret_cast<HcclRequest *>(&request);
        HcclIsend(buffer[rank], count, dataType, peerRank, tag, comm, hcclRequest);
        request->requestType = HcclRequestType::HCCL_REQUEST_RECV;

        struct ibv_wc wc[1];
        for (int i = 0; i < 1; i++) {
            wc[0].wr_id = reinterpret_cast<u64>(request);
            wc[0].status = IBV_WC_SUCCESS;
            wc[0].opcode = IBV_WC_SEND;
            wc[0].imm_data = i;
        }
        s32 num = 1;

        MOCKER(hrtIbvPollCq)
        .stubs()
        .with(any(), any(), outBoundP(wc, sizeof(wc)), outBound(num))
        .will(returnValue(HCCL_SUCCESS));

        // MOCKER_CPP(&TransportHeterogIbv::MaintainNumOfRecvWqes)
        // .stubs()
        // .with(any())
        // .will(returnValue(HCCL_SUCCESS));

        HcclStatus compStatus[1];
        HcclStatus status;
        status.srcRank = 0;
        status.tag = 0;
        status.error = 0;
        status.cancelled = 0;
        status.count = 0;
        compStatus[0] = status;

        int compCount = 0;
        int compIndices[1] = {0};

        int res = HcclTestSome(1, hcclRequest, &compCount, compIndices, compStatus);
        EXPECT_EQ(res, 0);

        res = HcclTestSome(1, hcclRequest, &compCount, compIndices, compStatus);
        EXPECT_EQ(res, 0);
        GlobalMockObject::verify();
    }

    ret = HcclFreeTag(comm, peerRank, tag);

    ret = HcclUnregisterBuffer(buffer[rank]);
    EXPECT_EQ(rtRet, HCCL_SUCCESS);

    ret = HcclFinalizeComm(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    rtRet = aclrtFreeHost(buffer[rank]);
    EXPECT_EQ(rtRet, HCCL_SUCCESS); 
}
#endif