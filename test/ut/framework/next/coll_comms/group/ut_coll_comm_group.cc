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
#include "../../ut_hcomm_base.h"
#define private public
#include "coll_comm.h"
#include "coll_comm_group.h"
#include "coll_comm_group_utils.h"
#undef private
#include "hccl_comm_pub.h"
#include <memory>

using namespace hccl;

extern thread_local s32 hcclP2pTaskNums;
extern thread_local std::vector<HcclComm> hcclGroupCommListV2;

class TestCollCommGroup : public TestHcommCAdptBase {
public:
    void SetUp() override {
        TestHcommCAdptBase::SetUp();
        hcclP2pTaskNums = 0;
        hcclGroupCommListV2.clear();
    }
    void TearDown() override {
        TestHcommCAdptBase::TearDown();
        hcclP2pTaskNums = 0;
        hcclGroupCommListV2.clear();
        GlobalMockObject::verify();
    }
};

// Test 1: HcclGroupAddP2pTaskV2 returns error when comm is nullptr (basic null check)
TEST_F(TestCollCommGroup, Ut_HcclGroupAddP2pTaskV2_When_CommIsNull_Return_HCCL_E_PTR)
{
    HcclP2pTask task;
    HcclOpP2pDesc p2pDesc;
    
    HcclResult ret = HcclGroupAddP2pTaskV2(nullptr, task, p2pDesc);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test 2: plannerV2 rankSize validation - simple test without mocks
TEST_F(TestCollCommGroup, Ut_PlannerV2_RankSize_Initialized_To_Zero)
{
    hcclKernelPlannerV2 planner;
    EXPECT_EQ(planner.rankSize, 0u);
    EXPECT_EQ(planner.nTasksP2p, -1);
}

// Test 3: MAX_P2P_TASK_NUM constant value
TEST_F(TestCollCommGroup, Ut_MaxP2pTaskNum_Value)
{
    EXPECT_EQ(MAX_P2P_TASK_NUM, 2048);
}

// Test 4: HcclP2pTask structure initialization
TEST_F(TestCollCommGroup, Ut_HcclP2pTask_DefaultInit)
{
    HcclP2pTask task;
    EXPECT_EQ(task.args, nullptr);
    EXPECT_EQ(task.argSize, 0u);
}

// Test 5: hcclKernelPlannerV2 peers vector resize
TEST_F(TestCollCommGroup, Ut_HcclKernelPlannerV2_PeersResize)
{
    hcclKernelPlannerV2 planner;
    planner.peers.resize(4);
    EXPECT_EQ(planner.peers.size(), 4u);
}

// Test 6: HcclP2pPair structure
TEST_F(TestCollCommGroup, Ut_HcclP2pPair_Struct)
{
    HcclP2pPair pair;
    pair.sendRank = 0;
    pair.recvRank = 1;
    EXPECT_EQ(pair.sendRank, 0u);
    EXPECT_EQ(pair.recvRank, 1u);
}

// Test 7: HcclP2pSendRecvQueue default initialization
TEST_F(TestCollCommGroup, Ut_HcclP2pSendRecvQueue_DefaultInit)
{
    HcclP2pSendRecvQueue queue;
    EXPECT_EQ(queue.sendQue.size(), 0u);
    EXPECT_EQ(queue.recvQue.size(), 0u);
}

// Test 8: Verify GetRankSize returns 0 when rankgraph_ is nullptr (testing CollComm behavior)
TEST_F(TestCollCommGroup, Ut_CollComm_GetRankSize_Returns_Zero_When_RankGraph_Null)
{
    std::unique_ptr<CollComm> collComm = std::make_unique<CollComm>(nullptr, 0u, std::string("ut_test"), 
        hccl::ManagerCallbacks{});
    
    // When rankgraph_ is nullptr, GetRankSize should return 0
    uint32_t rankSize = collComm->GetRankSize();
    EXPECT_EQ(rankSize, 0u);
}

// Test 9: Verify plannerV2 can be created and initialized
TEST_F(TestCollCommGroup, Ut_PlannerV2_Creation_And_Init)
{
    auto planner = std::make_shared<hcclKernelPlannerV2>();
    EXPECT_NE(planner, nullptr);
    EXPECT_EQ(planner->nTasksP2p, -1);
    EXPECT_EQ(planner->rankSize, 0u);
    
    // Simulate initialization
    planner->rankSize = 2;
    planner->nTasksP2p = 0;
    planner->peers.resize(2);
    
    EXPECT_EQ(planner->rankSize, 2u);
    EXPECT_EQ(planner->nTasksP2p, 0);
    EXPECT_EQ(planner->peers.size(), 2u);
}