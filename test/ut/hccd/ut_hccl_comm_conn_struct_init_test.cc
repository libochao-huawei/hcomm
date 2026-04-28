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
#include "hccl_comm_conn.h"
#include "hccp_common.h"

class HcclCommConnStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommConnStructInitTest, Ut_SocketListenInfoStructInit_Expect_ZeroInitialized)
{
    struct SocketListenInfoT serverInfo = {};
    EXPECT_EQ(serverInfo.socketHandle, nullptr);
    EXPECT_EQ(serverInfo.port, 0U);
}

TEST_F(HcclCommConnStructInitTest, Ut_SocketListenInfoBraceInit_Expect_AllFieldsZero)
{
    struct SocketListenInfoT serverInfo = {};
    EXPECT_EQ(serverInfo.socketHandle, nullptr);
    EXPECT_EQ(serverInfo.port, 0U);
}

TEST_F(HcclCommConnStructInitTest, Ut_SocketListenInfoSetFields_Expect_FieldsSetCorrectly)
{
    struct SocketListenInfoT serverInfo = {};
    serverInfo.socketHandle = reinterpret_cast<void*>(0x1000);
    serverInfo.port = 8080;
    EXPECT_EQ(serverInfo.socketHandle, reinterpret_cast<void*>(0x1000));
    EXPECT_EQ(serverInfo.port, 8080U);
}
