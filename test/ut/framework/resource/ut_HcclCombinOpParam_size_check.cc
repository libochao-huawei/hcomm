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
#include "aicpu_operator_pub.h"

class HcclCombinOpParamStructTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HcclCombinOpParamStructTest, TestHcclCombinOpParamSize)
{
    EXPECT_EQ(sizeof(HcclCombinOpParam), 8944u);
}

TEST_F(HcclCombinOpParamStructTest, TestHcclCombinOpParamFieldOffsets)
{
    EXPECT_EQ(offsetof(HcclCombinOpParam, mc2WorkSpace), 0u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, rankId), 16u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, rankNum), 20u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, winSize), 24u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsIn), 32u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsOut), 288u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, xnAddr), 544u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, ckeAddr), 552u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, msAddr), 560u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, msSize), 568u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, opType), 576u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, algorithmType), 608u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, wq), 616u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, cq), 3744u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, streamInfo), 6872u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, signalInfo), 7192u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, config), 8808u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, overFlowAddr), 8864u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, onlyRead), 8872u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, kfcControlTransferH2DParams), 8876u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, kfcStatusTransferD2HParams), 8896u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, padding), 8906u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, winExpSize), 8924u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, windowsExp), 8932u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, multiServerFlag), 8872u);
    EXPECT_EQ(offsetof(HcclCombinOpParam, ibverbsData), 8880u);
}

TEST_F(HcclCombinOpParamStructTest, TestHcclCombinOpParamFieldSizes)
{
    EXPECT_EQ(sizeof(HcclMC2WorkSpace), 16u);
    EXPECT_EQ(sizeof(HcclStreamInfo), 16u);
    EXPECT_EQ(sizeof(HcclCombinOpSignalParam), 616u);
    EXPECT_EQ(sizeof(HcclOpConfig), 56u);
    EXPECT_EQ(sizeof(HcclAiRMAWQ), 72u);
    EXPECT_EQ(sizeof(HcclAiRMACQ), 56u);
}
