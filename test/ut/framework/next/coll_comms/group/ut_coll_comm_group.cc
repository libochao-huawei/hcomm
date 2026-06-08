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

// Test 4: HcclAicpuKernelLaunch returns error when args is nullptr but argSize > 0
TEST_F(TestCollCommGroup, Ut_HcclAicpuKernelLaunch_When_ArgsNullButArgSizePositive_Return_HCCL_E_PTR)
{
    hccl::hcclComm hcclCommObj;
    HcclComm comm = reinterpret_cast<HcclComm>(&hcclCommObj);
    
    aclrtStream stream = reinterpret_cast<aclrtStream>(0x1000);
    ThreadHandle threadHandle = 1;
    
    HcclOpDesc opInfo;
    HcclKernelFuncInfo funcInfo;
    
    // args is nullptr but argSize > 0 - should return error
    HcclResult ret = HcclAicpuKernelLaunch(comm, opInfo, funcInfo, nullptr, 100, threadHandle, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test 5: HcclAicpuKernelLaunch returns error when comm is nullptr
TEST_F(TestCollCommGroup, Ut_HcclAicpuKernelLaunch_When_CommNull_Return_HCCL_E_PTR)
{
    aclrtStream stream = reinterpret_cast<aclrtStream>(0x1000);
    ThreadHandle threadHandle = 1;
    
    HcclOpDesc opInfo;
    HcclKernelFuncInfo funcInfo;
    char args[100];
    
    HcclResult ret = HcclAicpuKernelLaunch(nullptr, opInfo, funcInfo, args, 100, threadHandle, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test 6: HcclAicpuKernelLaunch returns error when userStream is nullptr
TEST_F(TestCollCommGroup, Ut_HcclAicpuKernelLaunch_When_UserStreamNull_Return_HCCL_E_PTR)
{
    hccl::hcclComm hcclCommObj;
    HcclComm comm = reinterpret_cast<HcclComm>(&hcclCommObj);
    
    ThreadHandle threadHandle = 1;
    
    HcclOpDesc opInfo;
    HcclKernelFuncInfo funcInfo;
    char args[100];
    
    HcclResult ret = HcclAicpuKernelLaunch(comm, opInfo, funcInfo, args, 100, threadHandle, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test 7: MAX_P2P_TASK_NUM limit
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

// Test 8: Valid GetRankSize initialization
TEST_F(TestCollCommGroup, Ut_InitGroupPlanner_When_RankSizeIsValid_Return_Success)
{
    std::unique_ptr<CollComm> collComm = std::make_unique<CollComm>(nullptr, 0u, std::string("ut_test"), 
        hccl::ManagerCallbacks{});
    
    collComm->plannerV2 = std::make_shared<hcclKernelPlannerV2>();
    collComm->plannerV2->nTasksP2p = -1;
    
    MOCKER_CPP(&CollComm::GetRankSize, uint32_t(CollComm::*)())
    .stubs()
    .will(returnValue(2u));
    
    hccl::hcclComm hcclCommObj;
    MOCKER_CPP(&hccl::hcclComm::GetCollComm, CollComm*(hccl::hcclComm::*)())
    .stubs()
    .will(returnValue(collComm.get()));
    
    MOCKER_CPP(&hccl::hcclComm::GetServerNum, uint32_t(hccl::hcclComm::*)())
    .stubs()
    .will(returnValue(1u));
    
    MOCKER_CPP(&hccl::hcclComm::GetRankLists, std::vector<RankInfo_t>(hccl::hcclComm::*)())
    .stubs()
    .will(returnValue(std::vector<RankInfo_t>{RankInfo_t{}, RankInfo_t{}}));
    
    MOCKER_CPP(&hccl::hcclComm::SetGroupMode, HcclResult(hccl::hcclComm::*)(bool))
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    HcclComm comm = reinterpret_cast<HcclComm>(&hcclCommObj);
    
    HcclP2pTask task;
    HcclOpP2pDesc p2pDesc;
    p2pDesc.cmdType = HcclCMDType::HCCL_CMD_SEND;
    p2pDesc.remoteRank = 1;
    
    HcclResult ret = HcclGroupAddP2pTaskV2(comm, task, p2pDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(collComm->plannerV2->rankSize, 2u);
}