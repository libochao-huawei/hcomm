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
#include "next/coll_comms/communicator/coll_comm.h"
#include "next/coll_comms/rank/my_rank.h"
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
        // create simple callbacks
        ManagerCallbacks callbacks;
        callbacks.getAicpuCommState = []() { return false; };
        callbacks.setAicpuCommState = [](bool) {};
        callbacks.kernelLaunchAicpuCommInit = []() { return HCCL_SUCCESS; };

        // construct CollComm using nullptr communicator and rank 0
        coll_ = std::make_unique<CollComm>(nullptr, 0u, std::string("ut_test"), callbacks);
        // ensure default state
        coll_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
        coll_->isCleaned_ = false;
        std::cout << "A Test case in CollCommTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in CollCommTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    std::unique_ptr<CollComm> coll_;
};

TEST_F(CollCommTest, test_get_comm_status_initial_and_after_change)
{
    EXPECT_EQ(coll_->GetCommStatus(), HcclCommStatus::HCCL_COMM_STATUS_INVALID);

    coll_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_READY;
    EXPECT_EQ(coll_->GetCommStatus(), HcclCommStatus::HCCL_COMM_STATUS_READY);
}

TEST_F(CollCommTest, test_suspend_success_and_idempotent)
{
    // mock MyRank::StopLaunch to return success
    MOCKER_CPP(&MyRank::StopLaunch, HcclResult(MyRank:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    // attach a MyRank instance (can be real or mocked; method is mocked above)
    aclrtBinHandle binHandle;
    coll_->myRank_ = std::make_shared<MyRank>(binHandle, 0, coll_->GetCommConfig(), ManagerCallbacks(), nullptr);

    auto ret = coll_->Suspend();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(coll_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);

    // calling Suspend again when already suspending should return success without error
    ret = coll_->Suspend();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CollCommTest, test_clean_fail_not_suspending)
{
    // when not suspending, Clean should return not support
    coll_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_READY;
    auto ret = coll_->Clean();
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(CollCommTest, test_clean_success_and_idempotent)
{
    // prepare for cleaning: put into suspending state
    coll_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    coll_->isCleaned_ = false;

    // mock MyRank::Clean to return success
    MOCKER_CPP(&MyRank::Clean, HcclResult(MyRank:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    // attach a MyRank instance
    aclrtBinHandle binHandle;
    coll_->myRank_ = std::make_shared<MyRank>(binHandle, 0, coll_->GetCommConfig(), ManagerCallbacks(), nullptr);

    auto ret = coll_->Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(coll_->isCleaned_);

    // calling Clean again should be idempotent and return success
    ret = coll_->Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CollCommTest, test_resume_fail_invalid_and_resume_success)
{
    // Resume when commStatus_ is INVALID should return internal error
    coll_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    auto ret = coll_->Resume();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    // Now test successful resume from SUSPENDING
    coll_->commStatus_ = HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING;
    coll_->isCleaned_ = true;

    // mock MyRank::Resume to return success
    MOCKER_CPP(&MyRank::Resume, HcclResult(MyRank:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    // attach a MyRank instance
    aclrtBinHandle binHandle;
    coll_->myRank_ = std::make_shared<MyRank>(binHandle, 0, coll_->GetCommConfig(), ManagerCallbacks(), nullptr);

    ret = coll_->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(coll_->commStatus_, HcclCommStatus::HCCL_COMM_STATUS_READY);
    EXPECT_FALSE(coll_->isCleaned_);
}
