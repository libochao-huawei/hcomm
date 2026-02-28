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

// 声明外部函数，用于测试
extern HcclWorkflowMode GetWorkflowMode();
extern void SetWorkflowMode(HcclWorkflowMode mode);

/**
 * @brief HcomSelectAlg 函数测试类
 */
class HcomSelectAlgTest : public testing::Test {
protected:
    void SetUp() override {
        // 创建测试 comm 对象
        testComm = std::make_shared<hccl::hcclComm>();
        ASSERT_NE(testComm, nullptr) << "Failed to create hccl::hcclComm";
        
        // 初始化测试参数
        group = "hccl_world_group";
        count = 1024;
        counts = nullptr;
        dataType = HCCL_DATA_TYPE_FP32;
        op = HCCL_REDUCE_SUM;
        opType = HCCL_CMD_ALLREDUCE;
        aivCoreLimit = 4;
        ifAiv = false;
        memset(algName, 0, ALG_NAME_MAX_LEN);
        
        // 保存原始 workflow mode
        originalWorkflowMode = GetWorkflowMode();
    }
    
    void TearDown() override {
        // 清理所有 mock，避免互相影响
        GlobalMockObject::reset();
        GlobalMockObject::verify();
        
        // 恢复 workflow mode
        SetWorkflowMode(originalWorkflowMode);
        
        // 重置状态
        memset(algName, 0, ALG_NAME_MAX_LEN);
        ifAiv = false;
    }
    
    std::shared_ptr<hccl::hcclComm> testComm;
    const char *group;
    u64 count;
    void* counts;
    HcclDataType dataType;
    HcclReduceOp op;
    HcclCMDType opType;
    int32_t aivCoreLimit;
    bool ifAiv;
    char algName[ALG_NAME_MAX_LEN];
    HcclWorkflowMode originalWorkflowMode;
};

// ==================== 参数校验测试 ====================

// 测试 ifAiv 指针为空场景 - 覆盖 CHK_PTR_NULL(ifAiv)
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_IfAivIsNull_Expect_ParaError) {
    bool *nullIfAiv = nullptr;
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, nullIfAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_PARA);
}

// ==================== HcomExecSelectAlg 失败场景测试 ====================

// 测试 HcomExecSelectAlg 调用失败场景 - 覆盖 HCCLV2_FUNC_RUN 失败分支
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_HcomExecSelectAlgFail_Expect_Internal) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    
    // Mock HcomExecSelectAlg 返回失败
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_E_INTERNAL));
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_INTERNAL);
    // 验证 algName 没有被修改
    EXPECT_STREQ(algName, "");
}

// ==================== 非 default comm 场景测试 ====================

// 测试非 default comm 场景 - HcclSelectAlg 成功
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_NonDefaultComm_HcclSelectAlgSuccess_Expect_Success) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    std::string testAlgName = "test_ring_alg";
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 成功，并设置算法名
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(testAlgNameBuf, testAlgName.c_str());
}

// 测试非 default comm 场景 - HcclSelectAlg 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_NonDefaultComm_HcclSelectAlgFail_Expect_NotSupport) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 失败
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_E_NOT_SUPPORT));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_E_NOT_SUPPORT);
    // 验证 algName 没有被修改
    EXPECT_STREQ(testAlgNameBuf, "");
}

// 测试非 default comm 场景 - 空 group 参数
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_NonDefaultComm_NullGroup_Expect_Success) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    std::string testAlgName = "test_alg_with_null_group";
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 成功
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, nullptr, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(testAlgNameBuf, testAlgName.c_str());
}

// ==================== default comm 场景测试 ====================

// 测试 default comm 场景 - HcclSelectAlg 成功
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_DefaultComm_HcclSelectAlgSuccess_Expect_Success) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    bool testIfAiv = false;
    std::string testAlgName = "test_mesh_alg";
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock HcomGetCommByGroup 成功返回 comm
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), any())
        .will(invoke([this](const char* groupName, std::shared_ptr<hccl::hcclComm>& comm) {
            comm = this->testComm;
            return HCCL_SUCCESS;
        }));
    
    // Mock hcclComm::HcclSelectAlg 成功
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(testAlgNameBuf, testAlgName.c_str());
}

// 测试 default comm 场景 - HcomGetCommByGroup 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_DefaultComm_GetCommByGroupFail_Expect_NotFound) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    bool testIfAiv = false;
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock HcomGetCommByGroup 失败
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_NOT_FOUND));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
}

// 测试 default comm 场景 - HcclSelectAlg 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_DefaultComm_HcclSelectAlgFail_Expect_Internal) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    bool testIfAiv = false;
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock HcomGetCommByGroup 成功
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), any())
        .will(invoke([this](const char* groupName, std::shared_ptr<hccl::hcclComm>& comm) {
            comm = this->testComm;
            return HCCL_SUCCESS;
        }));
    
    // Mock hcclComm::HcclSelectAlg 失败
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_E_INTERNAL));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_E_INTERNAL);
}

// 测试 default comm 场景 - group 为 nullptr，使用默认 group
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_DefaultComm_NullGroup_UseWorldGroup_Expect_Success) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    bool testIfAiv = false;
    std::string testAlgName = "test_alg_default_group";
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock HcomGetCommByGroup - 验证传入的 group 是 HCCL_WORLD_GROUP
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), any())
        .will(invoke([this](const char* groupName, std::shared_ptr<hccl::hcclComm>& comm) {
            // 验证 group 被设置为 HCCL_WORLD_GROUP
            EXPECT_STREQ(groupName, HCCL_WORLD_GROUP);
            comm = this->testComm;
            return HCCL_SUCCESS;
        }));
    
    // Mock hcclComm::HcclSelectAlg 成功
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, nullptr, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(testAlgNameBuf, testAlgName.c_str());
}

// ==================== memcpy_s 失败场景测试 ====================

// 测试 memcpy_s 失败场景 - 算法名过长
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_MemcpyFail_TooLongAlgName_Expect_ParaError) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    // 创建一个超长的算法名（超过 ALG_NAME_MAX_LEN）
    std::string tooLongAlgName(ALG_NAME_MAX_LEN + 100, 'x');
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 返回超长算法名
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&tooLongAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = tooLongAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_E_PARA);
}

// 测试 memcpy_s 失败场景 - 算法名刚好等于缓冲区大小（需要包括终止符）
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_MemcpyFail_AlgNameExactlyMaxSize_Expect_ParaError) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    // 创建长度等于 ALG_NAME_MAX_LEN 的算法名（没有空间放终止符）
    std::string exactAlgName(ALG_NAME_MAX_LEN, 'x');
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 返回刚好满的算法名
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&exactAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = exactAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_E_PARA);
}

// ==================== WorkflowMode 恢复验证测试 ====================

// 测试 WorkflowMode 被正确保存和恢复 - 非 default comm 成功路径
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_VerifyWorkflowModeRestored_NonDefaultComm) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    std::string testAlgName = "test_workflow_alg";
    
    // 设置一个不同的 workflow mode
    HcclWorkflowMode testMode = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB;
    SetWorkflowMode(testMode);
    HcclWorkflowMode beforeMode = GetWorkflowMode();
    EXPECT_EQ(beforeMode, testMode);
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 成功
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    
    // 验证 workflow mode 被恢复
    HcclWorkflowMode afterMode = GetWorkflowMode();
    EXPECT_EQ(afterMode, testMode);
}

// 测试 WorkflowMode 被正确保存和恢复 - default comm 成功路径
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_VerifyWorkflowModeRestored_DefaultComm) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    bool testIfAiv = false;
    std::string testAlgName = "test_workflow_default_alg";
    
    // 设置一个不同的 workflow mode
    HcclWorkflowMode testMode = HcclWorkflowMode::HCCL_WORKFLOW_MODE_DEFAULT;
    SetWorkflowMode(testMode);
    HcclWorkflowMode beforeMode = GetWorkflowMode();
    EXPECT_EQ(beforeMode, testMode);
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock HcomGetCommByGroup 成功
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), any())
        .will(invoke([this](const char* groupName, std::shared_ptr<hccl::hcclComm>& comm) {
            comm = this->testComm;
            return HCCL_SUCCESS;
        }));
    
    // Mock hcclComm::HcclSelectAlg 成功
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    
    // 验证 workflow mode 被恢复
    HcclWorkflowMode afterMode = GetWorkflowMode();
    EXPECT_EQ(afterMode, testMode);
}

// 测试 WorkflowMode 在错误路径下也被正确恢复
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_VerifyWorkflowModeRestored_OnErrorPath) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    
    // 设置一个不同的 workflow mode
    HcclWorkflowMode testMode = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB;
    SetWorkflowMode(testMode);
    HcclWorkflowMode beforeMode = GetWorkflowMode();
    EXPECT_EQ(beforeMode, testMode);
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 失败
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_E_NOT_SUPPORT));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_E_NOT_SUPPORT);
    
    // 验证 workflow mode 被恢复（即使函数失败）
    HcclWorkflowMode afterMode = GetWorkflowMode();
    EXPECT_EQ(afterMode, testMode);
}

// ==================== 边界条件测试 ====================

// 测试 counts 参数为 nullptr 的场景
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_CountsIsNull_Expect_Success) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    std::string testAlgName = "test_alg_null_counts";
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 成功
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
    HcclResult result = HcomSelectAlg(commValue, group, count, nullptr, dataType, op, 
        opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(testAlgNameBuf, testAlgName.c_str());
}

// 测试各种数据类型的组合
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_DifferentDataTypes_Expect_Success) {
    s64 commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    bool testIfAiv = false;
    std::string testAlgName = "test_alg_different_dt";
    
    // Mock HcomExecSelectAlg 成功
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock hcclComm::HcclSelectAlg 成功
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), any())
        .will(invoke([&testAlgName](auto, auto, auto, auto, auto, auto, auto, auto, std::string& name) {
            name = testAlgName;
            return HCCL_SUCCESS;
        }));
    
    // 测试不同的数据类型
    HcclDataType testDataTypes[] = {
        HCCL_DATA_TYPE_FP16,
        HCCL_DATA_TYPE_FP32,
        HCCL_DATA_TYPE_FP64,
        HCCL_DATA_TYPE_INT8,
        HCCL_DATA_TYPE_INT32,
        HCCL_DATA_TYPE_BFP16
    };
    
    for (auto dt : testDataTypes) {
        char testAlgNameBuf[ALG_NAME_MAX_LEN] = {0};
        HcclResult result = HcomSelectAlg(commValue, group, count, counts, dt, op, 
            opType, aivCoreLimit, &testIfAiv, testAlgNameBuf);
        
        EXPECT_EQ(result, HCCL_SUCCESS);
        EXPECT_STREQ(testAlgNameBuf, testAlgName.c_str());
    }
}