/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mc2_type.h"
#include "data_type.h"
#include "reduce_op.h"
#include "hccl_types.h"

using namespace Hccl;

class HcclOpArgsStructTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HcclOpArgsStructTest, TestHcclOpArgsSize)
{
    EXPECT_EQ(sizeof(HcclOpArgs), 296u);
}

TEST_F(HcclOpArgsStructTest, TestHcclOpArgsFieldOffsets)
{
    EXPECT_EQ(offsetof(HcclOpArgs, srcDataType), 0u);
    EXPECT_EQ(offsetof(HcclOpArgs, dstDataType), 4u);
    EXPECT_EQ(offsetof(HcclOpArgs, reduceType), 8u);
    EXPECT_EQ(offsetof(HcclOpArgs, count), 16u);
    EXPECT_EQ(offsetof(HcclOpArgs, algConfig), 24u);
    EXPECT_EQ(offsetof(HcclOpArgs, commEngine), 152u);
    EXPECT_EQ(offsetof(HcclOpArgs, reverse), 160u);
}

TEST_F(HcclOpArgsStructTest, TestHcclOpArgsFieldSizes)
{
    EXPECT_EQ(sizeof(DataType), 4u);
    EXPECT_EQ(sizeof(ReduceOp), 4u);
    EXPECT_EQ(sizeof(HcclAccelerator), 1u);
}

TEST_F(HcclOpArgsStructTest, TestHcclOpArgsAlgConfigSize)
{
    HcclOpArgs opArgs;
    EXPECT_EQ(sizeof(opArgs.algConfig), 128u);
}

TEST_F(HcclOpArgsStructTest, TestHcclOpArgsInitFunction)
{
    HcclOpArgs opArgs;
    opArgs.Init();
    
    EXPECT_EQ(opArgs.srcDataType, DataType::FP16);
    EXPECT_EQ(opArgs.dstDataType, DataType::FP16);
    EXPECT_EQ(opArgs.reduceType, ReduceOp::SUM);
    EXPECT_EQ(opArgs.count, 0u);
}
