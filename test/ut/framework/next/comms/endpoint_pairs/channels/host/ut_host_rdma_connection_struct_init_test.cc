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
#include <infiniband/verbs.h>
#include "host_rdma_connection.h"

class HostRdmaConnectionStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(HostRdmaConnectionStructInitTest, Ut_QpAttrStructInit_Expect_ZeroInitialized)
{
    struct QpAttr localQpAttr = {};
    EXPECT_EQ(localQpAttr.qpn, 0U);
    EXPECT_EQ(localQpAttr.qpState, 0);
    EXPECT_EQ(localQpAttr.curQpsState, 0);
}

TEST_F(HostRdmaConnectionStructInitTest, Ut_TypicalQpStructInit_Expect_ZeroInitialized)
{
    struct TypicalQp localQp = {};
    EXPECT_EQ(localQp.qpn, 0U);
    EXPECT_EQ(localQp.gidIdx, 0U);
    EXPECT_EQ(localQp.sl, 0U);
    EXPECT_EQ(localQp.tc, 0U);
    EXPECT_EQ(localQp.retryCnt, 0U);
}

TEST_F(HostRdmaConnectionStructInitTest, Ut_TypicalQpBraceInit_Expect_AllFieldsZero)
{
    struct TypicalQp rmtQp = {};
    EXPECT_EQ(rmtQp.qpn, 0U);
    EXPECT_EQ(rmtQp.psn, 0U);
    EXPECT_EQ(rmtQp.mtu, 0U);
}

TEST_F(HostRdmaConnectionStructInitTest, Ut_TypicalQpSetFields_Expect_FieldsSetCorrectly)
{
    struct TypicalQp localQp = {};
    localQp.qpn = 100;
    localQp.sl = 1;
    localQp.tc = 2;
    localQp.retryCnt = 3;
    EXPECT_EQ(localQp.qpn, 100U);
    EXPECT_EQ(localQp.sl, 1U);
    EXPECT_EQ(localQp.tc, 2U);
    EXPECT_EQ(localQp.retryCnt, 3U);
}
