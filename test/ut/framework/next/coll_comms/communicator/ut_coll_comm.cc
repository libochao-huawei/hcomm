/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "framework/next/coll_comms/communicator/coll_comm.h"
#undef private

using namespace hccl;
class CollCommTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        std::cout << "CollCommTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CollCommTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CollCommTest SetUp" << std::endl;
        // 创建一个 CollComm 实例用于测试
        void *comm = nullptr;
        uint32_t rankId = 0;
        std::string commName = "test_comm";
        ManagerCallbacks callbacks;
        collComm_ = std::make_unique<CollComm>(comm, rankId, commName, callbacks);
        // 初始化必要的成员变量
        collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_READY;
        collComm_->isCleaned_ = false;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in CollCommTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    std::unique_ptr<CollComm> collComm_;
};

TEST_F(CollCommTest, test_get_comm_status)
{
    // 测试初始状态
    EXPECT_EQ(collComm_->GetCommStatus(), HcclCommStatus::HCCL_COMM_STATUS_READY);
    
    // 测试状态变化
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    EXPECT_EQ(collComm_->GetCommStatus(), HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
    
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    EXPECT_EQ(collComm_->GetCommStatus(), HcclCommStatus::HCCL_COMM_STATUS_INVALID);
}

TEST_F(CollCommTest, test_suspend_success)
{
    // 模拟 myRank_->StopLaunch() 返回成功
    MOCKER_CPP(&MyRank::StopLaunch, HcclResult(MyRank:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试正常情况下的 Suspend
    auto ret = collComm_->Suspend();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(collComm_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
}

TEST_F(CollCommTest, test_suspend_already_suspending)
{
    // 设置为已挂起状态
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    
    // 测试重复挂起的情况
    auto ret = collComm_->Suspend();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(collComm_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
}

TEST_F(CollCommTest, test_clean_success)
{
    // 设置为挂起状态
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    collComm_->isCleaned_ = false;
    
    // 模拟 myRank_->Clean() 返回成功
    MOCKER_CPP(&MyRank::Clean, HcclResult(MyRank:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试正常情况下的 Clean
    auto ret = collComm_->Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(collComm_->isCleaned_);
}

TEST_F(CollCommTest, test_clean_not_suspended)
{
    // 保持为就绪状态（未挂起）
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_READY;
    
    // 测试未挂起时的 Clean
    auto ret = collComm_->Clean();
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    EXPECT_FALSE(collComm_->isCleaned_);
}

TEST_F(CollCommTest, test_clean_already_cleaned)
{
    // 设置为挂起状态且已清理
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    collComm_->isCleaned_ = true;
    
    // 测试重复清理的情况
    auto ret = collComm_->Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(collComm_->isCleaned_);
}

TEST_F(CollCommTest, test_resume_success)
{
    // 设置为挂起状态
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    collComm_->isCleaned_ = true;
    
    // 创建并初始化 myRank_ 模拟对象
    auto myRank = std::make_shared<MyRank>(nullptr, 0, nullptr, ManagerCallbacks(), nullptr);
    collComm_->myRank_ = myRank;
    
    // 模拟 myRank_->Resume() 返回成功
    MOCKER_CPP(&MyRank::Resume, HcclResult(MyRank:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试正常情况下的 Resume
    auto ret = collComm_->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(collComm_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_READY);
    EXPECT_FALSE(collComm_->isCleaned_);
}

TEST_F(CollCommTest, test_resume_invalid_comm)
{
    // 设置为无效状态
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    
    // 测试无效状态下的 Resume
    auto ret = collComm_->Resume();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    EXPECT_EQ(collComm_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_INVALID);
}

TEST_F(CollCommTest, test_resume_not_suspended)
{
    // 保持为就绪状态（未挂起）
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_READY;
    
    // 测试未挂起时的 Resume
    auto ret = collComm_->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(collComm_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_READY);
}

TEST_F(CollCommTest, test_resume_fail)
{
    // 设置为挂起状态
    collComm_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    
    // 创建并初始化 myRank_ 模拟对象
    auto myRank = std::make_shared<MyRank>(nullptr, 0, nullptr, ManagerCallbacks(), nullptr);
    collComm_->myRank_ = myRank;
    
    // 模拟 myRank_->Resume() 返回失败
    MOCKER_CPP(&MyRank::Resume, HcclResult(MyRank:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    
    // 测试 Resume 失败的情况
    auto ret = collComm_->Resume();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    EXPECT_EQ(collComm_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
}
