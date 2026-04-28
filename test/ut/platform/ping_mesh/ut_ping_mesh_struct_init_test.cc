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
#include "ping_mesh.h"

class PingMeshStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(PingMeshStructInitTest, Ut_RpingEidHeadTmpStructInit_Expect_ZeroInitialized)
{
    RpingEidHead EidHeadTmp = {};
    EXPECT_EQ(EidHeadTmp.eid, 0U);
    EXPECT_EQ(EidHeadTmp.flag, 0U);
}

TEST_F(PingMeshStructInitTest, Ut_RpingEidHeadTmpBraceInit_Expect_AllFieldsZero)
{
    RpingEidHead EidHeadTmp = {};
    EXPECT_EQ(EidHeadTmp.eid, 0U);
    EXPECT_EQ(EidHeadTmp.flag, 0U);
}

TEST_F(PingMeshStructInitTest, Ut_RpingEidHeadTmpSetFields_Expect_FieldsSetCorrectly)
{
    RpingEidHead EidHeadTmp = {};
    EidHeadTmp.eid = 123;
    EidHeadTmp.flag = 1;
    EXPECT_EQ(EidHeadTmp.eid, 123U);
    EXPECT_EQ(EidHeadTmp.flag, 1U);
}
