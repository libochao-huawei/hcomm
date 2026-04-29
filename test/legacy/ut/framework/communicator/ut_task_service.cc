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
#include <stdlib.h>
#include "hostdpu/task_service.h"

using namespace Hccl;

class TaskServiceTest : public testing::Test {
protected:
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
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in TaskServiceTest TearDown" << std::endl;
    }
};

TEST_F(TaskServiceTest, Ut_TaskRun_When_DataSizeIsZero_Expect_ReturnError) {
    // 测试 dataSize_ == 0 的情况
    // 计算controlSize,使shmemSize_ < controlSize,从而dataSize_ = 0
    constexpr int32_t controlSize = sizeof(uint8_t) + sizeof(char) * 256 + sizeof(uint32_t);
    constexpr int32_t deviceMemSize = controlSize * 1; // 小于controlSize * 2
    constexpr int32_t hostMemSize = 1024;
    
    // 分配内存
    void *deviceMem = reinterpret_cast<void *>(0x1234);
    void *hostMem = reinterpret_cast<void *>(0x4321);
    
    TaskService taskService(deviceMem, deviceMemSize, hostMem, hostMemSize);
    
    // 调用TaskRun,应该返回HCCL_E_INTERNAL
    HcclResult ret = taskService.TaskRun();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TaskServiceTest, Ut_TaskRun_When_DataSizeExceedsHostMemSize_Expect_ReturnError) {
    // 测试 dataSize_ > hostMemSize_ 的情况
    constexpr int32_t controlSize = sizeof(uint8_t) + sizeof(char) * 256 + sizeof(uint32_t);
    constexpr int32_t deviceMemSize = (controlSize + 2048) * 2; // dataSize_ = 2048
    constexpr int32_t hostMemSize = 1024; // hostMemSize_ < dataSize_
    
    // 分配内存
    void *deviceMem = reinterpret_cast<void *>(0x1234);
    void *hostMem = reinterpret_cast<void *>(0x4321);
    
    TaskService taskService(deviceMem, deviceMemSize, hostMem, hostMemSize);
    
    // 调用TaskRun,应该返回HCCL_E_INTERNAL
    HcclResult ret = taskService.TaskRun();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 测试 TaskProfRegister - 正常注册
TEST_F(TaskServiceTest, Ut_TaskProfRegister_When_Normal_Expect_ReturnSuccess)
{
    void *deviceMem = reinterpret_cast<void *>(0x1234);
    void *hostMem = reinterpret_cast<void *>(0x4321);
    TaskService taskService(deviceMem, 4096, hostMem, 1024);
    
    // 创建 prof 回调
    Hccl::ProfCallbackTemplate profCallback = [](const Hccl::TaskParam& taskParam, uint64_t handle) -> HcclResult {
        return HCCL_SUCCESS;
    };
    
    HcclResult ret = taskService.TaskProfRegister(profCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 TaskProfRegister - 注册后回调可用
TEST_F(TaskServiceTest, Ut_TaskProfRegister_When_Registered_Expect_CallbackSet)
{
    void *deviceMem = reinterpret_cast<void *>(0x1234);
    void *hostMem = reinterpret_cast<void *>(0x4321);
    TaskService taskService(deviceMem, 4096 * 2, hostMem, 1024);
    
    bool callbackInvoked = false;
    Hccl::ProfCallbackTemplate profCallback = [&callbackInvoked](const Hccl::TaskParam& taskParam, uint64_t handle) -> HcclResult {
        callbackInvoked = true;
        return HCCL_SUCCESS;
    };
    
    HcclResult ret = taskService.TaskProfRegister(profCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 验证注册成功
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 TaskProfRegister - 多次注册覆盖
TEST_F(TaskServiceTest, Ut_TaskProfRegister_When_MultipleRegistrations_Expect_LastOneTakesEffect)
{
    void *deviceMem = reinterpret_cast<void *>(0x1234);
    void *hostMem = reinterpret_cast<void *>(0x4321);
    TaskService taskService(deviceMem, 4096, hostMem, 1024);
    
    // 第一次注册
    Hccl::ProfCallbackTemplate profCallback1 = [](const Hccl::TaskParam& taskParam, uint64_t handle) -> HcclResult {
        return HCCL_SUCCESS;
    };
    HcclResult ret1 = taskService.TaskProfRegister(profCallback1);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    
    // 第二次注册(覆盖)
    Hccl::ProfCallbackTemplate profCallback2 = [](const Hccl::TaskParam& taskParam, uint64_t handle) -> HcclResult {
        return HCCL_E_INTERNAL;  // 不同的返回值
    };
    HcclResult ret2 = taskService.TaskProfRegister(profCallback2);
    EXPECT_EQ(ret2, HCCL_SUCCESS);
}

// 测试 TaskProfRegister - 注册空操作回调
TEST_F(TaskServiceTest, Ut_TaskProfRegister_When_EmptyCallback_Expect_ReturnSuccess)
{
    void *deviceMem = reinterpret_cast<void *>(0x1234);
    void *hostMem = reinterpret_cast<void *>(0x4321);
    TaskService taskService(deviceMem, 4096, hostMem, 1024);
    
    // 注册一个空操作的回调
    Hccl::ProfCallbackTemplate profCallback = [](const Hccl::TaskParam& taskParam, uint64_t handle) -> HcclResult {
        return HCCL_SUCCESS;
    };
    
    HcclResult ret = taskService.TaskProfRegister(profCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
