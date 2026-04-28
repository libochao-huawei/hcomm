/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for the details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <ctime>
#include "flush_manager.h"

class FlushManagerStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(FlushManagerStructInitTest, Ut_TimespecStartStructInit_Expect_ZeroInitialized)
{
    struct timespec start = {};
    EXPECT_EQ(start.tv_sec, 0);
    EXPECT_EQ(start.tv_nsec, 0);
}

TEST_F(FlushManagerStructInitTest, Ut_TimespecCurrentStructInit_Expect_ZeroInitialized)
{
    struct timespec current = {};
    EXPECT_EQ(current.tv_sec, 0);
    EXPECT_EQ(current.tv_nsec, 0);
}

TEST_F(FlushManagerStructInitTest, Ut_TimespecBraceInit_Expect_AllFieldsZero)
{
    struct timespec start = {};
    struct timespec current = {};
    EXPECT_EQ(start.tv_sec, current.tv_sec);
    EXPECT_EQ(start.tv_nsec, current.tv_nsec);
}

TEST_F(FlushManagerStructInitTest, Ut_TimespecSetValue_Expect_CorrectValue)
{
    struct timespec start = {};
    clock_gettime(CLOCK_MONOTONIC, &start);
    EXPECT_GT(start.tv_sec, 0);
    EXPECT_GE(start.tv_nsec, 0);
}
