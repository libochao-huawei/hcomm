/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
// A2对应的数据结构
#include "aicpu_operator_pub.h"

class HcclCombinOpParamStructTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HcclCombinOpParamStructTest, TestHcclCombinOpParamSize)
{
    EXPECT_EQ(sizeof(HcclCombinOpParam), 8928u);
}

TEST_F(HcclCombinOpParamStructTest, TestHcclCombinOpParamFieldOffsets)
{
    EXPECT_EQ(offsetof(HcclCombinOpParam, mc2WorkSpace), 0u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, rankId), 16u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, rankNum), 20u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, winSize), 24u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsIn), 32u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsOut), 288u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, hcomId), 544u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, streamInfo), 672u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, signalInfo), 1184u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, config), 8448u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, overFlowAddr), 8496u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, onlyRead),8504u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, kfcControlTransferH2DParams), 8512u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, kfcStatusTransferD2HParams), 8552u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, padding), 8592u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, winExpSize), 8608u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsExp), 8616u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, multiServerFlag), 8872u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, ibverbsData), 8880u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, ibverbsDataSize), 8888u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, sizeOfAiRMAInfo), 8896u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, aiRMAInfo), 8904u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, capabilityPtr), 8912u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, capabilitySize), 8920u);
}

TEST_F(HcclCombinOpParamStructTest, TestHcclCombinOpParamFieldSizes)
{
    EXPECT_EQ(sizeof(HcclMC2WorkSpace), 16u);
    EXPECT_EQ(sizeof(HcclStreamInfo), 16u);
    EXPECT_EQ(sizeof(HcclCombinOpSignalParam), 7264u);
    EXPECT_EQ(sizeof(HcclOpConfig), 48u);
    EXPECT_EQ(sizeof(HcclAiRMAWQ), 64u);
    EXPECT_EQ(sizeof(HcclAiRMACQ), 56u);
}
