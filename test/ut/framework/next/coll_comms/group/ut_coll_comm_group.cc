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

// Test 1: initGroupPlanner returns error when rankSize is 0 (commit fix)
TEST_F(TestCollCommGroup, Ut_InitGroupPlanner_When_RankSizeIsZero_Return_HCCL_E_PARA)
{
    std::unique_ptr<CollComm> collComm = std::make_unique<CollComm>(nullptr, 0u, std::string("ut_test"), 
        hccl::ManagerCallbacks{});
    
    collComm->plannerV2 = std::make_shared<hcclKernelPlannerV2>();
    collComm->plannerV2->nTasksP2p = -1;
    
    // Mock GetRankSize to return 0 (error case - testing the fix)
    MOCKER_CPP(&CollComm::GetRankSize, uint32_t(CollComm::*)())
    .stubs()
    .will(returnValue(0u));
    
    hccl::hcclComm hcclCommObj;
    MOCKER_CPP(&hccl::hcclComm::GetCollComm, CollComm*(hccl::hcclComm::*)())
    .stubs()
    .will(returnValue(collComm.get()));
    
    HcclComm comm = reinterpret_cast<HcclComm>(&hcclCommObj);
    
    HcclP2pTask task;
    HcclOpP2pDesc p2pDesc;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 1;
    
    // This should return HCCL_E_PARA because rankSize is 0
    HcclResult ret = HcclGroupAddP2pTaskV2(comm, task, p2pDesc);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// Test 2: HcclGroupAddP2pTaskV2 returns error when comm is nullptr
TEST_F(TestCollCommGroup, Ut_HcclGroupAddP2pTaskV2_When_CommIsNull_Return_HCCL_E_PTR)
{
    HcclP2pTask task;
    HcclOpP2pDesc p2pDesc;
    
    HcclResult ret = HcclGroupAddP2pTaskV2(nullptr, task, p2pDesc);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test 3: HcclGroupAddP2pTaskV2 returns error when plannerV2 is nullptr
TEST_F(TestCollCommGroup, Ut_HcclGroupAddP2pTaskV2_When_PlannerIsNull_Return_HCCL_E_PTR)
{
    std::unique_ptr<CollComm> collComm = std::make_unique<CollComm>(nullptr, 0u, std::string("ut_test"), 
        hccl::ManagerCallbacks{});
    collComm->plannerV2 = nullptr;
    
    hccl::hcclComm hcclCommObj;
    MOCKER_CPP(&hccl::hcclComm::GetCollComm, CollComm*(hccl::hcclComm::*)())
    .stubs()
    .will(returnValue(collComm.get()));
    
    HcclComm comm = reinterpret_cast<HcclComm>(&hcclCommObj);
    
    HcclP2pTask task;
    HcclOpP2pDesc p2pDesc;
    
    HcclResult ret = HcclGroupAddP2pTaskV2(comm, task, p2pDesc);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test 4: MAX_P2P_TASK_NUM limit
TEST_F(TestCollCommGroup, Ut_HcclGroupAddP2pTaskV2_When_ExceedMaxTaskNum_Return_HCCL_E_OOM)
{
    std::unique_ptr<CollComm> collComm = std::make_unique<CollComm>(nullptr, 0u, std::string("ut_test"), 
        hccl::ManagerCallbacks{});
    collComm->plannerV2 = std::make_shared<hcclKernelPlannerV2>();
    collComm->plannerV2->nTasksP2p = 0;
    collComm->plannerV2->rankSize = 2;
    collComm->plannerV2->peers.resize(2);
    
    hccl::hcclComm hcclCommObj;
    MOCKER_CPP(&hccl::hcclComm::GetCollComm, CollComm*(hccl::hcclComm::*)())
    .stubs()
    .will(returnValue(collComm.get()));
    
    MOCKER_CPP(&hccl::hcclComm::SetGroupMode, HcclResult(hccl::hcclComm::*)(bool))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    HcclComm comm = reinterpret_cast<HcclComm>(&hcclCommObj);
    
    // Simulate reaching MAX_P2P_TASK_NUM
    hcclP2pTaskNums = MAX_P2P_TASK_NUM;
    
    HcclP2pTask task;
    HcclOpP2pDesc p2pDesc;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 1;
    
    HcclResult ret = HcclGroupAddP2pTaskV2(comm, task, p2pDesc);
    EXPECT_EQ(ret, HCCL_E_OOM);
    
    hcclP2pTaskNums = 0;
}

// Test 5: plannerV2 rankSize validation - simple test without mocks
TEST_F(TestCollCommGroup, Ut_PlannerV2_RankSize_Initialized_To_Zero)
{
    hcclKernelPlannerV2 planner;
    EXPECT_EQ(planner.rankSize, 0u);
    EXPECT_EQ(planner.nTasksP2p, -1);
}

// Test 6: MAX_P2P_TASK_NUM constant value
TEST_F(TestCollCommGroup, Ut_MaxP2pTaskNum_Value)
{
    EXPECT_EQ(MAX_P2P_TASK_NUM, 2048);
}

// Test 7: HcclP2pTask structure initialization
TEST_F(TestCollCommGroup, Ut_HcclP2pTask_DefaultInit)
{
    HcclP2pTask task;
    EXPECT_EQ(task.args, nullptr);
    EXPECT_EQ(task.argSize, 0u);
}

// Test 8: hcclKernelPlannerV2 peers vector initialization
TEST_F(TestCollCommGroup, Ut_HcclKernelPlannerV2_PeersResize)
{
    hcclKernelPlannerV2 planner;
    planner.peers.resize(4);
    EXPECT_EQ(planner.peers.size(), 4u);
}