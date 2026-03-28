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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include <cstring>
#include <vector>

#include "hcomm_c_adpt.h"
#include "resource_entities.h"
#include "notifys/mem_notify.h"

using namespace hccl;

class WaitServiceTest : public testing::Test {
protected:
    void SetUp() override
    {
        // Create CpuThread via public API (AICPU + CPU → CpuThread)
        ThreadConfig config = {};
        config.notifyNumPerThread = kNotifyNum;
        HcclResult ret = HcommThreadAllocWithConfig(
            COMM_ENGINE_AICPU, 1, config, THREAD_TYPE_CPU, &threadHandle_);
        ASSERT_EQ(ret, HCCL_SUCCESS);

        // Fill default valid args
        memset(&args_, 0, sizeof(args_));
        args_.threadHandle = threadHandle_;
        args_.notifyIdx = 0;
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
    }

    static constexpr uint32_t kNotifyNum = 2;
    ThreadHandle threadHandle_{};
    WaitServiceArgs args_{};
};

// test_01: Normal → HCCL_SUCCESS
TEST_F(WaitServiceTest, Ut_WaitService_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER_CPP(&MemNotify::Wait)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = WaitService(&args_, sizeof(WaitServiceArgs));
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// test_02: args is nullptr → HCCL_E_PTR
TEST_F(WaitServiceTest, Ut_WaitService_When_ArgsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    HcclResult ret = WaitService(nullptr, sizeof(WaitServiceArgs));
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// test_03: argsSizeByte mismatch → HCCL_E_PARA
TEST_F(WaitServiceTest, Ut_WaitService_When_ArgsSizeByteInvalid_Expect_ReturnIsHCCL_E_PARA)
{
    HcclResult ret = WaitService(&args_, 0);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// test_04: threadHandle not in g_ThreadMap → HCCL_E_NOT_FOUND
TEST_F(WaitServiceTest, Ut_WaitService_When_ThreadHandleNotFoundInMap_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    args_.threadHandle = static_cast<ThreadHandle>(0xDEAD);
    HcclResult ret = WaitService(&args_, sizeof(WaitServiceArgs));
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

// test_05: thread is not CpuThread → HCCL_E_PARA (dynamic_cast fails)
TEST_F(WaitServiceTest, Ut_WaitService_When_ThreadIsNotCpuThread_Expect_ReturnIsHCCL_E_PARA)
{
    // Create an AicpuTsThread (not CpuThread) via HcommThreadAllocWithConfig.
    ThreadHandle tsHandle{};
    ThreadConfig wrongConfig = {};
    wrongConfig.notifyNumPerThread = 1;
    HcclResult allocRet = HcommThreadAllocWithConfig(
        COMM_ENGINE_AICPU_TS, 1, wrongConfig, THREAD_TYPE_TS, &tsHandle);
    ASSERT_EQ(allocRet, HCCL_SUCCESS);

    args_.threadHandle = tsHandle;
    HcclResult ret = WaitService(&args_, sizeof(WaitServiceArgs));
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// test_06: notifyIdx out of bounds → HCCL_E_PTR (GetMemNotify returns nullptr)
TEST_F(WaitServiceTest, Ut_WaitService_When_NotifyIdxOutOfBounds_Expect_ReturnIsHCCL_E_PTR)
{
    args_.notifyIdx = kNotifyNum; // out of range [0, kNotifyNum)
    HcclResult ret = WaitService(&args_, sizeof(WaitServiceArgs));
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// test_07: MemNotify::Wait fails → error code propagated
TEST_F(WaitServiceTest, Ut_WaitService_When_MemNotifyWaitFails_Expect_ReturnIsWaitErrorCode)
{
    MOCKER_CPP(&MemNotify::Wait)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = WaitService(&args_, sizeof(WaitServiceArgs));
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
