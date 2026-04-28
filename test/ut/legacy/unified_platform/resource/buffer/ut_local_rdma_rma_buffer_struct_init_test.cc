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
#include "local_rdma_rma_buffer.h"

class LocalRdmaRmaBufferStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(LocalRdmaRmaBufferStructInitTest, Ut_MrInfoTStructInit_Expect_ZeroInitialized)
{
    struct MrInfoT mrInfo = {};
    EXPECT_EQ(mrInfo.addr, nullptr);
    EXPECT_EQ(mrInfo.size, 0U);
    EXPECT_EQ(mrInfo.access, 0U);
}

TEST_F(LocalRdmaRmaBufferStructInitTest, Ut_MrInfoTBraceInit_Expect_AllFieldsZero)
{
    struct MrInfoT mrInfo = {};
    EXPECT_EQ(mrInfo.addr, nullptr);
    EXPECT_EQ(mrInfo.size, 0U);
    EXPECT_EQ(mrInfo.access, 0U);
}

TEST_F(LocalRdmaRmaBufferStructInitTest, Ut_MrInfoTSetFields_Expect_FieldsSetCorrectly)
{
    struct MrInfoT mrInfo = {};
    void* addr = reinterpret_cast<void*>(0x1000);
    mrInfo.addr = addr;
    mrInfo.size = 4096;
    mrInfo.access = RA_ACCESS_REMOTE_WRITE | RA_ACCESS_LOCAL_WRITE | RA_ACCESS_REMOTE_READ;
    EXPECT_EQ(mrInfo.addr, addr);
    EXPECT_EQ(mrInfo.size, 4096U);
    EXPECT_EQ(mrInfo.access, 7U);
}
