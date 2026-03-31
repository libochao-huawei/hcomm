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

#ifndef private
#define private public
#define protected public
#endif
#include "hccl_comm_pub.h"
#include "hccl_communicator.h"
#include "aicpu_indop_process.h"
#undef private
#undef protected

using namespace std;
using namespace hccl;

class hcclCommTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "hcclCommTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "hcclCommTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

// 测试因子表
// 1. IsCommunicatorV2() 返回值: true, false
// 2. communicator_->Resume() 返回值: HCCL_SUCCESS, HCCL_ERROR
// 3. AicpuIndopProcess::AicpuGetCommAll() 返回值: HCCL_SUCCESS, HCCL_ERROR
// 4. aicpuCommInfo 中是否包含匹配的 identifier_: 是, 否
// 5. deviceComm.second->GetCollCommAicpu() 返回值: 非nullptr, nullptr

// pair-wise 组合测试用例
// 1. IsCommunicatorV2()=false, communicator_->Resume()=HCCL_SUCCESS
// 2. IsCommunicatorV2()=false, communicator_->Resume()=HCCL_ERROR
// 3. IsCommunicatorV2()=true, AicpuGetCommAll()=HCCL_SUCCESS, 包含匹配identifier, GetCollCommAicpu()=非nullptr
// 4. IsCommunicatorV2()=true, AicpuGetCommAll()=HCCL_SUCCESS, 包含匹配identifier, GetCollCommAicpu()=nullptr
// 5. IsCommunicatorV2()=true, AicpuGetCommAll()=HCCL_SUCCESS, 不包含匹配identifier
// 6. IsCommunicatorV2()=true, AicpuGetCommAll()=HCCL_ERROR

TEST_F(hcclCommTest, Ut_Resume_When_IsCommunicatorV2False_Expect_CallCommunicatorResume) {
    // 准备
    hcclComm comm;
    comm.communicator_ = new HcclCommunicator();
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(false));
    
    MOCKER(&HcclCommunicator::Resume)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    // 执行
    HcclResult result = comm.Resume();
    
    // 验证
    ASSERT_EQ(result, HCCL_SUCCESS);
    
    // 清理
    delete comm.communicator_;
    comm.communicator_ = nullptr;
}

TEST_F(hcclCommTest, Ut_Resume_When_IsCommunicatorV2False_CommunicatorResumeError_Expect_ReturnError) {
    // 准备
    hcclComm comm;
    comm.communicator_ = new HcclCommunicator();
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(false));
    
    MOCKER(&HcclCommunicator::Resume)
        .stubs()
        .will(returnValue(HCCL_ERROR));
    
    // 执行
    HcclResult result = comm.Resume();
    
    // 验证
    ASSERT_EQ(result, HCCL_ERROR);
    
    // 清理
    delete comm.communicator_;
    comm.communicator_ = nullptr;
}

TEST_F(hcclCommTest, Ut_Resume_When_IsCommunicatorV2True_Expect_DoNotCallCommunicatorResume) {
    // 准备
    hcclComm comm;
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(true));
    
    // 执行
    HcclResult result = comm.Resume();
    
    // 验证
    ASSERT_EQ(result, HCCL_SUCCESS);
}

TEST_F(hcclCommTest, Ut_GetCommStatus_When_IsCommunicatorV2False_Expect_ReturnReadyStatus) {
    // 准备
    hcclComm comm;
    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_UNKNOWN;
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(false));
    
    // 执行
    HcclResult result = comm.GetCommStatus(status);
    
    // 验证
    ASSERT_EQ(result, HCCL_SUCCESS);
    ASSERT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_READY);
}

TEST_F(hcclCommTest, Ut_GetCommStatus_When_IsCommunicatorV2True_AicpuGetCommAllSuccess_HasMatchingIdentifier_GetCollCommAicpuNotNull_Expect_ReturnAicpuCommStatus) {
    // 准备
    hcclComm comm;
    comm.identifier_ = "test_identifier";
    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_UNKNOWN;
    
    // 创建模拟对象
    CollCommAicpuMgr *collCommAicpuMgr = new CollCommAicpuMgr();
    CollCommAicpu *collCommAicpu = new CollCommAicpu();
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(true));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommMutex)
        .stubs()
        .will(returnValue(*(new ReadWriteLockBase())));
    
    vector<pair<string, CollCommAicpuMgr *>> aicpuCommInfo;
    aicpuCommInfo.push_back(make_pair("test_identifier", collCommAicpuMgr));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommAll)
        .stubs()
        .with(outBound(aicpuCommInfo))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(&CollCommAicpuMgr::GetCollCommAicpu)
        .stubs()
        .will(returnValue(collCommAicpu));
    
    MOCKER(&CollCommAicpu::GetCommmStatus)
        .stubs()
        .will(returnValue(HcclCommStatus::HCCL_COMM_STATUS_RUNNING));
    
    // 执行
    HcclResult result = comm.GetCommStatus(status);
    
    // 验证
    ASSERT_EQ(result, HCCL_SUCCESS);
    ASSERT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_RUNNING);
    
    // 清理
    delete collCommAicpu;
    delete collCommAicpuMgr;
}

TEST_F(hcclCommTest, Ut_GetCommStatus_When_IsCommunicatorV2True_AicpuGetCommAllSuccess_HasMatchingIdentifier_GetCollCommAicpuNull_Expect_ReturnError) {
    // 准备
    hcclComm comm;
    comm.identifier_ = "test_identifier";
    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_UNKNOWN;
    
    // 创建模拟对象
    CollCommAicpuMgr *collCommAicpuMgr = new CollCommAicpuMgr();
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(true));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommMutex)
        .stubs()
        .will(returnValue(*(new ReadWriteLockBase())));
    
    vector<pair<string, CollCommAicpuMgr *>> aicpuCommInfo;
    aicpuCommInfo.push_back(make_pair("test_identifier", collCommAicpuMgr));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommAll)
        .stubs()
        .with(outBound(aicpuCommInfo))
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(&CollCommAicpuMgr::GetCollCommAicpu)
        .stubs()
        .will(returnValue(nullptr));
    
    // 执行
    HcclResult result = comm.GetCommStatus(status);
    
    // 验证
    ASSERT_EQ(result, HCCL_ERROR);
    
    // 清理
    delete collCommAicpuMgr;
}

TEST_F(hcclCommTest, Ut_GetCommStatus_When_IsCommunicatorV2True_AicpuGetCommAllSuccess_NoMatchingIdentifier_Expect_ReturnReadyStatus) {
    // 准备
    hcclComm comm;
    comm.identifier_ = "test_identifier";
    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_UNKNOWN;
    
    // 创建模拟对象
    CollCommAicpuMgr *collCommAicpuMgr = new CollCommAicpuMgr();
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(true));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommMutex)
        .stubs()
        .will(returnValue(*(new ReadWriteLockBase())));
    
    vector<pair<string, CollCommAicpuMgr *>> aicpuCommInfo;
    aicpuCommInfo.push_back(make_pair("other_identifier", collCommAicpuMgr));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommAll)
        .stubs()
        .with(outBound(aicpuCommInfo))
        .will(returnValue(HCCL_SUCCESS));
    
    // 执行
    HcclResult result = comm.GetCommStatus(status);
    
    // 验证
    ASSERT_EQ(result, HCCL_SUCCESS);
    ASSERT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_READY);
    
    // 清理
    delete collCommAicpuMgr;
}

TEST_F(hcclCommTest, Ut_GetCommStatus_When_IsCommunicatorV2True_AicpuGetCommAllError_Expect_ReturnError) {
    // 准备
    hcclComm comm;
    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_UNKNOWN;
    
    // Mock
    MOCKER(&hcclComm::IsCommunicatorV2)
        .stubs()
        .will(returnValue(true));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommMutex)
        .stubs()
        .will(returnValue(*(new ReadWriteLockBase())));
    
    MOCKER(&AicpuIndopProcess::AicpuGetCommAll)
        .stubs()
        .will(returnValue(HCCL_ERROR));
    
    // 执行
    HcclResult result = comm.GetCommStatus(status);
    
    // 验证
    ASSERT_EQ(result, HCCL_ERROR);
}
