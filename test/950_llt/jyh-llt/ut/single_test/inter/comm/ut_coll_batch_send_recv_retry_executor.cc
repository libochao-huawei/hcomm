#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "dlra_function.h"

#define private public
#define protected public
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "alg_template_base.h"
#include "coll_batch_send_recv_retry_executor.h"
#undef private
#undef protected
using namespace std;
using namespace hccl;

class CollBatchSendRecvRetryTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        DlRaFunction::GetInstance().DlRaFunctionInit();
        std::cout << "\033[36m--CollBatchSendRecvRetryTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CollBatchSendRecvRetryTest TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

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
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
}

#if 1
TEST_F(CollBatchSendRecvRetryTest, test_BatchSendRecvRetry_normally)
{
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CollBatchSendRecvRetryExecutor* executor = new CollBatchSendRecvRetryExecutor(impl->dispatcher_, topoMatcher);

    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);

    std::vector<HcclSendRecvItem> itemVec(1);
    itemVec[0].remoteRank = 1;
    itemVec[0].buf = inputMem.ptr();
    itemVec[0].count = 1024;
    itemVec[0].dataType = HCCL_DATA_TYPE_FP32;
    itemVec[0].sendRecvType = HcclSendRecvType::HCCL_SEND;

    OpParam opParam;
    opParam.tag = "test";
    opParam.BatchSendRecvDataDes.sendRecvItemsPtr = itemVec.data();
    opParam.BatchSendRecvDataDes.itemNum = 1;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.BatchSendRecvDataDes.curIterNum = 0;
    opParam.BatchSendRecvDataDes.curMode = BatchSendRecvCurMode::SEND;
    u8 isDirectRemoteRank[16] = {0};
    opParam.BatchSendRecvDataDes.isDirectRemoteRank = isDirectRemoteRank;


    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Post)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    DispatcherPub *dispatcher;

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(resourceRequest.streamNum, 2);

    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.opTransportResponse.resize(COMM_LEVEL_RESERVED);
    resourceResponse.opTransportResponse[COMM_COMBINE].resize(2);

    for (int i = 0; i < 2; i++) {
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links.resize(2);
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links[1].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    }
    ret = executor->CreatePairWiseList(opParam.BatchSendRecvDataDes.sendRecvItemsPtr, opParam.BatchSendRecvDataDes.itemNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete executor;
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(CollBatchSendRecvRetryTest, test_BatchSendRecvRetry_test)
{
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CollBatchSendRecvRetryExecutor* executor = new CollBatchSendRecvRetryExecutor(impl->dispatcher_, topoMatcher);

    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);

    std::vector<HcclSendRecvItem> itemVec(1);
    itemVec[0].remoteRank = 1;
    itemVec[0].buf = inputMem.ptr();
    itemVec[0].count = 1024;
    itemVec[0].dataType = HCCL_DATA_TYPE_FP32;
    itemVec[0].sendRecvType = HcclSendRecvType::HCCL_RECV;

    OpParam opParam;
    opParam.tag = "test";
    opParam.BatchSendRecvDataDes.sendRecvItemsPtr = itemVec.data();
    opParam.BatchSendRecvDataDes.itemNum = 1;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.BatchSendRecvDataDes.curIterNum = 0;
    opParam.BatchSendRecvDataDes.curMode = BatchSendRecvCurMode::RECV;
    u8 isDirectRemoteRank[16] = {0};
    opParam.BatchSendRecvDataDes.isDirectRemoteRank = isDirectRemoteRank;


    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Post)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    DispatcherPub *dispatcher;

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(resourceRequest.streamNum, 2);

    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.opTransportResponse.resize(COMM_LEVEL_RESERVED);
    resourceResponse.opTransportResponse[COMM_COMBINE].resize(2);

    for (int i = 0; i < 2; i++) {
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links.resize(2);
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links[1].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    }
    ret = executor->CreatePairWiseList(opParam.BatchSendRecvDataDes.sendRecvItemsPtr, opParam.BatchSendRecvDataDes.itemNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete executor;
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(CollBatchSendRecvRetryTest, test_BatchSendRecvRetry_test_1)
{
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CollBatchSendRecvRetryExecutor* executor = new CollBatchSendRecvRetryExecutor(impl->dispatcher_, topoMatcher);

    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);

    std::vector<HcclSendRecvItem> itemVec(2);
    itemVec[0].remoteRank = 0;
    itemVec[0].buf = inputMem.ptr();
    itemVec[0].count = 1024;
    itemVec[0].dataType = HCCL_DATA_TYPE_FP32;
    itemVec[0].sendRecvType = HcclSendRecvType::HCCL_SEND;

    itemVec[1].remoteRank = 0;
    itemVec[1].buf = inputMem.ptr();
    itemVec[1].count = 1024;
    itemVec[1].dataType = HCCL_DATA_TYPE_FP32;
    itemVec[1].sendRecvType = HcclSendRecvType::HCCL_RECV;

    OpParam opParam;
    opParam.tag = "test";
    opParam.BatchSendRecvDataDes.sendRecvItemsPtr = itemVec.data();
    opParam.BatchSendRecvDataDes.itemNum = 2;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.BatchSendRecvDataDes.curIterNum = 0;
    opParam.BatchSendRecvDataDes.curMode = BatchSendRecvCurMode::SEND_RECV;
    u8 isDirectRemoteRank[16] = {0};
    opParam.BatchSendRecvDataDes.isDirectRemoteRank = isDirectRemoteRank;


    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Post)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    DispatcherPub *dispatcher;

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(resourceRequest.streamNum, 2);

    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.opTransportResponse.resize(COMM_LEVEL_RESERVED);
    resourceResponse.opTransportResponse[COMM_COMBINE].resize(2);

    for (int i = 0; i < 2; i++) {
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links.resize(2);
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links[1].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    }
    ret = executor->CreatePairWiseList(opParam.BatchSendRecvDataDes.sendRecvItemsPtr, opParam.BatchSendRecvDataDes.itemNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete executor;
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(CollBatchSendRecvRetryTest, test_BatchSendRecvRetry_test_2)
{
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    CollBatchSendRecvRetryExecutor* executor = new CollBatchSendRecvRetryExecutor(impl->dispatcher_, topoMatcher);

    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);

    std::vector<HcclSendRecvItem> itemVec(2);
    itemVec[0].remoteRank = 1;
    itemVec[0].buf = inputMem.ptr();
    itemVec[0].count = 1024;
    itemVec[0].dataType = HCCL_DATA_TYPE_FP32;
    itemVec[0].sendRecvType = HcclSendRecvType::HCCL_SEND;

    itemVec[1].remoteRank = 1;
    itemVec[1].buf = inputMem.ptr();
    itemVec[1].count = 1024;
    itemVec[1].dataType = HCCL_DATA_TYPE_FP32;
    itemVec[1].sendRecvType = HcclSendRecvType::HCCL_RECV;

    OpParam opParam;
    opParam.tag = "test";
    opParam.BatchSendRecvDataDes.sendRecvItemsPtr = itemVec.data();
    opParam.BatchSendRecvDataDes.itemNum = 2;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    opParam.BatchSendRecvDataDes.curIterNum = 0;
    opParam.BatchSendRecvDataDes.curMode = BatchSendRecvCurMode::SEND_RECV;
    u8 isDirectRemoteRank[16] = {0};
    opParam.BatchSendRecvDataDes.isDirectRemoteRank = isDirectRemoteRank;


    MOCKER_CPP(&TransportManager::Alloc)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Post)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    DispatcherPub *dispatcher;

    AlgResourceRequest resourceRequest;
    AlgResourceResponse resourceResponse;
    ret = executor->CalcResRequest(opParam, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(resourceRequest.streamNum, 2);

    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.opTransportResponse.resize(COMM_LEVEL_RESERVED);
    resourceResponse.opTransportResponse[COMM_COMBINE].resize(2);

    for (int i = 0; i < 2; i++) {
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links.resize(2);
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links[1].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    }
    ret = executor->CreatePairWiseList(opParam.BatchSendRecvDataDes.sendRecvItemsPtr, opParam.BatchSendRecvDataDes.itemNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete executor;
    GlobalMockObject::verify();
}
#endif

