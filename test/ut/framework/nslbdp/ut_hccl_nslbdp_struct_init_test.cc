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
#include <sys/socket.h>
#include <netinet/in.h>
#include "hccl_nslbdp.h"

class HcclNslbdpStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(HcclNslbdpStructInitTest, Ut_SockaddrInStructInit_Expect_ZeroInitialized)
{
    struct sockaddr_in sa = {};
    EXPECT_EQ(sa.sin_family, 0);
    EXPECT_EQ(sa.sin_port, 0);
    EXPECT_EQ(sa.sin_addr.s_addr, 0);
}

TEST_F(HcclNslbdpStructInitTest, Ut_SockaddrInBraceInit_Expect_AllFieldsZero)
{
    struct sockaddr_in sa = {};
    EXPECT_EQ(sa.sin_family, AF_UNSPEC);
    EXPECT_EQ(sa.sin_addr.s_addr, 0);
}

TEST_F(HcclNslbdpStructInitTest, Ut_SockaddrInSetValue_Expect_CorrectValue)
{
    struct sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(8080);
    inet_pton(AF_INET, "192.168.1.1", &(sa.sin_addr));

    EXPECT_EQ(sa.sin_family, AF_INET);
    EXPECT_EQ(sa.sin_port, htons(8080));
    EXPECT_EQ(sa.sin_addr.s_addr, inet_addr("192.168.1.1"));
}

TEST_F(HcclNslbdpStructInitTest, Ut_IpToUint32_Expect_CorrectConversion)
{
    hcclNslbDp nslbdp;
    std::string ip = "192.168.1.1";
    u32 ipValue = nslbdp.ipToUint32(ip);
    EXPECT_EQ(ipValue, ntohl(inet_addr("192.168.1.1")));
}
