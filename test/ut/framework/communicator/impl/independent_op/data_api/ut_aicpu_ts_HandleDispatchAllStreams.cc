/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file for the full text of the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * @file ut_aicpu_ts_HandleDispatchAllStreams.cc
 * @brief HCCL HandleDispatchAllStreams接口单元测试文件
 *
 * 本文件用于测试HCCL通信库中的HandleDispatchAllStreams接口，该接口用于调用全局LaunchContext的HandleDispatchAllStreams方法。
 * HandleDispatchAllStreams负责调用DispatchAllStreams分发所有流的任务。
 *
 * 测试的接口：HandleDispatchAllStreams
 * - 功能：调用全局g_threadLaunchCtx的HandleDispatchAllStreams方法
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.hpp"
#include <mockcpp/mockcpp.hpp>

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#include "dispatcher_aicpu.h"
#undef private

using namespace hccl;

extern void AddThread(ThreadHandle thread);
extern HcclResult SetLaunchMode(const char* launchTag, HcommLaunchMode mode);

/**
 * @class UtAicpuTsHandleDispatchAllStreams
 * @brief HandleDispatchAllStreams接口测试类
 *
 * 该测试类用于验证HandleDispatchAllStreams接口的各种场景。
 */
class UtAicpuTsHandleDispatchAllStreams : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().will(returnValue(HCCL_SUCCESS));
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

/**
 * @test Ut_HandleDispatchAllStreams_When_LaunchTagNotSet_Expect_ReturnSuccess
 * @brief 测试当未设置launchTag时调用HandleDispatchAllStreams应返回成功
 *
 * 测试场景：未设置launchTag，直接调用HandleDispatchAllStreams
 * 预期结果：返回HCCL_SUCCESS（找不到launchTag，直接返回成功）
 */
TEST_F(UtAicpuTsHandleDispatchAllStreams, Ut_HandleDispatchAllStreams_When_LaunchTagNotSet_Expect_ReturnSuccess)
{
    HcclResult res = HandleDispatchAllStreams();
    EXPECT_EQ(res, HCCL_SUCCESS);
}

/**
 * @test Ut_HandleDispatchAllStreams_When_NoThreadRegistered_Expect_ReturnSuccess
 * @brief 测试当设置了launchTag但未注册线程时调用HandleDispatchAllStreams应返回成功
 *
 * 测试场景：设置了launchTag，但未注册任何线程
 * 预期结果：返回HCCL_SUCCESS（线程集合为空，直接返回成功）
 */
TEST_F(UtAicpuTsHandleDispatchAllStreams, Ut_HandleDispatchAllStreams_When_NoThreadRegistered_Expect_ReturnSuccess)
{
    HcclResult res = SetLaunchMode("test_tag", HCOMM_LAUNCH_MODE_BATCH);
    EXPECT_EQ(res, HCCL_SUCCESS);

    res = HandleDispatchAllStreams();
    EXPECT_EQ(res, HCCL_SUCCESS);
}

/**
 * @test Ut_HandleDispatchAllStreams_When_ThreadRegisteredOnA5_Expect_ReturnSuccess
 * @brief 测试当注册了A5设备上的线程时调用HandleDispatchAllStreams应返回成功
 *
 * 测试场景：设置了launchTag，注册了A5设备上的线程
 * 预期结果：返回HCCL_SUCCESS
 */
TEST_F(UtAicpuTsHandleDispatchAllStreams, Ut_HandleDispatchAllStreams_When_ThreadRegisteredOnA5_Expect_ReturnSuccess)
{
    HcclResult res = SetLaunchMode("test_tag", HCOMM_LAUNCH_MODE_BATCH);
    EXPECT_EQ(res, HCCL_SUCCESS);

    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);

    MOCKER(&AicpuTsThread::IsDeviceA5)
        .stubs()
        .will(returnValue(true));

    AddThread(thread);

    res = HandleDispatchAllStreams();
    EXPECT_EQ(res, HCCL_SUCCESS);
}

/**
 * @test Ut_HandleDispatchAllStreams_When_MultipleThreadsRegisteredOnA5_Expect_ReturnSuccess
 * @brief 测试当注册了多个A5设备上的线程时调用HandleDispatchAllStreams应返回成功
 *
 * 测试场景：设置了launchTag，注册了多个A5设备上的线程
 * 预期结果：返回HCCL_SUCCESS
 */
TEST_F(UtAicpuTsHandleDispatchAllStreams, Ut_HandleDispatchAllStreams_When_MultipleThreadsRegisteredOnA5_Expect_ReturnSuccess)
{
    HcclResult res = SetLaunchMode("test_tag", HCOMM_LAUNCH_MODE_BATCH);
    EXPECT_EQ(res, HCCL_SUCCESS);

    AicpuTsThread threadOnDevice1{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    AicpuTsThread threadOnDevice2{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread1 = reinterpret_cast<ThreadHandle>(&threadOnDevice1);
    ThreadHandle thread2 = reinterpret_cast<ThreadHandle>(&threadOnDevice2);

    MOCKER(&AicpuTsThread::IsDeviceA5)
        .stubs()
        .will(returnValue(true));

    AddThread(thread1);
    AddThread(thread2);

    res = HandleDispatchAllStreams();
    EXPECT_EQ(res, HCCL_SUCCESS);
}
