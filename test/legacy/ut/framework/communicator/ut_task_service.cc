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
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "hostdpu/task_service.h"
#include "acl/acl_rt.h"
#include <thread>

using namespace Hccl;
class TaskServiceTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        std::cout << "TaskServiceTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TaskServiceTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in TaskServiceTest SetUp" << std::endl;
        // 分配测试内存
        hostMem_ = new uint8_t[1024];
        npu2dpuMem_ = new uint8_t[1024];
        dpu2npuMem_ = new uint8_t[1024];
        taskService_ = new TaskService(npu2dpuMem_, 1024, hostMem_, 1024);
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in TaskServiceTest TearDown" << std::endl;
        delete taskService_;
        delete[] hostMem_;
        delete[] npu2dpuMem_;
        delete[] dpu2npuMem_;
        GlobalMockObject::verify();
    }

    TaskService *taskService_;
    uint8_t *hostMem_;
    uint8_t *npu2dpuMem_;
    uint8_t *dpu2npuMem_;
};

// 测试回调函数
int TestCallback(uint64_t data, uint32_t size)
{
    return 0;
}

TEST_F(TaskServiceTest, should_success_when_task_run_with_terminate_flag)
{
    // 模拟aclrtMemcpy函数
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    // 设置初始flag为TASK_TERMINATE
    uint8_t flag = TASK_TERMINATE;
    memcpy(npu2dpuMem_, &flag, sizeof(flag));

    // 运行TaskRun
    HcclResult ret = taskService_->TaskRun();
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(TaskServiceTest, should_success_when_task_run_with_terminate_response_flag)
{
    // 模拟aclrtMemcpy函数
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    // 设置初始flag为TASK_TERMINATE_RESPONSE
    uint8_t flag = TASK_TERMINATE_RESPONSE;
    memcpy(npu2dpuMem_, &flag, sizeof(flag));

    // 运行TaskRun
    HcclResult ret = taskService_->TaskRun();
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(TaskServiceTest, should_success_when_task_run_with_default_flag)
{
    // 模拟aclrtMemcpy函数
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    // 设置初始flag为默认值(非TASK_UNSET, TASK_OK, TASK_TERMINATE, TASK_TERMINATE_RESPONSE)
    uint8_t flag = 99;
    memcpy(npu2dpuMem_, &flag, sizeof(flag));

    // 运行TaskRun
    HcclResult ret = taskService_->TaskRun();
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(TaskServiceTest, should_fail_when_hostmem_null)
{
    // 创建一个hostMem为null的TaskService
    TaskService *taskService = new TaskService(npu2dpuMem_, 1024, nullptr, 1024);
    
    // 运行TaskRun
    HcclResult ret = taskService->TaskRun();
    EXPECT_EQ(HCCL_E_PARA, ret);
    
    delete taskService;
}

TEST_F(TaskServiceTest, should_fail_when_npu2dpumem_null)
{
    // 创建一个npu2dpuMem为null的TaskService
    TaskService *taskService = new TaskService(nullptr, 1024, hostMem_, 1024);
    
    // 运行TaskRun
    HcclResult ret = taskService->TaskRun();
    EXPECT_EQ(HCCL_E_PARA, ret);
    
    delete taskService;
}

TEST_F(TaskServiceTest, should_fail_when_dpu2npumem_null)
{
    // 由于dpu2npuMem_是私有成员，我们无法直接设置为null
    // 这个测试场景需要通过其他方式实现，暂时跳过
    GTEST_SKIP() << "dpu2npuMem_ is private, cannot be set to null directly";
}

TEST_F(TaskServiceTest, should_fail_when_data_size_zero)
{
    // 创建一个dataSize为0的TaskService
    // 通过设置deviceMemSize非常小来实现
    TaskService *taskService = new TaskService(npu2dpuMem_, 10, hostMem_, 1024);
    
    // 运行TaskRun
    HcclResult ret = taskService->TaskRun();
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
    
    delete taskService;
}

TEST_F(TaskServiceTest, should_fail_when_hostmem_size_less_than_data_size)
{
    // 创建一个hostMemSize小于dataSize的TaskService
    TaskService *taskService = new TaskService(npu2dpuMem_, 1024, hostMem_, 100);
    
    // 运行TaskRun
    HcclResult ret = taskService->TaskRun();
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
    
    delete taskService;
}

TEST_F(TaskServiceTest, should_handle_task_ok_flag)
{
    // 模拟aclrtMemcpy函数
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    // 注册测试回调
    taskService_->TaskRegister("test_task", TestCallback);

    // 设置初始flag为TASK_OK
    uint8_t flag = TASK_OK;
    memcpy(npu2dpuMem_, &flag, sizeof(flag));
    
    // 设置taskType
    char taskType[] = "test_task";
    memcpy(npu2dpuMem_ + sizeof(flag), taskType, sizeof(taskType));
    
    // 设置msgId
    uint32_t msgId = 123;
    memcpy(npu2dpuMem_ + sizeof(flag) + 256, &msgId, sizeof(msgId));

    // 运行TaskRun（会在循环中等待，需要在另一个线程中修改flag为TERMINATE）
    std::thread runThread([this]() {
        taskService_->TaskRun();
    });
    
    // 等待一段时间让TaskRun开始执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 修改flag为TERMINATE以退出循环
    uint8_t terminateFlag = TASK_TERMINATE;
    memcpy(npu2dpuMem_, &terminateFlag, sizeof(terminateFlag));
    
    // 等待线程结束
    runThread.join();
}

TEST_F(TaskServiceTest, should_handle_task_unset_flag)
{
    // 模拟aclrtMemcpy函数
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    // 设置初始flag为TASK_UNSET
    uint8_t flag = TASK_UNSET;
    memcpy(npu2dpuMem_, &flag, sizeof(flag));

    // 运行TaskRun（会在循环中等待，需要在另一个线程中修改flag为TERMINATE）
    std::thread runThread([this]() {
        taskService_->TaskRun();
    });
    
    // 等待一段时间让TaskRun开始执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 修改flag为TERMINATE以退出循环
    uint8_t terminateFlag = TASK_TERMINATE;
    memcpy(npu2dpuMem_, &terminateFlag, sizeof(terminateFlag));
    
    // 等待线程结束
    runThread.join();
}
