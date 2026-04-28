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
#include "interface_hccl.h"

class InterfaceHccpStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(InterfaceHccpStructInitTest, Ut_TypicalQpStructInit_Expect_ZeroInitialized)
{
    struct TypicalQp qpInfo = {};
    EXPECT_EQ(qpInfo.qpn, 0U);
    EXPECT_EQ(qpInfo.gidIdx, 0U);
    EXPECT_EQ(qpInfo.psn, 0U);
    EXPECT_EQ(qpInfo.sl, 0U);
    EXPECT_EQ(qpInfo.tc, 0U);
}

TEST_F(InterfaceHccpStructInitTest, Ut_TmStructInit_Expect_ZeroInitialized)
{
    struct tm errTime = {};
    EXPECT_EQ(errTime.tm_sec, 0);
    EXPECT_EQ(errTime.tm_min, 0);
    EXPECT_EQ(errTime.tm_hour, 0);
    EXPECT_EQ(errTime.tm_mday, 0);
    EXPECT_EQ(errTime.tm_mon, 0);
    EXPECT_EQ(errTime.tm_year, 0);
}

TEST_F(InterfaceHccpStructInitTest, Ut_TmBraceInit_Expect_AllFieldsZero)
{
    struct tm errTime = {};
    EXPECT_EQ(errTime.tm_wday, 0);
    EXPECT_EQ(errTime.tm_yday, 0);
    EXPECT_EQ(errTime.tm_isdst, 0);
}

TEST_F(InterfaceHccpStructInitTest, Ut_TypicalQpSetFields_Expect_FieldsSetCorrectly)
{
    struct TypicalQp qpInfo = {};
    qpInfo.qpn = 100;
    qpInfo.gidIdx = 1;
    qpInfo.psn = 200;
    EXPECT_EQ(qpInfo.qpn, 100U);
    EXPECT_EQ(qpInfo.gidIdx, 1U);
    EXPECT_EQ(qpInfo.psn, 200U);
}
