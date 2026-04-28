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
#include "rank_info_detect_client.h"
#include "hccp_common.h"

class RankInfoDetectClientStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(RankInfoDetectClientStructInitTest, Ut_RaInfoStructInit_Expect_ZeroInitialized)
{
    struct RaInfo raInfo = {};
    EXPECT_EQ(raInfo.mode, static_cast<s32>(0));
    EXPECT_EQ(raInfo.phyId, 0U);
}

TEST_F(RankInfoDetectClientStructInitTest, Ut_RaInfoBraceInit_Expect_AllFieldsZero)
{
    struct RaInfo raInfo = {};
    EXPECT_EQ(raInfo.mode, NETWORK_OFFLINE);
    EXPECT_EQ(raInfo.phyId, 0U);
}

TEST_F(RankInfoDetectClientStructInitTest, Ut_RaInfoSetFields_Expect_FieldsSetCorrectly)
{
    struct RaInfo raInfo = {};
    raInfo.mode = NETWORK_MODE_TCP;
    raInfo.phyId = 1;
    EXPECT_EQ(raInfo.mode, NETWORK_MODE_TCP);
    EXPECT_EQ(raInfo.phyId, 1U);
}
