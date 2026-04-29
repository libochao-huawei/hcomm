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

class TestHcclCommunicator {
public:
    HcclResult AicpuInitOpTilingDataBuf(const OpParam &opParam, const HcclCMDType &opType, 
        const std::string &kernelName, const AicpuOpTiling opTilingInfo, u64 dynamicDataSize) {
        MOCKER_CPP(&HcclCommunicator::InitAndCheckAicpuOrderNotify).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HcclCommunicator::BuildHierarchicalAlgOption).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HcclCommunicator::UpdateOpIndex).stubs().will(returnValue(0));
        MOCKER_CPP(&HcclCommunicator::GetOrderLaunchMode).stubs().will(returnValue(0));
        MOCKER_CPP(&HcclCommunicator::AicpuInitOpTilingDataFromOpParam).stubs().will(returnValue(HCCL_SUCCESS));
        
        return hcclCommunicator->AicpuInitOpTilingDataBuf(opParam, opType, kernelName, opTilingInfo, dynamicDataSize);
    }
    
    HcclCommunicator* hcclCommunicator;
};

TEST_F(HcclCommunicatorHostTest, Ut_SetDynamicTilingData_When_A2GroupSendRecv_Expect_SkipIsDirectRemoteRank) {
    std::unique_ptr<HcclCommunicator> hcclCommunicator(new (std::nothrow) HcclCommunicator());
    hcclCommunicator->rankInfoList_.resize(2);
    hcclCommunicator->realUserRank_ = 0;
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910B;
    hcclCommunicator->isGroupMode_ = true;
    hcclCommunicator->userRankSize_ = 2;
    
    OpParam opParam;
    opParam.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    opParam.BatchSendRecvDataDes.itemNum = 1;
    
    HcclSendRecvItem sendRecvInfo;
    opParam.BatchSendRecvDataDes.sendRecvItemsPtr = &sendRecvInfo;
    
    u8 isDirectRemoteRank[2] = {1, 0};
    opParam.BatchSendRecvDataDes.isDirectRemoteRank = isDirectRemoteRank;
    
    AicpuOpTiling opTilingInfo;
    opTilingInfo.algName = "test_alg";
    opTilingInfo.newTag = "test_new_tag";
    opTilingInfo.floatOverflowMode = 0;
    opTilingInfo.dumpDebug = 0;
    
    std::string kernelName = "test_kernel";
    u64 dynamicDataSize = 0;
    
    TestHcclCommunicator testComm;
    testComm.hcclCommunicator = hcclCommunicator.get();

    HcclResult ret = testComm.AicpuInitOpTilingDataBuf(opParam, HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, kernelName, opTilingInfo, dynamicDataSize);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    EXPECT_EQ(hcclCommunicator->deviceType_, DevType::DEV_TYPE_910B);
    EXPECT_EQ(hcclCommunicator->isGroupMode_, true);
    EXPECT_EQ(hcclCommunicator->userRankSize_, 2);
}