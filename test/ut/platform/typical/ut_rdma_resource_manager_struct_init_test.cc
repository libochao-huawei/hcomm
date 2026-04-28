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
#include "rdma_resource_manager.h"

class RdmaResourceManagerStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(RdmaResourceManagerStructInitTest, Ut_TmStructInit_Expect_ZeroInitialized)
{
    struct tm errTime = {};
    EXPECT_EQ(errTime.tm_sec, 0);
    EXPECT_EQ(errTime.tm_min, 0);
    EXPECT_EQ(errTime.tm_hour, 0);
    EXPECT_EQ(errTime.tm_mday, 0);
    EXPECT_EQ(errTime.tm_mon, 0);
    EXPECT_EQ(errTime.tm_year, 0);
}

TEST_F(RdmaResourceManagerStructInitTest, Ut_TmFromTimestamp_Expect_CorrectTime)
{
    time_t timestamp = 1609459200;
    struct tm errTime = {};
    localtime_r(&timestamp, &errTime);
    EXPECT_EQ(errTime.tm_year, 121);
    EXPECT_EQ(errTime.tm_mon, 0);
    EXPECT_EQ(errTime.tm_mday, 1);
}

TEST_F(RdmaResourceManagerStructInitTest, Ut_TmBraceInit_Expect_AllFieldsZero)
{
    struct tm errTime = {};
    EXPECT_EQ(errTime.tm_wday, 0);
    EXPECT_EQ(errTime.tm_yday, 0);
    EXPECT_EQ(errTime.tm_isdst, 0);
}
