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
#include <mockcpp/mockcpp.hpp>

#ifndef private
#define private public
#define protected public
#endif
#include "aicpu_communicator.h"
#include "executor_tracer.h"
#include "aicpu_hccl_process.h"
#include "common/aicpu_hccl_common.h"
#undef private
#undef protected

using namespace std;
using namespace hccl;
using namespace dfx_tracer;

static HcclCommAicpu* g_testCommAicpu = nullptr;
static HcclCommAicpu* g_testCommAicpu2 = nullptr;

HcclResult AicpuGetCommAll_stub(std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> &aicpuCommInfo)
{
    if (g_testCommAicpu != nullptr) {
        aicpuCommInfo.push_back(std::make_pair("test_group1", g_testCommAicpu));
    }
    if (g_testCommAicpu2 != nullptr) {
        aicpuCommInfo.push_back(std::make_pair("test_group2", g_testCommAicpu2));
    }
    return HCCL_SUCCESS;
}

class ExecutorTracerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "ExecutorTracerTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "ExecutorTracerTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "ExecutorTracerTest Test SetUP" << std::endl;
        g_testCommAicpu = nullptr;
        g_testCommAicpu2 = nullptr;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "ExecutorTracerTest Test TearDown" << std::endl;
        if (g_testCommAicpu != nullptr) {
            delete g_testCommAicpu;
            g_testCommAicpu = nullptr;
        }
        if (g_testCommAicpu2 != nullptr) {
            delete g_testCommAicpu2;
            g_testCommAicpu2 = nullptr;
        }
    }
};

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_When_CommOpenStatusIsFalse_Expect_NotCallStreamTaskMonitor)
{
    g_testCommAicpu = new HcclCommAicpu;
    g_testCommAicpu->commOpenStatus = false;
    g_testCommAicpu->identifier_ = "test_group";

    bool isStreamTaskMonitorCalled = false;
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke(AicpuGetCommAll_stub));
    MOCKER_CPP(&HcclCommAicpu::StreamTaskMonitor)
        .stubs()
        .will(invoke([&isStreamTaskMonitorCalled]() -> HcclResult {
            isStreamTaskMonitorCalled = true;
            return HCCL_SUCCESS;
        }));

    ExecutorTracer tracer;
    tracer.TaskMonitor();

    EXPECT_FALSE(isStreamTaskMonitorCalled);
}

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_When_CommOpenStatusIsTrue_Expect_CallStreamTaskMonitor)
{
    g_testCommAicpu = new HcclCommAicpu;
    g_testCommAicpu->commOpenStatus = true;
    g_testCommAicpu->identifier_ = "test_group";

    bool isStreamTaskMonitorCalled = false;
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke(AicpuGetCommAll_stub));
    MOCKER_CPP(&HcclCommAicpu::StreamTaskMonitor)
        .stubs()
        .will(invoke([&isStreamTaskMonitorCalled]() -> HcclResult {
            isStreamTaskMonitorCalled = true;
            return HCCL_SUCCESS;
        }));

    ExecutorTracer tracer;
    tracer.TaskMonitor();

    EXPECT_TRUE(isStreamTaskMonitorCalled);
}

TEST_F(ExecutorTracerTest, Ut_StreamTaskMonitor_When_CommOpenStatusIsFalse_Expect_ReturnSuccessWithoutLog)
{
    HcclCommAicpu *hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->commOpenStatus = false;
    hcclCommAicpu->identifier_ = "test_group";

    HcclResult ret = hcclCommAicpu->StreamTaskMonitor();

    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete hcclCommAicpu;
}

TEST_F(ExecutorTracerTest, Ut_StreamTaskMonitor_When_CommOpenStatusIsTrue_Expect_NormalExecution)
{
    HcclCommAicpu *hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->commOpenStatus = true;
    hcclCommAicpu->identifier_ = "test_group";
    hcclCommAicpu->taskMonitorInterval_ = 0;

    HcclResult ret = hcclCommAicpu->StreamTaskMonitor();

    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete hcclCommAicpu;
}

TEST_F(ExecutorTracerTest, Ut_StreamTaskMonitor_When_IsNoNeedMonitorIsTrue_Expect_ReturnSuccess)
{
    HcclCommAicpu *hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->commOpenStatus = true;
    hcclCommAicpu->identifier_ = "test_group";
    hcclCommAicpu->taskMonitorInterval_ = 0;

    HcclResult ret = hcclCommAicpu->StreamTaskMonitor();

    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete hcclCommAicpu;
}

TEST_F(ExecutorTracerTest, Ut_TaskMonitor_When_MultipleCommsWithMixedStatus_Expect_OnlyCallActiveOnes)
{
    g_testCommAicpu = new HcclCommAicpu;
    g_testCommAicpu->commOpenStatus = true;
    g_testCommAicpu->identifier_ = "active_group";

    g_testCommAicpu2 = new HcclCommAicpu;
    g_testCommAicpu2->commOpenStatus = false;
    g_testCommAicpu2->identifier_ = "inactive_group";

    int callCount = 0;
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke(AicpuGetCommAll_stub));
    MOCKER_CPP(&HcclCommAicpu::StreamTaskMonitor)
        .stubs()
        .will(invoke([&callCount]() -> HcclResult {
            callCount++;
            return HCCL_SUCCESS;
        }));

    ExecutorTracer tracer;
    tracer.TaskMonitor();

    EXPECT_EQ(callCount, 1);
}