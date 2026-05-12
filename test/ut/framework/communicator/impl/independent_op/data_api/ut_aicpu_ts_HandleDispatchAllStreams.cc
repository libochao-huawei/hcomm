/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use the file for the full text of the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * @file ut_aicpu_ts_HandleDispatchAllStreams.cc
 * @brief HCCL HandleDispatchAllStreams接口单元测试文件
 *
 * 本文件用于测试HCCL通信库中的HandleDispatchAllStreams接口，该接口用于调用全局LaunchContext的HandleDispatchAllStreams方法。
 *
 * 测试的接口：HandleDispatchAllStreams
 * - 功能：调用全局g_threadLaunchCtx的HandleDispatchAllStreams方法
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.hpp"
#include <mockcpp/mockcpp.hpp>

#include "aicpu_ts_thread.h"
#include "dispatcher_aicpu.h"

using namespace hccl;

extern HcclResult DispatchAllStreams(ThreadHandle *threads, uint32_t threadNum);

class UtAicpuTsHandleDispatchAllStreams : public testing::Test
{
protected:
    virtual void SetUp() override
    {
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

/**
 * @test Ut_HandleDispatchAllStreams_When_DispatchAllStreamsReturnsSuccess_Expect_ReturnSuccess
 * @brief 测试当DispatchAllStreams返回成功时，HandleDispatchAllStreams应返回成功
 *
 * 测试场景：mock DispatchAllStreams返回成功
 * 预期结果：返回HCCL_SUCCESS
 */
TEST_F(UtAicpuTsHandleDispatchAllStreams, Ut_HandleDispatchAllStreams_When_DispatchAllStreamsReturnsSuccess_Expect_ReturnSuccess)
{
    MOCKER(DispatchAllStreams)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult res = HandleDispatchAllStreams();
    EXPECT_EQ(res, HCCL_SUCCESS);
}

/**
 * @test Ut_HandleDispatchAllStreams_When_DispatchAllStreamsReturnsFailure_Expect_ReturnFailure
 * @brief 测试当DispatchAllStreams返回失败时，HandleDispatchAllStreams应返回失败
 *
 * 测试场景：mock DispatchAllStreams返回失败
 * 预期结果：返回HCCL_E_INTERNAL
 */
TEST_F(UtAicpuTsHandleDispatchAllStreams, Ut_HandleDispatchAllStreams_When_DispatchAllStreamsReturnsFailure_Expect_ReturnFailure)
{
    MOCKER(DispatchAllStreams)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    HcclResult res = HandleDispatchAllStreams();
    EXPECT_EQ(res, HCCL_E_INTERNAL);
}

/**
 * @test Ut_HandleDispatchAllStreams_When_DispatchAllStreamsReturnsNotSupport_Expect_ReturnNotSupport
 * @brief 测试当DispatchAllStreams返回不支持时，HandleDispatchAllStreams应返回不支持
 *
 * 测试场景：mock DispatchAllStreams返回不支持
 * 预期结果：返回HCCL_E_NOT_SUPPORT
 */
TEST_F(UtAicpuTsHandleDispatchAllStreams, Ut_HandleDispatchAllStreams_When_DispatchAllStreamsReturnsNotSupport_Expect_ReturnNotSupport)
{
    MOCKER(DispatchAllStreams)
        .stubs()
        .will(returnValue(HCCL_E_NOT_SUPPORT));

    HcclResult res = HandleDispatchAllStreams();
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}
