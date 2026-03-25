/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "framework/next/coll_comms/communicator/aicpu/hccl_aicpu_hdc_handler.h"
#undef private

using namespace hccl;
class HcclAicpuHdcHandlerTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        std::cout << "HcclAicpuHdcHandlerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HcclAicpuHdcHandlerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HcclAicpuHdcHandlerTest SetUp" << std::endl;
        // 创建模拟的 HDCommunicate 实例
        h2dTransfer_ = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_H2D, sizeof(Hccl::KfcCommand));
        d2hTransfer_ = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_D2H, sizeof(Hccl::KfcExecStatus));
        // 创建 HcclAicpuHdcHandler 实例
        hdcHandler_ = std::make_unique<HcclAicpuHdcHandler>(h2dTransfer_, d2hTransfer_);
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in HcclAicpuHdcHandlerTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    std::shared_ptr<HDCommunicate> h2dTransfer_;
    std::shared_ptr<HDCommunicate> d2hTransfer_;
    std::unique_ptr<HcclAicpuHdcHandler> hdcHandler_;
};

TEST_F(HcclAicpuHdcHandlerTest, test_get_kfc_command_success)
{
    // 模拟 h2dTransfer_->Get 方法返回成功
    Hccl::KfcCommand expectedCmd = Hccl::KfcCommand::NS_STOP_LAUNCH;
    MOCKER_CPP(&HDCommunicate::Get, HcclResult(HDCommunicate:: *)(uint32_t, size_t, uint8_t*))
    .stubs()
    .with(any(), any(), any())
    .will(doAnswer([expectedCmd](uint32_t offset, size_t size, uint8_t* buffer) {
        *reinterpret_cast<Hccl::KfcCommand*>(buffer) = expectedCmd;
        return HCCL_SUCCESS;
    }));
    
    // 测试 GetKfcCommand 方法
    Hccl::KfcCommand cmd;
    auto ret = hdcHandler_->GetKfcCommand(cmd);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(cmd, expectedCmd);
}

TEST_F(HcclAicpuHdcHandlerTest, test_get_kfc_command_fail)
{
    // 模拟 h2dTransfer_->Get 方法返回失败
    MOCKER_CPP(&HDCommunicate::Get, HcclResult(HDCommunicate:: *)(uint32_t, size_t, uint8_t*))
    .stubs()
    .with(any(), any(), any())
    .will(returnValue(HCCL_E_INTERNAL));
    
    // 测试 GetKfcCommand 方法
    Hccl::KfcCommand cmd;
    auto ret = hdcHandler_->GetKfcCommand(cmd);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(HcclAicpuHdcHandlerTest, test_get_kfc_command_same_command)
{
    // 模拟 h2dTransfer_->Get 方法返回相同的命令
    Hccl::KfcCommand expectedCmd = Hccl::KfcCommand::NS_STOP_LAUNCH;
    MOCKER_CPP(&HDCommunicate::Get, HcclResult(HDCommunicate:: *)(uint32_t, size_t, uint8_t*))
    .stubs()
    .with(any(), any(), any())
    .will(doAnswer([expectedCmd](uint32_t offset, size_t size, uint8_t* buffer) {
        *reinterpret_cast<Hccl::KfcCommand*>(buffer) = expectedCmd;
        return HCCL_SUCCESS;
    }));
    
    // 第一次调用
    Hccl::KfcCommand cmd1;
    auto ret1 = hdcHandler_->GetKfcCommand(cmd1);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    EXPECT_EQ(cmd1, expectedCmd);
    
    // 第二次调用，应该返回相同的命令
    Hccl::KfcCommand cmd2;
    auto ret2 = hdcHandler_->GetKfcCommand(cmd2);
    EXPECT_EQ(ret2, HCCL_SUCCESS);
    EXPECT_EQ(cmd2, expectedCmd);
}

TEST_F(HcclAicpuHdcHandlerTest, test_set_kfc_exec_status_success)
{
    // 模拟 d2hTransfer_->Put 方法返回成功
    MOCKER_CPP(&HDCommunicate::Put, HcclResult(HDCommunicate:: *)(uint32_t, size_t, uint8_t*))
    .stubs()
    .with(any(), any(), any())
    .will(returnValue(HCCL_SUCCESS));
    
    // 测试 SetKfcExecStatus 方法
    Hccl::KfcStatus state = Hccl::KfcStatus::STOP_LAUNCH_DONE;
    Hccl::KfcErrType errorCode = Hccl::KfcErrType::SUCCESS;
    hdcHandler_->SetKfcExecStatus(state, errorCode);
    // 由于该方法返回 void，我们只需要确保它能正常执行
}

TEST_F(HcclAicpuHdcHandlerTest, test_set_kfc_exec_status_fail)
{
    // 模拟 d2hTransfer_->Put 方法返回失败
    MOCKER_CPP(&HDCommunicate::Put, HcclResult(HDCommunicate:: *)(uint32_t, size_t, uint8_t*))
    .stubs()
    .with(any(), any(), any())
    .will(returnValue(HCCL_E_INTERNAL));
    
    // 测试 SetKfcExecStatus 方法
    Hccl::KfcStatus state = Hccl::KfcStatus::STOP_LAUNCH_DONE;
    Hccl::KfcErrType errorCode = Hccl::KfcErrType::SUCCESS;
    hdcHandler_->SetKfcExecStatus(state, errorCode);
    // 由于该方法返回 void，我们只需要确保它能正常执行，即使内部调用失败
}
