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
#include <stdexcept>
#include <vector>

#include "hcomm_c_adpt.h"
#include "resource_entities.h"
#include "orion_adapter_rts.h"

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#undef private

using namespace hccl;

class RecordServiceTest : public testing::Test {
protected:
    void SetUp() override
    {
        // Create AicpuTsThread via public API (AICPU_TS + TS → handle = AicpuTsThread*)
        ThreadConfig config = {};
        config.notifyNumPerThread = kNotifyNum;
        HcclResult ret = HcommThreadAllocWithConfig(
            COMM_ENGINE_AICPU_TS, 1, config, THREAD_TYPE_TS, &dstThreadHandle_);
        ASSERT_EQ(ret, HCCL_SUCCESS);

        auto *tsThreadEntPtr = reinterpret_cast<ThreadEntity *>(dstThreadHandle_);
        auto *tsThreadDevPtr = reinterpret_cast<AicpuTsThread *>(tsThreadEntPtr->threadObjAddr);
        tsThreadDevPtr->devType_ = DevType::DEV_TYPE_950;
        tsThreadDevPtr->pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();

        // Fill default valid args
        memset(&args_, 0, sizeof(args_));
        args_.threadHandle = static_cast<ThreadHandle>(0xABCD); // src handle (not looked up)
        args_.dstThreadHandle = dstThreadHandle_;
        args_.dstNotifyIdx = 0;
    }

    void TearDown() override
    {
        delete reinterpret_cast<ThreadEntity *>(dstThreadHandle_); // Clean up new ThreadEntity in GetThreadEntity
        GlobalMockObject::verify();
    }

    static constexpr uint32_t kNotifyNum = 2;
    ThreadHandle dstThreadHandle_{};
    RecordServiceArgs args_{};
};

// test_01: Normal → HCCL_SUCCESS
TEST_F(RecordServiceTest, Ut_RecordService_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER_CPP(&LocalNotify::GetNotifyData)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(Hccl::HrtMemcpy).stubs();

    HcclResult ret = RecordService(&args_, sizeof(RecordServiceArgs));
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// test_02: args is nullptr → HCCL_E_PTR
TEST_F(RecordServiceTest, Ut_RecordService_When_ArgsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    HcclResult ret = RecordService(nullptr, sizeof(RecordServiceArgs));
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// test_03: argsSizeByte mismatch → HCCL_E_PARA
TEST_F(RecordServiceTest, Ut_RecordService_When_ArgsSizeByteInvalid_Expect_ReturnIsHCCL_E_PARA)
{
    HcclResult ret = RecordService(&args_, 0);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// test_04: dstThreadHandle not in g_ThreadMap → HCCL_E_NOT_FOUND
TEST_F(RecordServiceTest, Ut_RecordService_When_DstThreadHandleNotFoundInMap_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    args_.dstThreadHandle = static_cast<ThreadHandle>(0xDEAD);
    HcclResult ret = RecordService(&args_, sizeof(RecordServiceArgs));
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

// test_05: dstThread is not AicpuTsThread → HCCL_E_PARA (dynamic_cast fails)
TEST_F(RecordServiceTest, Ut_RecordService_When_DstThreadIsNotAicpuTsThread_Expect_ReturnIsHCCL_E_PARA)
{
    ThreadHandle cpuHandle{};
    ThreadConfig wrongConfig = {};
    wrongConfig.notifyNumPerThread = 1;
    HcclResult allocRet = HcommThreadAllocWithConfig(
        COMM_ENGINE_AICPU, 1, wrongConfig, THREAD_TYPE_CPU, &cpuHandle);
    ASSERT_EQ(allocRet, HCCL_SUCCESS);

    args_.dstThreadHandle = cpuHandle;
    HcclResult ret = RecordService(&args_, sizeof(RecordServiceArgs));
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// test_06: dstNotifyIdx out of bounds → HCCL_E_PTR
TEST_F(RecordServiceTest, Ut_RecordService_When_DstNotifyIdxOutOfBounds_Expect_ReturnIsHCCL_E_PTR)
{
    args_.dstNotifyIdx = kNotifyNum; // out of range [0, kNotifyNum)
    HcclResult ret = RecordService(&args_, sizeof(RecordServiceArgs));
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// test_07: GetNotifyData fails → error code propagated
TEST_F(RecordServiceTest, Ut_RecordService_When_GetNotifyDataFails_Expect_ReturnIsErrorCode)
{
    MOCKER_CPP(&LocalNotify::GetNotifyData)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = RecordService(&args_, sizeof(RecordServiceArgs));
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// test_08: HrtMemcpy throws exception → error code via TRY_CATCH_RETURN
TEST_F(RecordServiceTest, Ut_RecordService_When_HrtMemcpyFails_Expect_ReturnIsErrorCode)
{
    MOCKER_CPP(&LocalNotify::GetNotifyData)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(Hccl::HrtMemcpy).stubs().will(throws(std::runtime_error("Mocked exception")));

    HcclResult ret = RecordService(&args_, sizeof(RecordServiceArgs));
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
