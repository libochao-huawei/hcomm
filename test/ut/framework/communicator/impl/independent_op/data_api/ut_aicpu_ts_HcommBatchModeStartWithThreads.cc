/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * @file ut_aicpu_ts_HcommBatchModeStartWithThreads.cc
 * @brief HCCL HcommBatchModeStartWithThreads接口单元测试文件
 *
 * 本文件用于测试HCCL通信库中的HcommBatchModeStartWithThreads接口，
 * 该接口用于启动批量操作模式并指定线程列表。
 *
 * 测试的接口：HcommBatchModeStartWithThreads
 * - 功能：启动批量操作模式，并指定要添加的线程列表
 * - 参数：batchTag（批次标签）、threads（线程句柄列表）、threadCount（线程数量）
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#undef private

using namespace hccl;

class UtAicpuTsHcommBatchModeStartWithThreads : public testing::Test
{
protected:
    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommBatchModeStartWithThreads, Ut_HcommBatchModeStartWithThreads_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    const char* batchTag = "test_batch_with_threads";
    ThreadHandle threads[2];
    threads[0] = static_cast<ThreadHandle>(0x1001);
    threads[1] = static_cast<ThreadHandle>(0x1002);
    uint32_t threadCount = 2;

    res = HcommBatchModeStartWithThreads(batchTag, threads, threadCount);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommBatchModeStartWithThreads, Ut_HcommBatchModeStartWithThreads_When_ThreadsNull_Expect_ReturnError)
{
    const char* batchTag = "test_batch_null_threads";
    ThreadHandle* threads = nullptr;
    uint32_t threadCount = 1;

    res = HcommBatchModeStartWithThreads(batchTag, threads, threadCount);
    EXPECT_NE(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommBatchModeStartWithThreads, Ut_HcommBatchModeStartWithThreads_When_ThreadCountZero_Expect_ReturnHCCL_SUCCESS)
{
    const char* batchTag = "test_batch_zero_count";
    ThreadHandle threads[1];
    threads[0] = static_cast<ThreadHandle>(0x2001);
    uint32_t threadCount = 0;

    res = HcommBatchModeStartWithThreads(batchTag, threads, threadCount);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommBatchModeStartWithThreads, Ut_HcommBatchModeStartWithThreads_When_SingleThread_Expect_ReturnHCCL_SUCCESS)
{
    const char* batchTag = "test_batch_single_thread";
    ThreadHandle threads[1];
    threads[0] = static_cast<ThreadHandle>(0x3001);
    uint32_t threadCount = 1;

    res = HcommBatchModeStartWithThreads(batchTag, threads, threadCount);
    EXPECT_EQ(res, HCCL_SUCCESS);
}