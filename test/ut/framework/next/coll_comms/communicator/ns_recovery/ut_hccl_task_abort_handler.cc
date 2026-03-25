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
#include "framework/next/coll_comms/communicator/ns_recovery/task_abort_handler.h"
#undef private

using namespace hccl;
class HcclTaskAbortHandlerTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        std::cout << "HcclTaskAbortHandlerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HcclTaskAbortHandlerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        HcclTaskAbortHandler::GetInstance().commVector_.clear();
        std::cout << "A Test case in HcclTaskAbortHandlerTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in HcclTaskAbortHandlerTest TearDown" << std::endl;
        HcclTaskAbortHandler::GetInstance().commVector_.clear();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclTaskAbortHandlerTest, test_register_and_unregister)
{
    // 构造一个 CollComm 指针（这里使用 nullptr 模拟）
    CollComm *comm1 = nullptr;
    CollComm *comm2 = nullptr;
    
    // 测试注册功能
    auto ret = HcclTaskAbortHandler::GetInstance().Register(comm1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(HcclTaskAbortHandler::GetInstance().commVector_.size(), 1U);
    
    // 测试再次注册
    ret = HcclTaskAbortHandler::GetInstance().Register(comm2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(HcclTaskAbortHandler::GetInstance().commVector_.size(), 2U);
    
    // 测试注销存在的 communicator
    ret = HcclTaskAbortHandler::GetInstance().UnRegister(comm1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(HcclTaskAbortHandler::GetInstance().commVector_.size(), 1U);
    
    // 测试注销不存在的 communicator
    CollComm *comm3 = nullptr;
    ret = HcclTaskAbortHandler::GetInstance().UnRegister(comm3);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(HcclTaskAbortHandler::GetInstance().commVector_.size(), 1U);
    
    // 测试注销剩余的 communicator
    ret = HcclTaskAbortHandler::GetInstance().UnRegister(comm2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(HcclTaskAbortHandler::GetInstance().commVector_.size(), 0U);
}

TEST_F(HcclTaskAbortHandlerTest, test_process_task_abort_pre_success)
{
    // 构造测试数据
    std::vector<CollComm *> commVector;
    CollComm *comm = nullptr;
    commVector.push_back(comm);
    std::chrono::seconds timeout(30);
    
    // 模拟 Suspend 方法返回成功
    MOCKER_CPP(&CollComm::Suspend, HcclResult(CollComm:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试带超时的情况
    auto result = ProcessTaskAbortPre(commVector, timeout);
    EXPECT_EQ(result, static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS));
    
    // 测试不带超时的情况
    timeout = std::chrono::seconds(0);
    result = ProcessTaskAbortPre(commVector, timeout);
    EXPECT_EQ(result, static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS));
}

TEST_F(HcclTaskAbortHandlerTest, test_process_task_abort_pre_fail)
{
    // 构造测试数据
    std::vector<CollComm *> commVector;
    CollComm *comm = nullptr;
    commVector.push_back(comm);
    std::chrono::seconds timeout(30);
    
    // 模拟 Suspend 方法返回失败
    MOCKER_CPP(&CollComm::Suspend, HcclResult(CollComm:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    
    // 测试失败情况
    auto result = ProcessTaskAbortPre(commVector, timeout);
    EXPECT_EQ(result, static_cast<int>(TaskAbortResult::TASK_ABORT_FAIL));
}

TEST_F(HcclTaskAbortHandlerTest, test_process_task_abort_post_success)
{
    // 构造测试数据
    std::vector<CollComm *> commVector;
    CollComm *comm = nullptr;
    commVector.push_back(comm);
    int32_t deviceLogicId = 0;
    std::chrono::seconds timeout(30);
    
    // 模拟相关方法
    MOCKER(hcomm::CcuSetTaskKill).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hcomm::CcuSetTaskKillDone).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollComm::Clean, HcclResult(CollComm:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试带超时的情况
    auto result = ProcessTaskAbortPost(commVector, deviceLogicId, timeout);
    EXPECT_EQ(result, static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS));
    
    // 测试不带超时的情况
    timeout = std::chrono::seconds(0);
    result = ProcessTaskAbortPost(commVector, deviceLogicId, timeout);
    EXPECT_EQ(result, static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS));
}

TEST_F(HcclTaskAbortHandlerTest, test_process_task_abort_post_fail)
{
    // 构造测试数据
    std::vector<CollComm *> commVector;
    CollComm *comm = nullptr;
    commVector.push_back(comm);
    int32_t deviceLogicId = 0;
    std::chrono::seconds timeout(30);
    
    // 模拟相关方法
    MOCKER(hcomm::CcuSetTaskKill).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hcomm::CcuSetTaskKillDone).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollComm::Clean, HcclResult(CollComm:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    
    // 测试失败情况
    auto result = ProcessTaskAbortPost(commVector, deviceLogicId, timeout);
    EXPECT_EQ(result, static_cast<int>(TaskAbortResult::TASK_ABORT_FAIL));
}

TEST_F(HcclTaskAbortHandlerTest, test_process_task_abort_handle_callback_pre)
{
    // 构造测试数据
    int32_t deviceLogicId = 0;
    aclrtDeviceTaskAbortStage stage = aclrtDeviceTaskAbortStage::ACL_RT_DEVICE_TASK_ABORT_PRE;
    uint32_t timeout = 30U;
    
    // 准备 communicator
    CollComm *comm = nullptr;
    HcclTaskAbortHandler::GetInstance().Register(comm);
    void* args = reinterpret_cast<void*>(&HcclTaskAbortHandler::GetInstance().commVector_);
    
    // 模拟 Suspend 方法返回成功
    MOCKER_CPP(&CollComm::Suspend, HcclResult(CollComm:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试回调函数
    auto ret = ProcessTaskAbortHandleCallback(deviceLogicId, stage, timeout, args);
    EXPECT_EQ(ret, static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS));
    
    // 清理
    HcclTaskAbortHandler::GetInstance().UnRegister(comm);
}

TEST_F(HcclTaskAbortHandlerTest, test_process_task_abort_handle_callback_post)
{
    // 构造测试数据
    int32_t deviceLogicId = 0;
    aclrtDeviceTaskAbortStage stage = aclrtDeviceTaskAbortStage::ACL_RT_DEVICE_TASK_ABORT_POST;
    uint32_t timeout = 30U;
    
    // 准备 communicator
    CollComm *comm = nullptr;
    HcclTaskAbortHandler::GetInstance().Register(comm);
    void* args = reinterpret_cast<void*>(&HcclTaskAbortHandler::GetInstance().commVector_);
    
    // 模拟相关方法
    MOCKER(hcomm::CcuSetTaskKill).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hcomm::CcuSetTaskKillDone).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&CollComm::Clean, HcclResult(CollComm:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试回调函数
    auto ret = ProcessTaskAbortHandleCallback(deviceLogicId, stage, timeout, args);
    EXPECT_EQ(ret, static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS));
    
    // 清理
    HcclTaskAbortHandler::GetInstance().UnRegister(comm);
}

TEST_F(HcclTaskAbortHandlerTest, test_process_task_abort_handle_callback_null_args)
{
    // 构造测试数据
    int32_t deviceLogicId = 0;
    aclrtDeviceTaskAbortStage stage = aclrtDeviceTaskAbortStage::ACL_RT_DEVICE_TASK_ABORT_PRE;
    uint32_t timeout = 30U;
    void* args = nullptr;
    
    // 测试空参数情况
    auto ret = ProcessTaskAbortHandleCallback(deviceLogicId, stage, timeout, args);
    // 由于 args 为 null，应该返回失败
    EXPECT_NE(ret, static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS));
}
