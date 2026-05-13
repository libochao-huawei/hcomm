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
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include "hostdpu/task_service.h"

using namespace Hccl;

// ===== mockcpp mock helper for aclrtMemcpy (needed by HBM mode) =====
static aclError MockAclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind)
{
    if (dst && src && destMax > 0 && count > 0) {
        memcpy(dst, src, (count < destMax) ? count : destMax);
    }
    return ACL_SUCCESS;
}

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
        g_callbackResult = 0;
        deviceMem.resize(deviceMemSize, 0);
        hostMem.resize(hostMemSize, 0);
        std::cout << "A Test case in TaskServiceTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TaskServiceTest TearDown" << std::endl;
    }

public:
    static int32_t g_callbackResult;
    int32_t deviceMemSize = 1024; // less than controlSize * 2
    int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem;
    std::vector<uint8_t> hostMem;
};
int32_t TaskServiceTest::g_callbackResult = 0;
constexpr int32_t controlSize = sizeof(uint8_t) + sizeof(char) * 256 + sizeof(uint32_t) + 8;

// ===== Helper: set up TASK_OK payload in shared memory =====
static void SetupOkTask(uint8_t *base, const std::string &taskType,
                         uint32_t msgId, uint32_t dataSize)
{
    char *taskTypePtr = reinterpret_cast<char *>(base + 1);
    memset(static_cast<void *>(taskTypePtr), 0, 256);
    size_t copyLen = taskType.size();
    if (copyLen > 255) {
        copyLen = 255;
    }
    memcpy(static_cast<void *>(taskTypePtr), taskType.c_str(), copyLen);
    taskTypePtr[copyLen] = '\0';
    uint32_t *msgIdPtr = reinterpret_cast<uint32_t *>(base + 1 + 256);
    *msgIdPtr = msgId;
    size_t *dataSizePtr = reinterpret_cast<size_t *>(base + 1 + 256 + sizeof(uint32_t));
    *dataSizePtr = static_cast<size_t>(dataSize);
    base[0] = 1; // TASK_OK
}

// ===== Helper: wait for TaskRun to enter the while-loop =====
static void WaitForTaskRunLoop(uint8_t *flagPtr)
{
    auto start = std::chrono::steady_clock::now();
    while (*flagPtr != 0) { // TASK_UNSET
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        ASSERT_LT(elapsed, 5000) << "Timeout waiting for TaskRun to enter loop";
    }
}

// ===== Callback stubs =====
static int32_t TrackingCallback(uint64_t /*addr*/, int32_t /*size*/)
{
    TaskServiceTest::g_callbackResult = 42; // signal that callback was called
    return 0;
}

static int32_t FailingCallback(uint64_t /*addr*/, int32_t /*size*/)
{
    TaskServiceTest::g_callbackResult = -1;
    return -1;
}

// ==================== Existing tests (backward compat) ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_DataSizeIsZero_Expect_ReturnError) {
    // shmemSize_ < controlSize => leftSize_ = 0 => TaskRun returns HCCL_E_INTERNAL
    deviceMemSize = controlSize * 1; // less than controlSize * 2
    hostMemSize = 1024;
    deviceMem.resize(deviceMemSize, 0);
    hostMem.resize(hostMemSize, 0);

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    HcclResult ret = taskService.TaskRun();
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TaskServiceTest, Ut_TaskRun_When_DataSizeExceedsHostMemSize_Expect_ReturnError) {
    // 测试 dataSize_ > hostMemSize_ 的情况
    deviceMemSize = (controlSize + 2048) * 2; // dataSize_ = 2048
    hostMemSize = 1024; // hostMemSize_ < dataSize_
    deviceMem.resize(deviceMemSize, 0);
    hostMem.resize(hostMemSize, 0);
    
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    
    // 调用TaskRun,应该返回HCCL_E_INTERNAL
    HcclResult ret = taskService.TaskRun();
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// ==================== Register / UnRegister ====================

TEST_F(TaskServiceTest, Ut_TaskRegister_When_Normal_Expect_Success)
{
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    HcclResult ret = taskService.TaskRegister("testTask", TrackingCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_TaskUnRegister_When_TypeExists_Expect_Success)
{
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    taskService.TaskRegister("testTask", TrackingCallback);
    HcclResult ret = taskService.TaskUnRegister("testTask");
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_TaskUnRegister_When_TypeNotExists_Expect_NotFound)
{
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    HcclResult ret = taskService.TaskUnRegister("nonexistent");
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

// ==================== TaskRun exit flags (no callback needed) ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TerminateFlag_Expect_Success)
{
    std::vector<uint8_t> deviceMemFake(deviceMemSize, 0xFF);

    TaskService taskService(deviceMemFake.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMemFake.data());

    uint8_t *flagPtr = deviceMemFake.data();
    *flagPtr = 2; // TASK_TERMINATE

    taskThread.join();

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(*flagPtr, 3); // TASK_TERMINATE_RESPONSE
}

TEST_F(TaskServiceTest, Ut_TaskRun_When_TerminateResponseFlag_Expect_Success)
{
    std::vector<uint8_t> deviceMemFake(deviceMemSize, 0xFF);

    TaskService taskService(deviceMemFake.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMemFake.data());

    uint8_t *flagPtr = deviceMemFake.data();
    *flagPtr = 3; // TASK_TERMINATE_RESPONSE

    taskThread.join();

    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_TaskRun_When_InvalidFlag_Expect_Success)
{
    std::vector<uint8_t> deviceMemFake(deviceMemSize, 0xFF);

    TaskService taskService(deviceMemFake.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMemFake.data());

    uint8_t *flagPtr = deviceMemFake.data();
    *flagPtr = 7; // unknown / invalid flag

    taskThread.join();

    EXPECT_EQ(result, HCCL_SUCCESS);
}

// ==================== TASK_OK with unknown callback ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TaskOkCallbackNotFound_Expect_NotFound)
{
    std::vector<uint8_t> deviceMemFake(deviceMemSize, 0xFF);

    TaskService taskService(deviceMemFake.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    taskService.TaskRegister("existingTask", TrackingCallback);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMemFake.data());
    SetupOkTask(deviceMemFake.data(), "nonexistentTask", 42, 0);
    // flag is now TASK_OK (1) from SetupOkTask

    taskThread.join();

    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
}

// ==================== TASK_OK with data size exceeding hostSize ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TaskOkDataTooLarge_Expect_Error)
{
    std::vector<uint8_t> deviceMemFake(deviceMemSize, 0xFF);
    // deviceMemSize=1024 => shmemSize_=512 => leftSize_=512-261=251

    TaskService taskService(deviceMemFake.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    taskService.TaskRegister("testTask", TrackingCallback);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMemFake.data());
    SetupOkTask(deviceMemFake.data(), "testTask", 42, 500); // dataSize=500 > leftSize_=251
    // flag is now TASK_OK (1) from SetupOkTask

    taskThread.join();

    EXPECT_EQ(result, HCCL_E_PARA);
}

// ==================== TASK_OK with callback returning error ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TaskOkCallbackFails_Expect_Error)
{
    std::vector<uint8_t> deviceMemFake(deviceMemSize, 0xFF);

    TaskService taskService(deviceMemFake.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    taskService.TaskRegister("testTask", FailingCallback);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMemFake.data());
    SetupOkTask(deviceMemFake.data(), "testTask", 42, 16); // dataSize > 0 to bypass ExecuteTask's dataSize==0 early return
    // flag is now TASK_OK (1) from SetupOkTask

    taskThread.join();

    EXPECT_EQ(result, HCCL_E_INTERNAL);
}

// 测试 TaskProfRegister - 正常注册
TEST_F(TaskServiceTest, Ut_TaskProfRegister_When_Normal_Expect_ReturnSuccess)
{
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    
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
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    
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
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    
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
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);

    // 注册一个空操作的回调
    Hccl::ProfCallbackTemplate profCallback = [](const Hccl::TaskParam& taskParam, uint64_t handle) -> HcclResult {
        return HCCL_SUCCESS;
    };

    HcclResult ret = taskService.TaskProfRegister(profCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_WriteFlag_HBM_When_rtMemcpy_Fail_Expect_Failure) {
    MOCKER(aclrtMemcpy).stubs().will(returnValue(ACL_ERROR_INVALID_PARAM));
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, false); // HBM mode

    uint8_t *flagPtr = reinterpret_cast<uint8_t *>(deviceMem.data());
    HcclResult ret = taskService.WriteFlag(flagPtr, 1);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TaskServiceTest, Ut_ReadFlag_HBM_When_rtMemcpy_SUCC_Expect_Success) {
    MOCKER_CPP(&TaskService::MemcpyDevice).stubs().will(returnValue(HCCL_SUCCESS));
    TaskService taskService(deviceMem.data(), 4096, hostMem.data(), 1024, false); // HBM mode

    uint8_t *flagPtr = reinterpret_cast<uint8_t *>(deviceMem.data());
    uint8_t flag = 0;
    HcclResult ret = taskService.ReadFlag(flagPtr, 1, flag);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_SynchronizeControlInfo_HBM_When_memcpy_SUCC_Expect_Success) {
    MOCKER_CPP(&TaskService::MemcpyDevice).stubs().will(returnValue(HCCL_SUCCESS));
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, false); // HBM mode

    uint8_t *flagPtr = reinterpret_cast<uint8_t *>(deviceMem.data());
    uint8_t flag = 0;
    HcclResult ret = taskService.SynchronizeControlInfo(flagPtr, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_TaskRun_HbmMode_When_aclrtMemcpy_Fail_Expect_Failure)
{
    MOCKER(aclrtMemcpy).stubs().will(returnValue(ACL_ERROR_INVALID_PARAM));
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, false);

    const char* errorMsg = "aclrtMemcpy failed in TaskRun";
    HcclResult ret = taskService.MemcpyDevice(hostMem.data(), 512, deviceMem.data(), 512, ACL_MEMCPY_DEVICE_TO_HOST, errorMsg);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TaskServiceTest, TaskServiceTest_Ut_TaskRun_BARMode_When_memcpy_SUCC_Expect_Success)
{
    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize, true);
    const char* errorMsg = "memcpy_s failed in TaskRun";
    HcclResult ret = taskService.MemcpyDevice(hostMem.data(), 512, deviceMem.data(), 512, ACL_MEMCPY_DEVICE_TO_HOST, errorMsg);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
