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

#ifndef private
#define private public
#define protected public
#endif
#include "hccl_communicator.h"
#undef private
#undef protected
#include "llt_hccl_stub_pub.h"

using namespace std;
using namespace hccl;

class HcclCommunicatorHostTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcclCommunicatorHostTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HcclCommunicatorHostTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "HcclCommunicatorHostTest Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "HcclCommunicatorHostTest Test TearDown" << std::endl;
    }
};

TEST_F(HcclCommunicatorHostTest, Ut_InitSymmetricMemory_When_Normal_Expect_ReturnIsHCCL_SUCCESS) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910_93;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_NE(hcclCommunicator->symmetricMemory_, nullptr);
}

TEST_F(HcclCommunicatorHostTest, Ut_InitSymmetricMemory_When_StrideIsValid_Expect_ReturnsIsHCCL_SUCCESS) {
    HcclCommConfig config;
    HcclCommConfigInit(&config);
    config.hcclSymWinMaxMemSizePerRank = 8;
    CommConfig commConfig;
    commConfig.Load(&config);
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator(commConfig));
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910_93;
    auto ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_NE(hcclCommunicator->symmetricMemory_, nullptr);
}

TEST_F(HcclCommunicatorHostTest, Ut_InitSymmetricMemory_When_SuperPodNumGreaterThanOne_Expect_ReturnIsHCCL_SUCCESS) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->superPodNum_ = 2;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // assert not created
    EXPECT_EQ(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_EQ(hcclCommunicator->symmetricMemory_, nullptr);
}

TEST_F(HcclCommunicatorHostTest, Ut_IsSupportSymmetricMemory_When_Normal_Expect_ReturnIsTrue) {
    MOCKER_CPP(&SymmetricMemory::FindSymmetricWindow).stubs().will(returnValue(HCCL_SUCCESS));
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910_93;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_NE(hcclCommunicator->symmetricMemory_, nullptr);
    // 配置满足前置条件
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    hcclCommunicator->deviceNumPerAggregation_ = 2;
    hcclCommunicator->multiModuleDiffDeviceNumMode_ = false;

    OpParam opParam;
    opParam.inputSymWindow = reinterpret_cast<void*>(0x1000);
    opParam.outputSymWindow = reinterpret_cast<void*>(0x2000);
    opParam.aicpuUnfoldMode = true;

    bool retBool = hcclCommunicator->IsSupportSymmetricMemory(HcclCMDType::HCCL_CMD_ALLGATHER, opParam);
    EXPECT_EQ(retBool, true);
    GlobalMockObject::verify();
}

TEST_F(HcclCommunicatorHostTest, Ut_IsSupportSymmetricMemory_When_AicpuUnfoldIsFalse_Expect_ReturnIsFalse) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910_93;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_NE(hcclCommunicator->symmetricMemory_, nullptr);
    OpParam opParam;
    opParam.aicpuUnfoldMode = false;
    bool retBool = hcclCommunicator->IsSupportSymmetricMemory(HcclCMDType::HCCL_CMD_ALLGATHER, opParam);
    EXPECT_EQ(retBool, false);
}

TEST_F(HcclCommunicatorHostTest, Ut_IsSupportSymmetricMemory_When_WorkFlowModeIsNotOpBase_Expect_ReturnIsFalse) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910_93;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_NE(hcclCommunicator->symmetricMemory_, nullptr);
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB);
    bool retBool = hcclCommunicator->IsSupportSymmetricMemory(HcclCMDType::HCCL_CMD_ALLGATHER, opParam);
    EXPECT_EQ(retBool, false);
}

TEST_F(HcclCommunicatorHostTest, Ut_IsSupportSymmetricMemory_When_deviceTypeIsNot910_93_Expect_ReturnIsFalse) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910B;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);

    bool retBool = hcclCommunicator->IsSupportSymmetricMemory(HcclCMDType::HCCL_CMD_ALLGATHER, opParam);
    EXPECT_EQ(retBool, false);
}

TEST_F(HcclCommunicatorHostTest, Ut_IsSupportSymmetricMemory_When_multiModuleDiffDeviceNumModeIsTrue_Expect_ReturnIsFalse) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910_93;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_NE(hcclCommunicator->symmetricMemory_, nullptr);
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    hcclCommunicator->multiModuleDiffDeviceNumMode_ = true;
    bool retBool = hcclCommunicator->IsSupportSymmetricMemory(HcclCMDType::HCCL_CMD_ALLGATHER, opParam);
    EXPECT_EQ(retBool, false);
}

TEST_F(HcclCommunicatorHostTest, Ut_IsSupportSymmetricMemory_When_FindSymmetricWindowReturnIsHCCL_E_NOT_FOUND_Expect_ReturnIsFalse) {
    MOCKER_CPP(&SymmetricMemory::FindSymmetricWindow).stubs().will(returnValue(HCCL_E_NOT_FOUND));
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910_93;
    HcclResult ret = hcclCommunicator->InitSymmetricMemory();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcclCommunicator->symmetricMemoryAgent_, nullptr);
    EXPECT_NE(hcclCommunicator->symmetricMemory_, nullptr);
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    hcclCommunicator->deviceNumPerAggregation_ = 2;
    hcclCommunicator->multiModuleDiffDeviceNumMode_ = false;
    bool retBool = hcclCommunicator->IsSupportSymmetricMemory(HcclCMDType::HCCL_CMD_ALLGATHER, opParam);
    EXPECT_EQ(retBool, false);
    GlobalMockObject::verify();
}