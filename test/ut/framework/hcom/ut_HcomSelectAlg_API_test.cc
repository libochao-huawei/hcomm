/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <memory>
#include <cstring>

#include "hccl/base.h"
#include "hccl/hccl_types.h"
#include "hcom_pub.h"
#include "hcom_common.h"
#include "comm_config_pub.h"
#include "common.h"

using namespace hccl;

/**
 * @brief HcomSelectAlg 函数测试类
 */
class HcomSelectAlgTest : public testing::Test {
protected:
    void SetUp() override {
        comm.reset(new (std::nothrow) hccl::hcclComm());
        if (!comm) {
            HCCL_ERROR("Failed to create hccl::hcclComm");
            return;
        }

        // 初始化测试参数
        commHandle = 0;
        group = const_cast<char*>("hccl_world_group");
        count = 1024;
        counts = nullptr;
        dataType = HCCL_DATA_TYPE_FP32;
        op = HCCL_REDUCE_SUM;
        opType = HCCL_CMD_ALLREDUCE;
        aivCoreLimit = 8;
        ifAiv = false;
        algName = new char[ALG_NAME_MAX_LEN];
        memset(algName, 0, ALG_NAME_MAX_LEN);
        testAlgName = "test_alg_name";
    }
    
    void TearDown() override {
        GlobalMockObject::verify();
        if (algName != nullptr) {
            delete[] algName;
            algName = nullptr;
        }
    }
    
    std::shared_ptr<hccl::hcclComm> comm;
    s64 commHandle;
    char* group;
    u64 count;
    void* counts;
    HcclDataType dataType;
    HcclReduceOp op;
    HcclCMDType opType;
    int32_t aivCoreLimit;
    bool ifAiv;
    char* algName;
    std::string testAlgName;
};

// 测试场景1：comm == COMM_VALUE_DEFAULT，group 有效，正常返回
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_CommDefaultAndGroupValid_Expect_Success) {
    // Mock HcomGetCommByGroup 返回有效的 hcclComm
    MOCKER(HcomGetCommByGroup)
    .stubs()
    .with(any(), outBound(comm))
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock SetWorkflowMode
    MOCKER(SetWorkflowMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm->HcclSelectAlg 返回成功
    MOCKER_CPP(&hccl::hcclComm::HcclSelectAlg)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBound(ifAiv), outBound(testAlgName))
    .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
}

// 测试场景2：comm != COMM_VALUE_DEFAULT，直接使用 hcclComm
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_CommNotDefault_Expect_Success) {
    // 设置非默认的 comm 值
    s64 validCommHandle = 0x12345678;
    
    // Mock SetWorkflowMode
    MOCKER(SetWorkflowMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm->HcclSelectAlg 返回成功
    MOCKER_CPP(&hccl::hcclComm::HcclSelectAlg)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBound(ifAiv), outBound(testAlgName))
    .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcomSelectAlg(validCommHandle, group, count, counts, dataType, op, opType, 
        aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
}

// 测试场景3：ifAiv 指针为空
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_IfAivIsNull_Expect_ParamError) {
    HcclResult result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, nullptr, algName);
    
    EXPECT_EQ(result, HCCL_E_PARA);
}

// 测试场景4：HcomGetCommByGroup 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_HcomGetCommByGroupFails_Expect_Error) {
    // Mock HcomGetCommByGroup 返回错误
    MOCKER(HcomGetCommByGroup)
    .stubs()
    .with(any(), outBound(comm))
    .will(returnValue(HCCL_E_PTR));
    
    HcclResult result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_PTR);
}

// 测试场景5：hcclComm->HcclSelectAlg 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_HcclSelectAlgFails_Expect_Error) {
    // Mock HcomGetCommByGroup 返回有效的 hcclComm
    MOCKER(HcomGetCommByGroup)
    .stubs()
    .with(any(), outBound(comm))
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock SetWorkflowMode
    MOCKER(SetWorkflowMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm->HcclSelectAlg 返回错误
    MOCKER_CPP(&hccl::hcclComm::HcclSelectAlg)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBound(ifAiv), outBound(testAlgName))
    .will(returnValue(HCCL_E_INTERNAL));
    
    HcclResult result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_INTERNAL);
}

// 测试场景6：不同数据类型的正常路径
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_NormalWithVariousDataTypes_Expect_Success) {
    // Mock HcomGetCommByGroup 返回有效的 hcclComm
    MOCKER(HcomGetCommByGroup)
    .stubs()
    .with(any(), outBound(comm))
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock SetWorkflowMode
    MOCKER(SetWorkflowMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm->HcclSelectAlg 返回成功
    MOCKER_CPP(&hccl::hcclComm::HcclSelectAlg)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBound(ifAiv), outBound(testAlgName))
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试 FP16 数据类型
    dataType = HCCL_DATA_TYPE_FP16;
    HcclResult result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
    
    // 重置 algName
    memset(algName, 0, ALG_NAME_MAX_LEN);
    
    // 测试 INT32 数据类型
    dataType = HCCL_DATA_TYPE_INT32;
    result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
}

// 测试场景7：AIV 场景
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_AivModeEnabled_Expect_Success) {
    // Mock HcomGetCommByGroup 返回有效的 hcclComm
    MOCKER(HcomGetCommByGroup)
    .stubs()
    .with(any(), outBound(comm))
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock SetWorkflowMode
    MOCKER(SetWorkflowMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm->HcclSelectAlg 返回成功，并设置 ifAiv 为 true
    bool aivEnabled = true;
    MOCKER_CPP(&hccl::hcclComm::HcclSelectAlg)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBound(aivEnabled), outBound(testAlgName))
    .will(returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_TRUE(ifAiv);
    EXPECT_STREQ(algName, testAlgName.c_str());
}

// 测试场景8：不同操作类型
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_DifferentOpTypes_Expect_Success) {
    // Mock HcomGetCommByGroup 返回有效的 hcclComm
    MOCKER(HcomGetCommByGroup)
    .stubs()
    .with(any(), outBound(comm))
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock SetWorkflowMode
    MOCKER(SetWorkflowMode)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm->HcclSelectAlg 返回成功
    MOCKER_CPP(&hccl::hcclComm::HcclSelectAlg)
    .stubs()
    .with(any(), any(), any(), any(), any(), any(), outBound(ifAiv), outBound(testAlgName))
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试 AllGather 操作类型
    opType = HCCL_CMD_ALLGATHER;
    HcclResult result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
    
    // 重置 algName
    memset(algName, 0, ALG_NAME_MAX_LEN);
    
    // 测试 ReduceScatter 操作类型
    opType = HCCL_CMD_REDUCE_SCATTER;
    result = HcomSelectAlg(static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT), group, count, 
        counts, dataType, op, opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
}