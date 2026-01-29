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
#include <cstdio>

#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include "dlra_function.h"

#define private public
#define protected public
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "alg_template_base_pub.h"
#include "coll_batch_send_recv_executor.h"
#undef private
#undef protected
using namespace std;
using namespace hccl;

class CollBatchSendRecvExecutorTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        DlRaFunction::GetInstance().DlRaFunctionInit();
        std::cout << "\033[36m--CollBatchSendRecvExecutorTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CollAllReduceInterTest TearDown--\033[0m" << std::endl;
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
TEST_F(CollBatchSendRecvExecutorTest, st_BatchSendRecv_host_unfold)
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
    CollBatchSendRecvExecutor* executor = new CollBatchSendRecvExecutor(impl->dispatcher_, topoMatcher);

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
    EXPECT_EQ(resourceRequest.streamNum, 1);

    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, opParam, resourceRequest, resourceResponse);
    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.opTransportResponse.resize(COMM_LEVEL_RESERVED);
    resourceResponse.opTransportResponse[COMM_COMBINE_ORDER].resize(2);

    for (int i = 0; i < 2; i++) {
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links.resize(2);
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links[1].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    }
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(CollBatchSendRecvExecutorTest, test_BatchSendRecv_aicpu_unfold)
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
    CollBatchSendRecvExecutor* executor = new CollBatchSendRecvExecutor(impl->dispatcher_, topoMatcher);
    executor->aicpuUnfoldMode_ = true;

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
    EXPECT_EQ(resourceRequest.streamNum, 1);

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
    executor->aicpuUnfoldMode_ = true;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete executor;
    GlobalMockObject::verify();
}

TEST_F(CollBatchSendRecvExecutorTest, st_aicpu_unfold_SetRetryEnable)
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

    OpParam opParam;
    opParam.tag = "";
    opParam.aicpuUnfoldMode = true;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    ret = implBase->ExecOp(opType, opParam);
    opType = HcclCMDType::HCCL_CMD_ALLTOALL;
    ret = implBase->ExecOpAlltoAll(opType, opParam);
    GlobalMockObject::verify();
}

static void TestConstructParam2(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = 2;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910_93;

    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 1;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 0;
    rankVec[1].serverId = "192.168.0.101";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 1;
}

TEST_F(CollBatchSendRecvExecutorTest, st_execop_aivmode)
{
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam2(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AllocAlgResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::RegisterToHeartBeat, HcclResult(HcclCommunicator::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(StarsCounter)
    .stubs()
    .will(returnValue(HCCL_E_PARA));
    MOCKER_CPP(&HcclCommunicator::UnRegisterDfxInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::RegisterDfxInfo)
    .stubs()
    .will(returnValue(HCCL_E_PARA));
    MOCKER_CPP(&CollAlgOperator::SetNumBlocks)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    OpParam opParam;
    opParam.tag = "";
    opParam.aicpuUnfoldMode = false;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    implBase->SetAivModeConfig(true);
    ret = implBase->ExecOp(opType, opParam);
    GlobalMockObject::verify();
}
#endif

TEST_F(CollBatchSendRecvExecutorTest, st_execop_cache)
{
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam2(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AllocAlgResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::RegisterToHeartBeat, HcclResult(HcclCommunicator::*)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(CollExecutorBase::RunTemplate)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER(StarsCounter)
    .stubs()
    .will(returnValue(HCCL_E_PARA));
    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::UnRegisterDfxInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::RegisterDfxInfo)
    .stubs()
    .will(returnValue(HCCL_E_PARA));
    MOCKER_CPP(&CollAlgOperator::SetNumBlocks)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    OpParam opParam;
    opParam.tag = "";
    opParam.aicpuUnfoldMode = false;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.isCapture = false;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    HcclCacheInfo cacheInfo;
    cacheInfo.newTag = "hccl";
    cacheInfo.resourceArgs.aivTag = 1;
    cacheInfo.opArgs.op = HCCL_REDUCE_SUM;
    ret = implBase->ExecOpCache(opType, opParam, cacheInfo);
    GlobalMockObject::verify();
}

TEST_F(CollBatchSendRecvExecutorTest, st_execop_cache_alltoall)
{
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    HcclResult ret = HCCL_SUCCESS;
    HcclCommParams params;
    RankTable_t rankTable;
    TestConstructParam2(params, rankTable);
    params.deviceType = DevType::DEV_TYPE_910_93;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    MOCKER_CPP(&HcclCommunicator::InitRaResource)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::RegisterDfxInfo)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(StarsCounter)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hccl::ExecuteKernelLaunch)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(StarsCounter)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&HcclCommunicator::UnRegisterDfxInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = implBase->Init(params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    OpParam opParam;
    opParam.tag = "";
    opParam.aicpuUnfoldMode = false;
    opParam.DataDes.count = 1024;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT32;
    opParam.reduceType = HCCL_REDUCE_SUM;
    opParam.isCapture = false;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLTOALL;
    HcclCacheInfo cacheInfo;
    cacheInfo.newTag = "hccl";
    cacheInfo.resourceArgs.aivTag = 1;
    ret = implBase->ExecOpCache(opType, opParam, cacheInfo);
    GlobalMockObject::verify();
}

#if 1
TEST_F(CollBatchSendRecvExecutorTest,  st_BatchSendRecv_create_link_incrementally){
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
    CollBatchSendRecvExecutor* executor = new CollBatchSendRecvExecutor(impl->dispatcher_,topoMatcher);

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
    EXPECT_EQ(resourceRequest.streamNum, 1);

    implBase->AllocAlgResource(opParam.tag, HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, opParam, resourceRequest, resourceResponse);

    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    resourceResponse.opTransportResponse.resize(COMM_LEVEL_RESERVED);
    resourceResponse.opTransportResponse[COMM_COMBINE_ORDER].resize(2);

    for (int i = 0; i < 2; i++) {
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links.resize(2);
        resourceResponse.opTransportResponse[COMM_COMBINE_ORDER][i].links[1].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
        dispatcher, nullptr, machinePara, timeout)));
    }
    bool needLink = false;
    std::set<u32> ranksHasLinked = {0, 1};
    ret = executor->CalcIncreLinkRequest(opParam, ranksHasLinked, resourceRequest, needLink);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    implBase->IncreAllocLink(opParam.tag, opParam, resourceRequest, resourceResponse);

    resourceResponse.cclInputMem = inputMem;
    resourceResponse.cclOutputMem = outputMem;
    ret = executor->Orchestrate(opParam, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete executor;
    GlobalMockObject::verify();
}


TEST_F(CollBatchSendRecvExecutorTest, st_GetPairWiseList_AllSend){
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
    topoMatcher->topoInfo_.userRank = 0;
    topoMatcher->topoInfo_.userRankSize = 8;
    CollBatchSendRecvExecutor* executor = new CollBatchSendRecvExecutor(impl->dispatcher_, topoMatcher);
    char* str = "test";
    HcclSendRecvItemDef data[] = {{HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 7},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 2},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 4},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 5},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 6},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 0},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 3},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 0},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 1}};

    u32 itemNum = 9;
    HcclSendRecvItem *listPtr = data;
    ret = executor->GetPairWiseList(listPtr, itemNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (!executor->sendToSelfDeque_.empty() && !executor->recvFromSelfDeque_.empty()) {
        EXPECT_EQ(executor->sendToSelfDeque_.front()->remoteRank, 0);
        EXPECT_EQ(executor->recvFromSelfDeque_.front()->remoteRank, 0);
    }

    EXPECT_EQ(executor->sendDeque_[0]->remoteRank, 7);
    EXPECT_EQ(executor->sendDeque_[1]->remoteRank, 6);
    EXPECT_EQ(executor->sendDeque_[2]->remoteRank, 5);
    EXPECT_EQ(executor->sendDeque_[3]->remoteRank, 4);
    EXPECT_EQ(executor->sendDeque_[4]->remoteRank, 3);
    EXPECT_EQ(executor->sendDeque_[5]->remoteRank, 2);
    EXPECT_EQ(executor->sendDeque_[6]->remoteRank, 1);
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(CollBatchSendRecvExecutorTest, st_GetPairWiseList_SendRecvNormal){
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
    topoMatcher->topoInfo_.userRank = 5;
    topoMatcher->topoInfo_.userRankSize = 8;
    CollBatchSendRecvExecutor* executor = new CollBatchSendRecvExecutor(impl->dispatcher_, topoMatcher);
    char* str = "test";
    HcclSendRecvItem data[] = {{HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 7},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 6},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 2},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 0},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 4},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 6},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 5},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 3},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 1},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 5}};
    u32 itemNum = 10;
    HcclSendRecvItem *listPtr = data;
    ret = executor->GetPairWiseList(listPtr, itemNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    if (!executor->sendToSelfDeque_.empty() && !executor->recvFromSelfDeque_.empty()) {
        EXPECT_EQ(executor->sendToSelfDeque_.front()->remoteRank, 5);
        EXPECT_EQ(executor->recvFromSelfDeque_.front()->remoteRank, 5);
    }
    EXPECT_EQ(executor->sendDeque_[0]->remoteRank, 3);
    EXPECT_EQ(executor->recvDeque_[0]->remoteRank, 6);
    EXPECT_EQ(executor->sendDeque_[1]->remoteRank, 2);
    EXPECT_EQ(executor->recvDeque_[1]->remoteRank, 0);
    EXPECT_EQ(executor->sendDeque_[2]->remoteRank, 7);
    EXPECT_EQ(executor->recvDeque_[2]->remoteRank, 1);
    EXPECT_EQ(executor->sendDeque_[3]->remoteRank, 6);
    EXPECT_EQ(executor->recvDeque_[3]->remoteRank, 4);
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(CollBatchSendRecvExecutorTest, st_GetPairWiseList_TasksRemoteIdDuplicated){
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
    topoMatcher->topoInfo_.userRank = 3;
    topoMatcher->topoInfo_.userRankSize = 8;
    CollBatchSendRecvExecutor* executor = new CollBatchSendRecvExecutor(impl->dispatcher_, topoMatcher);
    char* str = "test";
    HcclSendRecvItem data[] = {{HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 7},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 6},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 2},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 0},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 4},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 6},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 3, HcclDataType::HCCL_DATA_TYPE_INT8, 4},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 5},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 3, HcclDataType::HCCL_DATA_TYPE_INT8, 5},
                                  {HcclSendRecvType::HCCL_RECV, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 1},
                                  {HcclSendRecvType::HCCL_SEND, static_cast<void*>(str), 4, HcclDataType::HCCL_DATA_TYPE_INT8, 5}};
    u32 itemNum = 11;
    HcclSendRecvItem *listPtr = data;
    ret = executor->GetPairWiseList(listPtr, itemNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(executor->sendDeque_[0]->remoteRank, 2);
    EXPECT_EQ(executor->recvDeque_[0]->remoteRank, 4);
    EXPECT_EQ(executor->sendDeque_[1]->remoteRank, 7);
    EXPECT_EQ(executor->recvDeque_[1]->remoteRank, 4);
    EXPECT_EQ(executor->sendDeque_[2]->remoteRank, 6);
    EXPECT_EQ(executor->recvDeque_[2]->remoteRank, 6);
    EXPECT_EQ(executor->sendDeque_[3]->remoteRank, 5);
    EXPECT_EQ(executor->recvDeque_[3]->remoteRank, 0);
    EXPECT_EQ(executor->sendDeque_[4]->remoteRank, 5);
    EXPECT_EQ(executor->recvDeque_[4]->remoteRank, 1);
    EXPECT_EQ(executor->sendDeque_[5]->remoteRank, 5);

    EXPECT_EQ(executor->recvDeque_[0]->count, 2);
    EXPECT_EQ(executor->recvDeque_[1]->count, 3);
    EXPECT_EQ(executor->sendDeque_[3]->count, 2);
    EXPECT_EQ(executor->sendDeque_[4]->count, 3);
    EXPECT_EQ(executor->sendDeque_[5]->count, 4);
    delete executor;
    GlobalMockObject::verify();
}

TEST_F(CollBatchSendRecvExecutorTest, st_GetPairWiseList_paraError){

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
    topoMatcher->topoInfo_.userRank = 0;
    topoMatcher->topoInfo_.userRankSize = 8;
    CollBatchSendRecvExecutor* executor = new CollBatchSendRecvExecutor(impl->dispatcher_, topoMatcher);
    u32 itemNum = 2;

    char* str = "test";
    ret = executor->GetPairWiseList(nullptr, itemNum);
    EXPECT_EQ(ret, HCCL_E_PTR);

    HcclSendRecvItem dataTypeError[] = {{HcclSendRecvType::HCCL_SEND_RECV_RESERVED, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 7},
                                        {HcclSendRecvType::HCCL_SEND_RECV_RESERVED, static_cast<void*>(str), 2, HcclDataType::HCCL_DATA_TYPE_INT8, 2}};
    HcclSendRecvItem *ptr2 = dataTypeError;
    HcclResult ret2 = executor->GetPairWiseList(ptr2, itemNum);
    EXPECT_EQ(ret2, HCCL_E_PARA);
    delete executor;
    GlobalMockObject::verify();
}
#endif