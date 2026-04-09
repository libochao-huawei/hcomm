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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hccp_tlv_hdc_mgr.h"
#include "hccl_common.h"
#include "hccp_tlv.h"

using namespace hcomm;

class HccpTlvHdcMgrTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HccpTlvHdcMgrTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HccpTlvHdcMgrTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HccpTlvHdcMgrTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HccpTlvHdcMgrTest TearDown" << std::endl;
    }
};

// 测试Deinit在未初始化时返回SUCCESS
TEST_F(HccpTlvHdcMgrTest, Ut_Deinit_When_NotInit_Expect_HCCL_SUCCESS)
{
    HccpTlvHdcMgr& mgr = HccpTlvHdcMgr::GetInstance(0);
    HcclResult ret = mgr.Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试Deinit在HccpTlvDeinit成功时返回SUCCESS
TEST_F(HccpTlvHdcMgrTest, Ut_Deinit_When_HccpTlvDeinitSuccess_Expect_HCCL_SUCCESS)
{
    HccpTlvHdcMgr& mgr = HccpTlvHdcMgr::GetInstance(0);
    
    // Mock RaTlvInit返回成功
    TlvHandle mockHandle = reinterpret_cast<TlvHandle>(0x12345);
    MOCKER(RaTlvInit).stubs().will(returnValue(0));
    MOCKER(RaTlvInit).stubs().with(any(), any(), outBound(mockHandle)).will(returnValue(0));
    
    // Mock RaTlvDeinit返回成功
    MOCKER(RaTlvDeinit).stubs().will(returnValue(0));
    
    // 先初始化
    HcclResult initRet = mgr.Init();
    EXPECT_EQ(initRet, HCCL_SUCCESS);
    
    // 再Deinit
    HcclResult ret = mgr.Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试Deinit在HccpTlvDeinit失败时返回错误码
TEST_F(HccpTlvHdcMgrTest, Ut_Deinit_When_HccpTlvDeinitFailed_Expect_HCCL_E_NETWORK)
{
    HccpTlvHdcMgr& mgr = HccpTlvHdcMgr::GetInstance(1);
    
    // Mock RaTlvInit返回成功
    TlvHandle mockHandle = reinterpret_cast<TlvHandle>(0x12346);
    MOCKER(RaTlvInit).stubs().will(returnValue(0));
    MOCKER(RaTlvInit).stubs().with(any(), any(), outBound(mockHandle)).will(returnValue(0));
    
    // Mock RaTlvDeinit返回失败（-1）
    MOCKER(RaTlvDeinit).stubs().will(returnValue(-1));
    
    // 先初始化
    HcclResult initRet = mgr.Init();
    EXPECT_EQ(initRet, HCCL_SUCCESS);
    
    // 再Deinit，应该返回错误码
    HcclResult ret = mgr.Deinit();
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

// 测试Init重复调用返回SUCCESS
TEST_F(HccpTlvHdcMgrTest, Ut_Init_When_AlreadyInit_Expect_HCCL_SUCCESS)
{
    HccpTlvHdcMgr& mgr = HccpTlvHdcMgr::GetInstance(2);
    
    // Mock RaTlvInit返回成功
    TlvHandle mockHandle = reinterpret_cast<TlvHandle>(0x12347);
    MOCKER(RaTlvInit).stubs().will(returnValue(0));
    MOCKER(RaTlvInit).stubs().with(any(), any(), outBound(mockHandle)).will(returnValue(0));
    
    // 第一次初始化
    HcclResult initRet1 = mgr.Init();
    EXPECT_EQ(initRet1, HCCL_SUCCESS);
    
    // 第二次初始化，应该返回SUCCESS（幂等性）
    HcclResult initRet2 = mgr.Init();
    EXPECT_EQ(initRet2, HCCL_SUCCESS);
}

// 测试Init在RaTlvInit失败时返回错误
TEST_F(HccpTlvHdcMgrTest, Ut_Init_When_RaTlvInitFailed_Expect_HCCL_E_NETWORK)
{
    HccpTlvHdcMgr& mgr = HccpTlvHdcMgr::GetInstance(3);
    
    // Mock RaTlvInit返回失败
    MOCKER(RaTlvInit).stubs().will(returnValue(-1));
    
    // 初始化应该失败
    HcclResult ret = mgr.Init();
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

// 测试GetHandle返回正确的句柄
TEST_F(HccpTlvHdcMgrTest, Ut_GetHandle_Expect_ValidHandle)
{
    HccpTlvHdcMgr& mgr = HccpTlvHdcMgr::GetInstance(4);
    
    // Mock RaTlvInit返回成功
    TlvHandle mockHandle = reinterpret_cast<TlvHandle>(0x12348);
    MOCKER(RaTlvInit).stubs().will(returnValue(0));
    MOCKER(RaTlvInit).stubs().with(any(), any(), outBound(mockHandle)).will(returnValue(0));
    
    // 初始化
    HcclResult initRet = mgr.Init();
    EXPECT_EQ(initRet, HCCL_SUCCESS);
    
    // 获取句柄
    TlvHandle handle = mgr.GetHandle();
    EXPECT_EQ(handle, mockHandle);
}

// 测试多设备实例的独立性
TEST_F(HccpTlvHdcMgrTest, Ut_MultiDeviceInstances_Expect_Independent)
{
    HccpTlvHdcMgr& mgr0 = HccpTlvHdcMgr::GetInstance(0);
    HccpTlvHdcMgr& mgr1 = HccpTlvHdcMgr::GetInstance(1);
    
    // Mock RaTlvInit返回成功
    TlvHandle mockHandle0 = reinterpret_cast<TlvHandle>(0x12349);
    TlvHandle mockHandle1 = reinterpret_cast<TlvHandle>(0x12350);
    
    MOCKER(RaTlvInit).stubs().will(returnValue(0));
    MOCKER(RaTlvInit).stubs().with(any(), any(), outBound(mockHandle0)).will(returnValue(0));
    MOCKER(RaTlvInit).stubs().with(any(), any(), outBound(mockHandle1)).will(returnValue(0));
    
    // 分别初始化
    HcclResult initRet0 = mgr0.Init();
    EXPECT_EQ(initRet0, HCCL_SUCCESS);
    
    HcclResult initRet1 = mgr1.Init();
    EXPECT_EQ(initRet1, HCCL_SUCCESS);
    
    // 验证句柄不同
    EXPECT_NE(mgr0.GetHandle(), mgr1.GetHandle());
    
    // 清理
    mgr0.Deinit();
    mgr1.Deinit();
}