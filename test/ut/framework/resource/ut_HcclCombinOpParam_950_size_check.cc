/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mc2_type.h"
using namespace Hccl;

class HcclCombinOpParam950StructTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HcclCombinOpParam950StructTest, TestHcclCombinOpParamSize)
{
    EXPECT_EQ(sizeof(HcclCombinOpParam), 9320u);
}

TEST_F(HcclCombinOpParam950StructTest, TestHcclCombinOpParamFieldOffsets)
{
    EXPECT_EQ(offsetof(HcclCombinOpParam, workSpace), 0u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, workSpaceSize), 8u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, rankId), 16u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, rankDim), 20u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, winSize), 24u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsIn), 32u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsOut), 544u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, xnAddr), 1056u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, ckeAddr), 1064u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, msAddr), 1072u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, msSize), 1080u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, opType), 1088u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, algorithmType), 1120u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, wq), 1128u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, cq), 6248u);
}

TEST_F(HcclCombinOpParam950StructTest, TestHcclCombinOpParamFieldSizes)
{
    EXPECT_EQ(sizeof(HcclAiRMAWQ), 80u);
    EXPECT_EQ(sizeof(HcclAiRMACQ), 48u);
}
