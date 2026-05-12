/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * @file ut_aicpu_ts_DispatchAllStreams.cc
 * @brief HCCL DispatchAllStreams接口单元测试文件
 *
 * 本文件用于测试HCCL通信库中的DispatchAllStreams接口，该接口用于在A5设备上分发所有流的任务。
 * DispatchAllStreams负责遍历所有线程并调用TryLaunchTask尝试下发任务。
 *
 * 测试的接口：DispatchAllStreams
 * - 功能：在A5设备上分发所有流的任务
 * - 参数：threads（线程数组指针）、threadNum（线程数量）
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#include "dispatcher_aicpu.h"
#undef private

using namespace hccl;

/**
 * @class UtAicpuTsDispatchAllStreams
 * @brief DispatchAllStreams接口测试类
 *
 * 该测试类用于验证DispatchAllStreams接口的参数校验和基本功能。
 * 测试场景包括空指针校验、参数有效性校验和设备类型检查。
 */
class UtAicpuTsDispatchAllStreams : public testing::Test
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

    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    int32_t res{HCCL_E_RESERVED};
};

/**
 * @test Ut_DispatchAllStreams_When_Threads_IsNull_Expect_ReturnHCCL_E_PTR
 * @brief 测试当线程数组指针为空时，DispatchAllStreams应返回参数错误
 *
 * 测试场景：传入空指针作为线程数组
 * 预期结果：返回HCCL_E_PTR错误码，表示参数错误
 */
TEST_F(UtAicpuTsDispatchAllStreams, Ut_DispatchAllStreams_When_Threads_IsNull_Expect_ReturnHCCL_E_PTR)
{
    res = DispatchAllStreams(nullptr, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

/**
 * @test Ut_DispatchAllStreams_When_ThreadNum_IsZero_Expect_ReturnHCCL_E_PARA
 * @brief 测试当线程数量为零时，DispatchAllStreams应返回参数错误
 *
 * 测试场景：传入线程数量为0
 * 预期结果：返回HCCL_E_PARA错误码，表示参数错误
 */
TEST_F(UtAicpuTsDispatchAllStreams, Ut_DispatchAllStreams_When_ThreadNum_IsZero_Expect_ReturnHCCL_E_PARA)
{
    res = DispatchAllStreams(&thread, 0);
    EXPECT_EQ(res, HCCL_E_PARA);
}

/**
 * @test Ut_DispatchAllStreams_When_NotOnA5Device_Expect_ReturnHCCL_E_NOT_SUPPORT
 * @brief 测试当不在A5设备上时，DispatchAllStreams应返回不支持错误
 *
 * 测试场景：在非A5设备上调用DispatchAllStreams
 * 预期结果：返回HCCL_E_NOT_SUPPORT错误码，表示不支持
 */
TEST_F(UtAicpuTsDispatchAllStreams, Ut_DispatchAllStreams_When_NotOnA5Device_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    res = DispatchAllStreams(&thread, 1);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

/**
 * @test Ut_DispatchAllStreams_When_ThreadNum_IsOne_Expect_ReturnHCCL_E_NOT_SUPPORT
 * @brief 测试当线程数量为1但不在A5设备上时，DispatchAllStreams应返回不支持错误
 *
 * 测试场景：在非A5设备上调用DispatchAllStreams，线程数量为1
 * 预期结果：返回HCCL_E_NOT_SUPPORT错误码，表示不支持
 */
TEST_F(UtAicpuTsDispatchAllStreams, Ut_DispatchAllStreams_When_ThreadNum_IsOne_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    res = DispatchAllStreams(&thread, 1);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

/**
 * @test Ut_DispatchAllStreams_When_OnA5Device_Expect_ReturnSuccess
 * @brief 测试在A5设备上调用DispatchAllStreams应返回成功
 *
 * 测试场景：在A5设备上调用DispatchAllStreams
 * 预期结果：返回HCCL_SUCCESS
 *
 * 注意：需要mock IsDeviceA5返回true来模拟A5设备环境
 */
TEST_F(UtAicpuTsDispatchAllStreams, Ut_DispatchAllStreams_When_OnA5Device_Expect_ReturnSuccess)
{
    MOCKER(&AicpuTsThread::IsDeviceA5)
        .stubs()
        .will(returnValue(true));

    res = DispatchAllStreams(&thread, 1);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

/**
 * @test Ut_DispatchAllStreams_When_MultipleThreadsOnA5Device_Expect_ReturnSuccess
 * @brief 测试在A5设备上使用多个线程调用DispatchAllStreams应返回成功
 *
 * 测试场景：在A5设备上使用多个线程调用DispatchAllStreams
 * 预期结果：返回HCCL_SUCCESS
 *
 * 注意：需要mock IsDeviceA5返回true来模拟A5设备环境
 */
TEST_F(UtAicpuTsDispatchAllStreams, Ut_DispatchAllStreams_When_MultipleThreadsOnA5Device_Expect_ReturnSuccess)
{
    AicpuTsThread thread2{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle threads[2] = {reinterpret_cast<ThreadHandle>(&threadOnDevice), reinterpret_cast<ThreadHandle>(&thread2)};

    MOCKER(&AicpuTsThread::IsDeviceA5)
        .stubs()
        .will(returnValue(true));

    res = DispatchAllStreams(threads, 2);
    EXPECT_EQ(res, HCCL_SUCCESS);
}
