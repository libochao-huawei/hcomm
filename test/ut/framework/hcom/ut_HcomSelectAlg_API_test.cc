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
        
        group = "hccl_world_group";
        count = 1024;
        counts = nullptr;
        dataType = HCCL_DATA_TYPE_FP32;
        op = HCCL_REDUCE_SUM;
        opType = HCCL_CMD_ALLREDUCE;
        aivCoreLimit = 4;
        ifAiv = false;
        memset(algName, 0, ALG_NAME_MAX_LEN);
        
        // 设置默认 comm 为非 default 值
        commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    }
    
    void TearDown() override {
        GlobalMockObject::verify();
    }
    
    std::shared_ptr<hccl::hcclComm> comm;
    const char *group;
    u64 count;
    void* counts;
    HcclDataType dataType;
    HcclReduceOp op;
    HcclCMDType opType;
    int32_t aivCoreLimit;
    bool ifAiv;
    char algName[ALG_NAME_MAX_LEN];
    s64 commValue;
};

// 测试 ifAiv 指针为空场景 - 覆盖 CHK_PTR_NULL(ifAiv)
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_IfAivIsNull_Expect_ParaError) {
    bool *nullIfAiv = nullptr;
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, nullIfAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_PARA);
}

// 测试 HcomExecSelectAlg 调用失败场景 - 覆盖 HCCLV2_FUNC_RUN(HcomExecSelectAlg)
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_HcomExecSelectAlgFail_Expect_Internal) {
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), 
              outBound(ifAiv), any())
        .will(returnValue(HCCL_E_INTERNAL));
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_INTERNAL);
}

// 测试非 default comm 场景 - HcclSelectAlg 成功 - 覆盖 CHK_RET(hcclHcomComm->HcclSelectAlg)
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_CommNotDefault_Success_Expect_Success) {
    commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), 
              outBound(ifAiv), any())
        .will(returnValue(HCCL_SUCCESS));
    
    std::string testAlgName = "test_ring_alg";
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), outBound(any()))
        .will(assignValue(testAlgName), returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
}

// 测试非 default comm 场景 - HcclSelectAlg 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_CommNotDefault_HcclSelectAlgFail_Expect_NotSupport) {
    commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT) + 1;
    
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), 
              outBound(ifAiv), any())
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), outBound(any()))
        .will(returnValue(HCCL_E_NOT_SUPPORT));
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_NOT_SUPPORT);
}

// 测试 default comm 场景 - HcclSelectAlg 成功 - 覆盖 CHK_RET(hcclComm->HcclSelectAlg)
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_CommDefault_Success_Expect_Success) {
    commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), 
              outBound(ifAiv), any())
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), outBound(comm))
        .will(returnValue(HCCL_SUCCESS));
    
    std::string testAlgName = "test_mesh_alg";
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), outBound(any()))
        .will(assignValue(testAlgName), returnValue(HCCL_SUCCESS));
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
}

// 测试 default comm 场景 - HcomGetCommByGroup 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_CommDefault_GetCommByGroupFail_Expect_NotFound) {
    commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), 
              outBound(ifAiv), any())
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), outBound(comm))
        .will(returnValue(HCCL_E_NOT_FOUND));
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
}

// 测试 default comm 场景 - HcclSelectAlg 失败
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_CommDefault_HcclSelectAlgFail_Expect_Internal) {
    commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), 
              outBound(ifAiv), any())
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), outBound(comm))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), outBound(any()))
        .will(returnValue(HCCL_E_INTERNAL));
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &ifAiv, algName);
    
    EXPECT_EQ(result, HCCL_E_INTERNAL);
}

// 测试正常路径完整测试 - 验证 WorkflowMode 恢复
TEST_F(HcomSelectAlgTest, Ut_HcomSelectAlg_When_NormalPath_Expect_WorkflowModeRestored) {
    commValue = static_cast<s64>(CommNumHcom::COMM_VALUE_DEFAULT);
    
    MOCKER(HcomExecSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), 
              outBound(ifAiv), any())
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(HcomGetCommByGroup)
        .stubs()
        .with(any(), outBound(comm))
        .will(returnValue(HCCL_SUCCESS));
    
    std::string testAlgName = "test_normal_alg";
    MOCKER(&hccl::hcclComm::HcclSelectAlg)
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any(), outBound(any()))
        .will(assignValue(testAlgName), returnValue(HCCL_SUCCESS));
    
    // 保存原始 workflow mode
    HcclWorkflowMode originalMode = GetWorkflowMode();
    
    HcclResult result = HcomSelectAlg(commValue, group, count, counts, dataType, op, 
        opType, aivCoreLimit, &ifAiv, algName);
    
    // 验证 workflow mode 被正确恢复
    HcclWorkflowMode finalMode = GetWorkflowMode();
    
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_STREQ(algName, testAlgName.c_str());
    EXPECT_EQ(originalMode, finalMode);
}