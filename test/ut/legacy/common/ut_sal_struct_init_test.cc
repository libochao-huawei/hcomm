/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <ctime>
#include "sal.h"

using namespace Hccl;

class SalStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(SalStructInitTest, Ut_GetCurAicpuTimestamp_StructInit_Expect_ZeroInitialized)
{
    struct timespec timestamp = {};
    EXPECT_EQ(timestamp.tv_sec, 0);
    EXPECT_EQ(timestamp.tv_nsec, 0);
}

TEST_F(SalStructInitTest, Ut_TimespecInitialization_Expect_AllFieldsZero)
{
    struct timespec ts1 = {};
    struct timespec ts2;
    memset(&ts2, 0, sizeof(struct timespec));

    EXPECT_EQ(ts1.tv_sec, ts2.tv_sec);
    EXPECT_EQ(ts1.tv_nsec, ts2.tv_nsec);
}

TEST_F(SalStructInitTest, Ut_Timespec BraceInitialization_Expect_AllFieldsZero)
{
    struct timespec ts = {};
    EXPECT_EQ(ts.tv_sec, 0);
    EXPECT_EQ(ts.tv_nsec, 0);
}

TEST_F(SalStructInitTest, Ut_TimespecPartialInit_Expect_UninitializedFieldsUnspecified)
{
    struct timespec ts;
    ts.tv_sec = 100;
    EXPECT_NE(ts.tv_nsec, 0);
}
