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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dev_ub_connection.h"

class DevUbConnectionStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(DevUbConnectionStructInitTest, Ut_InAddrStructInit_Expect_ZeroInitialized)
{
    struct in_addr addr = {};
    EXPECT_EQ(addr.s_addr, 0);
}

TEST_F(DevUbConnectionStructInitTest, Ut_InAddrBraceInit_Expect_ZeroInitialized)
{
    struct in_addr addr = {};
    EXPECT_EQ(addr.s_addr, inet_addr("0.0.0.0"));
}

TEST_F(DevUbConnectionStructInitTest, Ut_InAddrSetValue_Expect_CorrectValue)
{
    struct in_addr addr;
    addr.s_addr = inet_addr("192.168.1.1");
    EXPECT_EQ(addr.s_addr, inet_addr("192.168.1.1"));
}
