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
#include "host_cpu_roce_channel.h"

class HostCpuRoceChannelStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(HostCpuRoceChannelStructInitTest, Ut_IbvSgeStructInit_Expect_ZeroInitialized)
{
    struct ibv_sge sg = {};
    EXPECT_EQ(sg.addr, 0U);
    EXPECT_EQ(sg.length, 0U);
    EXPECT_EQ(sg.lkey, 0U);
}

TEST_F(HostCpuRoceChannelStructInitTest, Ut_IbvSgeBraceInit_Expect_AllFieldsZero)
{
    struct ibv_sge sg = {};
    EXPECT_EQ(sg.addr, 0ULL);
    EXPECT_EQ(sg.length, 0ULL);
}

TEST_F(HostCpuRoceChannelStructInitTest, Ut_IbvSgeSetFields_Expect_FieldsSetCorrectly)
{
    struct ibv_sge sg = {};
    sg.addr = 0x1000;
    sg.length = 1024;
    sg.lkey = 100;
    EXPECT_EQ(sg.addr, 0x1000ULL);
    EXPECT_EQ(sg.length, 1024ULL);
    EXPECT_EQ(sg.lkey, 100U);
}

TEST_F(HostCpuRoceChannelStructInitTest, Ut_IbvSendWrStructInit_Expect_ZeroInitialized)
{
    struct ibv_send_wr wr = {};
    EXPECT_EQ(wr.wr_id, 0U);
    EXPECT_EQ(wr.num_sge, 0U);
    EXPECT_EQ(wr.opcode, IBV_WR_RDMA_WRITE);
}
