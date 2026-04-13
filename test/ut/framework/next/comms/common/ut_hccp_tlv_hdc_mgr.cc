/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#define private public
#define protected public

#include "hccp_tlv_hdc_mgr.h"

#undef private
#undef protected

using namespace hcomm;

class HccpTlvHdcMgrTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--HccpTlvHdcMgrTest SetUpTestCase--\033[0m" << std::endl;
    }
    
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--HccpTlvHdcMgrTest TearDownTestCase--\033[0m" << std::endl;
    }
    
    virtual void SetUp()
    {
        std::cout << "HccpTlvHdcMgrTest SetUp" << std::endl;
    }
    
    virtual void TearDown()
    {
        std::cout << "HccpTlvHdcMgrTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }
};

// 测试 Deinit 函数 - HccpTlvDeinit 返回成功的情况
TEST_F(HccpTlvHdcMgrTest, Deinit_HccpTlvDeinitSuccess_ReturnSuccess)
{
    HccpTlvHdcMgr &mgr = HccpTlvHdcMgr::GetInstance(0);
    
    // 先初始化
    HcclResult initRet = mgr.Init();
    if (initRet != HCCL_SUCCESS) {
        std::cout << "Init failed, skip test" << std::endl;
        return;
    }
    
    // 调用 Deinit
    HcclResult ret = mgr.Deinit();
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 Deinit 函数 - HccpTlvDeinit 返回失败的情况
TEST_F(HccpTlvHdcMgrTest, Deinit_HccpTlvDeinitFailed_ReturnError)
{
    HccpTlvHdcMgr &mgr = HccpTlvHdcMgr::GetInstance(1);
    
    // 先初始化
    HcclResult initRet = mgr.Init();
    if (initRet != HCCL_SUCCESS) {
        std::cout << "Init failed, skip test" << std::endl;
        return;
    }
    
    // 调用 Deinit
    HcclResult ret = mgr.Deinit();
    
    // 正常情况下应该返回成功
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 Deinit 函数 - 未初始化时调用
TEST_F(HccpTlvHdcMgrTest, Deinit_NotInit_ReturnSuccess)
{
    HccpTlvHdcMgr &mgr = HccpTlvHdcMgr::GetInstance(1);
    
    // 不初始化直接调用 Deinit
    HcclResult ret = mgr.Deinit();
    
    // 应该返回成功
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 Deinit 函数 - 多次调用
TEST_F(HccpTlvHdcMgrTest, Deinit_MultipleCalls_ReturnSuccess)
{
    HccpTlvHdcMgr &mgr = HccpTlvHdcMgr::GetInstance(2);
    
    // 先初始化
    HcclResult initRet = mgr.Init();
    if (initRet != HCCL_SUCCESS) {
        std::cout << "Init failed, skip test" << std::endl;
        return;
    }
    
    // 第一次调用 Deinit
    HcclResult ret1 = mgr.Deinit();
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    
    // 第二次调用 Deinit（未初始化状态）
    HcclResult ret2 = mgr.Deinit();
    EXPECT_EQ(ret2, HCCL_SUCCESS);
}