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
        std::cout << "A Test case in TaskServiceTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in TaskServiceTest TearDown" << std::endl;
    }

public:
    static int32_t g_callbackResult;
};
int32_t TaskServiceTest::g_callbackResult = 0;

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
    uint32_t *dataSizePtr = reinterpret_cast<uint32_t *>(base + 1 + 256 + sizeof(uint32_t));
    *dataSizePtr = dataSize;
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
    // shmemSize_ < controlSize => hostSize_ = 0 => TaskRun returns HCCL_E_INTERNAL
    constexpr int32_t controlSize = sizeof(uint8_t) + sizeof(char) * 256 + sizeof(uint32_t) + sizeof(uint32_t);
    constexpr int32_t deviceMemSize = controlSize * 1; // less than controlSize * 2
    constexpr int32_t hostMemSize = 1024;

    std::vector<uint8_t> deviceMem(deviceMemSize, 0);
    std::vector<uint8_t> hostMem(hostMemSize, 0);

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);

    HcclResult ret = taskService.TaskRun();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// ==================== Register / UnRegister ====================

TEST_F(TaskServiceTest, Ut_TaskRegister_When_Normal_Expect_Success)
{
    std::vector<uint8_t> deviceMem(1024);
    std::vector<uint8_t> hostMem(512);
    TaskService taskService(deviceMem.data(), 1024, hostMem.data(), 512);

    HcclResult ret = taskService.TaskRegister("testTask", TrackingCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_TaskUnRegister_When_TypeExists_Expect_Success)
{
    std::vector<uint8_t> deviceMem(1024);
    std::vector<uint8_t> hostMem(512);
    TaskService taskService(deviceMem.data(), 1024, hostMem.data(), 512);

    taskService.TaskRegister("testTask", TrackingCallback);
    HcclResult ret = taskService.TaskUnRegister("testTask");
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_TaskUnRegister_When_TypeNotExists_Expect_NotFound)
{
    std::vector<uint8_t> deviceMem(1024);
    std::vector<uint8_t> hostMem(512);
    TaskService taskService(deviceMem.data(), 1024, hostMem.data(), 512);

    HcclResult ret = taskService.TaskUnRegister("nonexistent");
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

// ==================== TaskRun exit flags (no callback needed) ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TerminateFlag_Expect_Success)
{
    constexpr int32_t deviceMemSize = 1024;
    constexpr int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem(deviceMemSize, 0xFF);
    std::vector<uint8_t> hostMem(hostMemSize, 0);

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMem.data());

    uint8_t *flagPtr = deviceMem.data();
    *flagPtr = 2; // TASK_TERMINATE

    taskThread.join();

    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(*flagPtr, 3); // TASK_TERMINATE_RESPONSE
}

TEST_F(TaskServiceTest, Ut_TaskRun_When_TerminateResponseFlag_Expect_Success)
{
    constexpr int32_t deviceMemSize = 1024;
    constexpr int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem(deviceMemSize, 0xFF);
    std::vector<uint8_t> hostMem(hostMemSize, 0);

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMem.data());

    uint8_t *flagPtr = deviceMem.data();
    *flagPtr = 3; // TASK_TERMINATE_RESPONSE

    taskThread.join();

    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(TaskServiceTest, Ut_TaskRun_When_InvalidFlag_Expect_Success)
{
    constexpr int32_t deviceMemSize = 1024;
    constexpr int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem(deviceMemSize, 0xFF);
    std::vector<uint8_t> hostMem(hostMemSize, 0);

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMem.data());

    uint8_t *flagPtr = deviceMem.data();
    *flagPtr = 255; // unknown / invalid flag

    taskThread.join();

    EXPECT_EQ(result, HCCL_SUCCESS);
}

// ==================== TASK_OK with unknown callback ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TaskOkCallbackNotFound_Expect_NotFound)
{
    constexpr int32_t deviceMemSize = 1024;
    constexpr int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem(deviceMemSize, 0xFF);
    std::vector<uint8_t> hostMem(hostMemSize, 0);

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);
    taskService.TaskRegister("existingTask", TrackingCallback);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMem.data());
    SetupOkTask(deviceMem.data(), "nonexistentTask", 42, 0);
    // flag is now TASK_OK (1) from SetupOkTask

    taskThread.join();

    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
}

// ==================== TASK_OK with data size exceeding hostSize ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TaskOkDataTooLarge_Expect_Error)
{
    constexpr int32_t deviceMemSize = 1024;
    constexpr int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem(deviceMemSize, 0xFF);
    std::vector<uint8_t> hostMem(hostMemSize, 0);
    // deviceMemSize=1024 => shmemSize_=512 => hostSize_=512-265=247

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);
    taskService.TaskRegister("testTask", TrackingCallback);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMem.data());
    SetupOkTask(deviceMem.data(), "testTask", 42, 500); // dataSize=500 > hostSize_=247
    // flag is now TASK_OK (1) from SetupOkTask

    taskThread.join();

    EXPECT_EQ(result, HCCL_E_INTERNAL);
}

// ==================== TASK_OK with callback returning error ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TaskOkCallbackFails_Expect_Error)
{
    constexpr int32_t deviceMemSize = 1024;
    constexpr int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem(deviceMemSize, 0xFF);
    std::vector<uint8_t> hostMem(hostMemSize, 0);

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);
    taskService.TaskRegister("testTask", FailingCallback);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMem.data());
    SetupOkTask(deviceMem.data(), "testTask", 42, 16); // dataSize > 0 to bypass ExecuteTask's dataSize==0 early return
    // flag is now TASK_OK (1) from SetupOkTask

    taskThread.join();

    EXPECT_EQ(result, HCCL_E_INTERNAL);
}

// ==================== TASK_OK full success flow ====================

TEST_F(TaskServiceTest, Ut_TaskRun_When_TaskOkFlow_Expect_Success)
{
    constexpr int32_t deviceMemSize = 1024;
    constexpr int32_t hostMemSize = 512;
    std::vector<uint8_t> deviceMem(deviceMemSize, 0xFF);
    std::vector<uint8_t> hostMem(hostMemSize, 0);

    uint8_t *dpu2npuMem = deviceMem.data() + deviceMemSize / 2; // second half

    TaskService taskService(deviceMem.data(), deviceMemSize, hostMem.data(), hostMemSize);
    taskService.TaskRegister("myTask", TrackingCallback);

    HcclResult result = HCCL_SUCCESS;
    std::thread taskThread([&taskService, &result]() {
        result = taskService.TaskRun();
    });

    WaitForTaskRunLoop(deviceMem.data());

    // Prepare and issue TASK_OK
    const std::string taskType = "myTask";
    const uint32_t msgId = 12345;
    const uint32_t dataSize = 16;
    for (uint32_t i = 0; i < dataSize; i++) {
        deviceMem[265 + i] = static_cast<uint8_t>(i); // fill data area
    }
    SetupOkTask(deviceMem.data(), taskType, msgId, dataSize);
    // flag is now TASK_OK (1) from SetupOkTask

    // Wait for callback to be invoked
    auto start = std::chrono::steady_clock::now();
    while (TaskServiceTest::g_callbackResult != 42) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        ASSERT_LT(elapsed, 5000) << "Timeout waiting for callback";
    }

    // Stop the loop
    *deviceMem.data() = 2; // TASK_TERMINATE
    taskThread.join();

    EXPECT_EQ(result, HCCL_SUCCESS);

    // After TaskRun: npu2dpu flag should be TERMINATE_RESPONSE
    // (but the TASK_OK path set it to UNSET first, then TERMINATE set it to TERMINATE_RESPONSE)
    // After TASK_OK processing, flag is UNSET, then we wrote TERMINATE, TaskRun wrote TERMINATE_RESPONSE
    EXPECT_EQ(*deviceMem.data(), 3); // TASK_TERMINATE_RESPONSE

    // Verify dpu2npu response (SynchronizeControlInfo copies control info)
    EXPECT_EQ(dpu2npuMem[0], 1); // dpu2npu flag should be 1 (sent by SynchronizeControlInfo)

    // Verify dpu2npu task type matches
    char *dpuTaskType = reinterpret_cast<char *>(dpu2npuMem + 1);
    EXPECT_STREQ(dpuTaskType, taskType.c_str());

    // Verify dpu2npu msgId matches
    uint32_t *dpuMsgId = reinterpret_cast<uint32_t *>(dpu2npuMem + 1 + 256);
    EXPECT_EQ(*dpuMsgId, msgId);
}
