/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <memory>
#include <vector>
#include <map>

#include "hccl/base.h"
#include "hccl/hccl_types.h"
#include "hccl_comm_pub.h"
#include "hccl_group.h"
#include "hccl_group_utils.h"
#include "coll_alg_utils.h"
#include "common.h"

using namespace hccl;

extern thread_local s32 hcclP2pTaskNums;
extern thread_local std::vector<HcclComm> hcclGroupCommList;
constexpr s32 MAX_P2P_TASK_NUM = 2048;

class HcclGroupP2pTest : public testing::Test {
protected:
    void SetUp() override {
        comm.reset(new (std::nothrow) hccl::hcclComm());
        ASSERT_NE(comm, nullptr);
        comm->planner = std::make_shared<hcclKernelPlanner>();
    }
    
    void TearDown() override {
        GlobalMockObject::verify();
        hcclGroupDepth = 0;
        hcclP2pTaskNums = 0;
        hcclGroupCommList.clear();
    }
    
    std::shared_ptr<hccl::hcclComm> comm;
    std::vector<RankInfo> rankList;
};

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_FirstSendTask_Expect_InitAndSuccess)
{
    comm->planner->nTasksP2p = -1;
    comm->planner->rankSize = 0;
    comm->planner->peers.clear();
    
    HcclP2pTask task;
    task.desc.buffer = nullptr;
    task.desc.count = 1024;
    task.desc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    task.desc.remoteRank = 1;
    task.desc.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    
    HcclOpP2pDesc p2pDesc;
    p2pDesc.buffer = nullptr;
    p2pDesc.count = 1024;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 1;
    p2pDesc.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    
    u32 rankSize = 4;
    u32 userRank = 0;
    
    std::vector<RankInfo> localRankList(rankSize);
    for (u32 i = 0; i < rankSize; i++) {
        localRankList[i].userRank = i;
        localRankList[i].localRank = i;
        localRankList[i].serverIdx = 0;
    }
    
    MOCKER_CPP(&hcclComm::GetRankSize)
        .stubs()
        .with(outBound(rankSize))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&hcclComm::GetUserRank)
        .stubs()
        .with(outBound(userRank))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&hcclComm::GetServerNum)
        .stubs()
        .will(returnValue(1u));
    
    MOCKER_CPP(&hcclComm::GetRankLists)
        .stubs()
        .will(returnValue(localRankList));
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), task, p2pDesc);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 1);
    EXPECT_EQ(comm->planner->peers.size(), rankSize);
    EXPECT_EQ(comm->planner->peers[1].sendQue.size(), 1u);
    EXPECT_EQ(hcclGroupCommList.size(), 1u);
}

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_FirstRecvTask_Expect_InitAndSuccess)
{
    comm->planner->nTasksP2p = -1;
    comm->planner->rankSize = 0;
    comm->planner->peers.clear();
    
    HcclP2pTask task;
    task.desc.buffer = nullptr;
    task.desc.count = 2048;
    task.desc.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
    task.desc.remoteRank = 2;
    task.desc.dataType = HcclDataType::HCCL_DATA_TYPE_INT32;
    
    HcclOpP2pDesc p2pDesc;
    p2pDesc.buffer = nullptr;
    p2pDesc.count = 2048;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
    p2pDesc.remoteRank = 2;
    p2pDesc.dataType = HcclDataType::HCCL_DATA_TYPE_INT32;
    
    u32 rankSize = 4;
    u32 userRank = 0;
    
    std::vector<RankInfo> localRankList(rankSize);
    for (u32 i = 0; i < rankSize; i++) {
        localRankList[i].userRank = i;
        localRankList[i].localRank = i;
        localRankList[i].serverIdx = 0;
    }
    
    MOCKER_CPP(&hcclComm::GetRankSize)
        .stubs()
        .with(outBound(rankSize))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&hcclComm::GetUserRank)
        .stubs()
        .with(outBound(userRank))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&hcclComm::GetServerNum)
        .stubs()
        .will(returnValue(1u));
    
    MOCKER_CPP(&hcclComm::GetRankLists)
        .stubs()
        .will(returnValue(localRankList));
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), task, p2pDesc);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 1);
    EXPECT_EQ(comm->planner->peers.size(), rankSize);
    EXPECT_EQ(comm->planner->peers[2].recvQue.size(), 1u);
    EXPECT_EQ(hcclGroupCommList.size(), 1u);
}

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_SendTaskAfterInit_Expect_AddToSendQue)
{
    comm->planner->nTasksP2p = 0;
    comm->planner->rankSize = 4;
    comm->planner->peers.resize(4);
    
    HcclP2pTask task;
    task.desc.buffer = nullptr;
    task.desc.count = 512;
    task.desc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    task.desc.remoteRank = 3;
    task.desc.dataType = HcclDataType::HCCL_DATA_TYPE_FP16;
    
    HcclOpP2pDesc p2pDesc;
    p2pDesc.buffer = nullptr;
    p2pDesc.count = 512;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 3;
    p2pDesc.dataType = HcclDataType::HCCL_DATA_TYPE_FP16;
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), task, p2pDesc);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 1);
    EXPECT_EQ(comm->planner->peers[3].sendQue.size(), 1u);
    EXPECT_EQ(comm->planner->peers[3].recvQue.size(), 0u);
    EXPECT_EQ(hcclGroupCommList.size(), 1u);
}

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_RecvTaskAfterInit_Expect_AddToRecvQue)
{
    comm->planner->nTasksP2p = 0;
    comm->planner->rankSize = 4;
    comm->planner->peers.resize(4);
    
    HcclP2pTask task;
    task.desc.buffer = nullptr;
    task.desc.count = 256;
    task.desc.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
    task.desc.remoteRank = 0;
    task.desc.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    
    HcclOpP2pDesc p2pDesc;
    p2pDesc.buffer = nullptr;
    p2pDesc.count = 256;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
    p2pDesc.remoteRank = 0;
    p2pDesc.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), task, p2pDesc);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 1);
    EXPECT_EQ(comm->planner->peers[0].recvQue.size(), 1u);
    EXPECT_EQ(comm->planner->peers[0].sendQue.size(), 0u);
    EXPECT_EQ(hcclGroupCommList.size(), 1u);
}

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_TaskNumReachLimit_Expect_ReturnError)
{
    comm->planner->nTasksP2p = 0;
    comm->planner->rankSize = 4;
    comm->planner->peers.resize(4);
    
    HcclP2pTask task;
    task.desc.buffer = nullptr;
    task.desc.count = 128;
    task.desc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    task.desc.remoteRank = 1;
    
    HcclOpP2pDesc p2pDesc;
    p2pDesc.buffer = nullptr;
    p2pDesc.count = 128;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 1;
    
    hcclP2pTaskNums = MAX_P2P_TASK_NUM;
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), task, p2pDesc);
    EXPECT_EQ(result, HCCL_E_OOM);
    EXPECT_EQ(comm->planner->nTasksP2p, 0);
    EXPECT_EQ(comm->planner->peers[1].sendQue.size(), 0u);
}

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_AddMultipleTasksToSamePeer_Expect_QueSizeIncrease)
{
    comm->planner->nTasksP2p = 0;
    comm->planner->rankSize = 4;
    comm->planner->peers.resize(4);
    
    HcclP2pTask sendTask1;
    sendTask1.desc.buffer = nullptr;
    sendTask1.desc.count = 100;
    sendTask1.desc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    sendTask1.desc.remoteRank = 2;
    
    HcclOpP2pDesc sendP2pDesc1;
    sendP2pDesc1.buffer = nullptr;
    sendP2pDesc1.count = 100;
    sendP2pDesc1.cmdType = HcclCMDType::HCCL_CMD_SEND;
    sendP2pDesc1.remoteRank = 2;
    
    HcclP2pTask sendTask2;
    sendTask2.desc.buffer = nullptr;
    sendTask2.desc.count = 200;
    sendTask2.desc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    sendTask2.desc.remoteRank = 2;
    
    HcclOpP2pDesc sendP2pDesc2;
    sendP2pDesc2.buffer = nullptr;
    sendP2pDesc2.count = 200;
    sendP2pDesc2.cmdType = HcclCMDType::HCCL_CMD_SEND;
    sendP2pDesc2.remoteRank = 2;
    
    HcclP2pTask recvTask;
    recvTask.desc.buffer = nullptr;
    recvTask.desc.count = 300;
    recvTask.desc.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
    recvTask.desc.remoteRank = 2;
    
    HcclOpP2pDesc recvP2pDesc;
    recvP2pDesc.buffer = nullptr;
    recvP2pDesc.count = 300;
    recvP2pDesc.cmdType = HcclCMDType::HCCL_CMD_RECEIVE;
    recvP2pDesc.remoteRank = 2;
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result1 = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), sendTask1, sendP2pDesc1);
    EXPECT_EQ(result1, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 1);
    EXPECT_EQ(comm->planner->peers[2].sendQue.size(), 1u);
    
    HcclResult result2 = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), sendTask2, sendP2pDesc2);
    EXPECT_EQ(result2, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 2);
    EXPECT_EQ(comm->planner->peers[2].sendQue.size(), 2u);
    
    HcclResult result3 = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), recvTask, recvP2pDesc);
    EXPECT_EQ(result3, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 3);
    EXPECT_EQ(comm->planner->peers[2].recvQue.size(), 1u);
    EXPECT_EQ(comm->planner->peers[2].sendQue.size(), 2u);
    EXPECT_EQ(hcclGroupCommList.size(), 1u);
}

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_CommAlreadyInList_Expect_NoDuplicateAdd)
{
    comm->planner->nTasksP2p = 0;
    comm->planner->rankSize = 4;
    comm->planner->peers.resize(4);
    
    hcclGroupCommList.push_back(static_cast<HcclComm>(comm.get()));
    EXPECT_EQ(hcclGroupCommList.size(), 1u);
    
    HcclP2pTask task;
    task.desc.buffer = nullptr;
    task.desc.count = 1024;
    task.desc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    task.desc.remoteRank = 1;
    
    HcclOpP2pDesc p2pDesc;
    p2pDesc.buffer = nullptr;
    p2pDesc.count = 1024;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 1;
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), task, p2pDesc);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 1);
    EXPECT_EQ(hcclGroupCommList.size(), 1u);
}

TEST_F(HcclGroupP2pTest, Ut_HcclGroupAddP2pTask_When_RankSize16GroupSize1_Expect_InitAndSuccess)
{
    comm->planner->nTasksP2p = -1;
    comm->planner->rankSize = 0;
    comm->planner->peers.clear();
    
    HcclP2pTask task;
    task.desc.buffer = nullptr;
    task.desc.count = 1024;
    task.desc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    task.desc.remoteRank = 5;
    task.desc.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    
    HcclOpP2pDesc p2pDesc;
    p2pDesc.buffer = nullptr;
    p2pDesc.count = 1024;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 5;
    p2pDesc.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    
    u32 rankSize = 16;
    u32 userRank = 0;
    u32 serverNum = 16;
    
    std::vector<RankInfo> localRankList(rankSize);
    for (u32 i = 0; i < rankSize; i++) {
        localRankList[i].userRank = i;
        localRankList[i].localRank = 0;
        localRankList[i].serverIdx = i;
    }

    std::vector<std::pair<u32, u32>> p2pSche = {
        {0, 0}, {1, 15}, {3, 13}, {6, 10}, 
        {10, 6}, {15, 1}, {5, 11}, {12, 4},
        {4, 12}, {13, 3}, {7, 9}, {2, 14},
        {14, 2}, {11, 5}, {9, 7}, {8, 8}    
    };
    
    MOCKER_CPP(&hcclComm::GetRankSize)
        .stubs()
        .with(outBound(rankSize))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&hcclComm::GetUserRank)
        .stubs()
        .with(outBound(userRank))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER_CPP(&hcclComm::GetServerNum)
        .stubs()
        .will(returnValue(serverNum));
    
    MOCKER_CPP(&hcclComm::GetRankLists)
        .stubs()
        .will(returnValue(localRankList));
    
    MOCKER_CPP(&hcclComm::SetGroupMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcclGroupAddP2pTask(static_cast<HcclComm>(comm.get()), task, p2pDesc);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(comm->planner->nTasksP2p, 1);

    for (u32 j = 0; j < rankSize; j++) {
        EXPECT_EQ(comm->planner->p2pSchedule[j].sendRank, p2pSche[j].first);
        EXPECT_EQ(comm->planner->p2pSchedule[j].recvRank, p2pSche[j].second);
    }
}
