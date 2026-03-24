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
#include <mockcpp/mockcpp.hpp>
#include <string>
#include <unordered_map>

#include "op_unfold_key.h"

using namespace std;
using namespace hccl;

class OpUnfoldKeyTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "OpUnfoldKeyTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "OpUnfoldKeyTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

// 测试默认构造函数
TEST_F(OpUnfoldKeyTest, ut_DefaultConstructor_Expect_DefaultValues)
{
    OpUnfoldKey key;
    EXPECT_EQ(key.opType, HcclCMDType::HCCL_CMD_INVALID);
    EXPECT_EQ(key.dataType, HcclDataType::HCCL_DATA_TYPE_RESERVED);
    EXPECT_EQ(key.reduceType, HcclReduceOp::HCCL_REDUCE_RESERVED);
    EXPECT_EQ(key.isZeroCopy, false);
    EXPECT_EQ(key.inputSize, 0);
    EXPECT_EQ(key.isInplacePreSync, false);
    EXPECT_EQ(key.workflowMode, HcclWorkflowMode::HCCL_WORKFLOW_MODE_RESERVED);
    EXPECT_EQ(key.isCapture, false);
}

// 测试拷贝构造函数
TEST_F(OpUnfoldKeyTest, ut_CopyConstructor_Expect_CopiedValues)
{
    OpUnfoldKey key1;
    key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    OpUnfoldKey key2(key1);
    EXPECT_EQ(key2.opType, HcclCMDType::HCCL_CMD_ALLREDUCE);
    EXPECT_EQ(key2.dataType, HcclDataType::HCCL_DATA_TYPE_FP32);
    EXPECT_EQ(key2.reduceType, HcclReduceOp::HCCL_REDUCE_SUM);
    EXPECT_EQ(key2.isZeroCopy, true);
    EXPECT_EQ(key2.inputSize, 1024);
    EXPECT_EQ(key2.isInplacePreSync, false);
    EXPECT_EQ(key2.workflowMode, HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    EXPECT_EQ(key2.isCapture, true);
}

// 测试 Init 函数
TEST_F(OpUnfoldKeyTest, ut_Init_WithValidParams_Expect_Success)
{
    OpUnfoldKey key;
    HcclResult ret = key.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(key.opType, HcclCMDType::HCCL_CMD_ALLREDUCE);
    EXPECT_EQ(key.dataType, HcclDataType::HCCL_DATA_TYPE_FP32);
    EXPECT_EQ(key.reduceType, HcclReduceOp::HCCL_REDUCE_SUM);
    EXPECT_EQ(key.isZeroCopy, true);
    EXPECT_EQ(key.inputSize, 1024);
    EXPECT_EQ(key.isInplacePreSync, false);
    EXPECT_EQ(key.workflowMode, HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    EXPECT_EQ(key.isCapture, true);
}

// 测试 GetKeyString 函数
TEST_F(OpUnfoldKeyTest, ut_GetKeyString_Expect_ValidStringFormat)
{
    OpUnfoldKey key;
    key.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
             HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
             HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    std::string keyStr = key.GetKeyString();
    // 验证字符串包含必要的字段信息
    EXPECT_NE(keyStr.find("opType"), std::string::npos);
    EXPECT_NE(keyStr.find("dataType"), std::string::npos);
    EXPECT_NE(keyStr.find("reduceType"), std::string::npos);
    EXPECT_NE(keyStr.find("isZeroCopy"), std::string::npos);
    EXPECT_NE(keyStr.find("inputSize"), std::string::npos);
    EXPECT_NE(keyStr.find("isInplacePreSync"), std::string::npos);
    EXPECT_NE(keyStr.find("workflowMode"), std::string::npos);
    EXPECT_NE(keyStr.find("isCapture"), std::string::npos);
}

// 测试 operator== - 相同的 key
TEST_F(OpUnfoldKeyTest, ut_OperatorEqual_WithSameKey_Expect_True)
{
    OpUnfoldKey key1;
    key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    OpUnfoldKey key2;
    key2.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    EXPECT_TRUE(key1 == key2);
}

// 测试 operator= 赋值操作符
TEST_F(OpUnfoldKeyTest, OperatorAssign_Expect_CopiedValues)
{
    OpUnfoldKey key1;
    key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    OpUnfoldKey key2;
    key2 = key1;

    EXPECT_EQ(key2.opType, HcclCMDType::HCCL_CMD_ALLREDUCE);
    EXPECT_EQ(key2.dataType, HcclDataType::HCCL_DATA_TYPE_FP32);
    EXPECT_EQ(key2.reduceType, HcclReduceOp::HCCL_REDUCE_SUM);
    EXPECT_EQ(key2.isZeroCopy, true);
    EXPECT_EQ(key2.inputSize, 1024);
    EXPECT_EQ(key2.isInplacePreSync, false);
    EXPECT_EQ(key2.workflowMode, HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    EXPECT_EQ(key2.isCapture, true);
}
