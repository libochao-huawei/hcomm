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
TEST_F(OpUnfoldKeyTest, OpUnfoldKey_DefaultConstructor_Expect_DefaultValues)
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
TEST_F(OpUnfoldKeyTest, OpUnfoldKey_CopyConstructor_Expect_CopiedValues)
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

// 测试 Init 函数 - 正常情况 (非V类算子)
TEST_F(OpUnfoldKeyTest, Init_WithValidParams_Expect_Success)
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

// 测试 Init 函数 - 正常情况 (V类算子 dataType 为 RESERVED)
TEST_F(OpUnfoldKeyTest, Init_WithVClassOp_Expect_Success)
{
    OpUnfoldKey key;
    HcclResult ret = key.Init(HcclCMDType::HCCL_CMD_ALLTOALLV, HcclDataType::HCCL_DATA_TYPE_RESERVED,
                              HcclReduceOp::HCCL_REDUCE_RESERVED, false, 2048, true,
                              HcclWorkflowMode::HCCL_WORKFLOW_MODE_GRAPH_BASE, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(key.opType, HcclCMDType::HCCL_CMD_ALLTOALLV);
    EXPECT_EQ(key.dataType, HcclDataType::HCCL_DATA_TYPE_RESERVED);
}

// 测试 Init 函数 - 无效 opType
TEST_F(OpUnfoldKeyTest, Init_WithInvalidOpType_Expect_Failure)
{
    OpUnfoldKey key;
    HcclResult ret = key.Init(HcclCMDType::HCCL_CMD_INVALID, HcclDataType::HCCL_DATA_TYPE_FP32,
                              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 测试 Init 函数 - 非V类算子 dataType 为 RESERVED
TEST_F(OpUnfoldKeyTest, Init_WithReservedDataTypeForNonVOp_Expect_Failure)
{
    OpUnfoldKey key;
    HcclResult ret = key.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_RESERVED,
                              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 测试 Init 函数 - workflowMode 为 RESERVED
TEST_F(OpUnfoldKeyTest, Init_WithReservedWorkflowMode_Expect_Failure)
{
    OpUnfoldKey key;
    HcclResult ret = key.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                              HcclWorkflowMode::HCCL_WORKFLOW_MODE_RESERVED, true);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// 测试 GetKeyString 函数
TEST_F(OpUnfoldKeyTest, GetKeyString_Expect_ValidStringFormat)
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
TEST_F(OpUnfoldKeyTest, OperatorEqual_WithSameKey_Expect_True)
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

// 测试 operator== - 不同的 key
TEST_F(OpUnfoldKeyTest, OperatorEqual_WithDifferentKey_Expect_False)
{
    OpUnfoldKey key1;
    key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    OpUnfoldKey key2;
    key2.Init(HcclCMDType::HCCL_CMD_ALLGATHER, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    EXPECT_FALSE(key1 == key2);
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

// 测试 self-assignment 场景
TEST_F(OpUnfoldKeyTest, OperatorAssign_SelfAssignment_Expect_NoChange)
{
    OpUnfoldKey key;
    key.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    key = key;  // 自赋值

    EXPECT_EQ(key.opType, HcclCMDType::HCCL_CMD_ALLREDUCE);
    EXPECT_EQ(key.dataType, HcclDataType::HCCL_DATA_TYPE_FP32);
}

// 测试 std::hash<OpUnfoldKey> - 相同 key 产生相同 hash 值
TEST_F(OpUnfoldKeyTest, HashFunction_SameKey_Expect_SameHash)
{
    OpUnfoldKey key1;
    key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    OpUnfoldKey key2;
    key2.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    std::hash<OpUnfoldKey> hashFunc;
    size_t hash1 = hashFunc(key1);
    size_t hash2 = hashFunc(key2);

    EXPECT_EQ(hash1, hash2);
}

// 测试 OpUnfoldKey 作为 unordered_map 的 key
TEST_F(OpUnfoldKeyTest, OpUnfoldKey_AsUnorderedMapKey_Expect_Works)
{
    std::unordered_map<OpUnfoldKey, int> keyMap;

    OpUnfoldKey key1;
    key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
              HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);

    OpUnfoldKey key2;
    key2.Init(HcclCMDType::HCCL_CMD_ALLGATHER, HcclDataType::HCCL_DATA_TYPE_FP16,
              HcclReduceOp::HCCL_REDUCE_RESERVED, false, 2048, true,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_GRAPH_BASE, false);

    keyMap[key1] = 100;
    keyMap[key2] = 200;

    EXPECT_EQ(keyMap[key1], 100);
    EXPECT_EQ(keyMap[key2], 200);
    EXPECT_EQ(keyMap.size(), 2);
}

// 测试 ALLTOALLVC 算子 (V类算子)
TEST_F(OpUnfoldKeyTest, Init_WithAllToAllVC_Expect_Success)
{
    OpUnfoldKey key;
    HcclResult ret = key.Init(HcclCMDType::HCCL_CMD_ALLTOALLVC, HcclDataType::HCCL_DATA_TYPE_RESERVED,
                              HcclReduceOp::HCCL_REDUCE_RESERVED, false, 4096, true,
                              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(key.opType, HcclCMDType::HCCL_CMD_ALLTOALLVC);
    EXPECT_EQ(key.dataType, HcclDataType::HCCL_DATA_TYPE_RESERVED);
}

// 测试边界值: inputSize 为 0
TEST_F(OpUnfoldKeyTest, Init_WithZeroInputSize_Expect_Success)
{
    OpUnfoldKey key;
    HcclResult ret = key.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                              HcclReduceOp::HCCL_REDUCE_SUM, true, 0, false,
                              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(key.inputSize, 0);
}

// 测试所有布尔字段组合
TEST_F(OpUnfoldKeyTest, Init_WithDifferentBoolCombinations_Expect_CorrectValues)
{
    // 测试所有布尔字段为 false
    OpUnfoldKey key1;
    key1.Init(HcclCMDType::HCCL_CMD_BROADCAST, HcclDataType::HCCL_DATA_TYPE_INT32,
              HcclReduceOp::HCCL_REDUCE_RESERVED, false, 512, false,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_GRAPH_BASE, false);
    EXPECT_EQ(key1.isZeroCopy, false);
    EXPECT_EQ(key1.isInplacePreSync, false);
    EXPECT_EQ(key1.isCapture, false);

    // 测试所有布尔字段为 true
    OpUnfoldKey key2;
    key2.Init(HcclCMDType::HCCL_CMD_REDUCE_SCATTER, HcclDataType::HCCL_DATA_TYPE_INT8,
              HcclReduceOp::HCCL_REDUCE_MAX, true, 512, true,
              HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(key2.isZeroCopy, true);
    EXPECT_EQ(key2.isInplacePreSync, true);
    EXPECT_EQ(key2.isCapture, true);
}

// 测试不同 workflowMode
TEST_F(OpUnfoldKeyTest, Init_WithDifferentWorkflowModes_Expect_Success)
{
    OpUnfoldKey key1;
    HcclResult ret1 = key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                                HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                                HcclWorkflowMode::HCCL_WORKFLOW_MODE_GRAPH_BASE, true);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    EXPECT_EQ(key1.workflowMode, HcclWorkflowMode::HCCL_WORKFLOW_MODE_GRAPH_BASE);

    OpUnfoldKey key2;
    HcclResult ret2 = key2.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                                HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                                HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret2, HCCL_SUCCESS);
    EXPECT_EQ(key2.workflowMode, HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
}

// 测试不同 dataType
TEST_F(OpUnfoldKeyTest, Init_WithDifferentDataTypes_Expect_Success)
{
    // FP16
    OpUnfoldKey key1;
    HcclResult ret1 = key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP16,
                                HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                                HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    EXPECT_EQ(key1.dataType, HcclDataType::HCCL_DATA_TYPE_FP16);

    // INT8
    OpUnfoldKey key2;
    HcclResult ret2 = key2.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_INT8,
                                HcclReduceOp::HCCL_REDUCE_SUM, true, 1024, false,
                                HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret2, HCCL_SUCCESS);
    EXPECT_EQ(key2.dataType, HcclDataType::HCCL_DATA_TYPE_INT8);
}

// 测试不同 reduceType
TEST_F(OpUnfoldKeyTest, Init_WithDifferentReduceTypes_Expect_Success)
{
    OpUnfoldKey key1;
    HcclResult ret1 = key1.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                                HcclReduceOp::HCCL_REDUCE_MAX, true, 1024, false,
                                HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    EXPECT_EQ(key1.reduceType, HcclReduceOp::HCCL_REDUCE_MAX);

    OpUnfoldKey key2;
    HcclResult ret2 = key2.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                                HcclReduceOp::HCCL_REDUCE_MIN, true, 1024, false,
                                HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret2, HCCL_SUCCESS);
    EXPECT_EQ(key2.reduceType, HcclReduceOp::HCCL_REDUCE_MIN);

    OpUnfoldKey key3;
    HcclResult ret3 = key3.Init(HcclCMDType::HCCL_CMD_ALLREDUCE, HcclDataType::HCCL_DATA_TYPE_FP32,
                                HcclReduceOp::HCCL_REDUCE_PROD, true, 1024, false,
                                HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, true);
    EXPECT_EQ(ret3, HCCL_SUCCESS);
    EXPECT_EQ(key3.reduceType, HcclReduceOp::HCCL_REDUCE_PROD);
}
