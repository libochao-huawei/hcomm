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
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#define protected public

#include "hccl_ccu_res.h"
#include "hccl_types.h"
#include "ccu_kernel.h"

#include "ccu_res_pack.h"
#include "ccu_kernel_mgr.h"
#include "ccu_res_repo.h"
#include "ccu_common.h"

#undef private
#undef protected

class CcuMgrTest : public testing::Test {
protected:    
    static void SetUpTestCase()
    {
        std::cout << "CcuMgrTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        GlobalMockObject::verify();
        std::cout << "CcuMgrTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CcuMgrTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CcuMgrTest TearDown" << std::endl;
    }

};

TEST_F(CcuMgrTest, Ut_CcuRegister_When_resourceNotEnough_Expect_Return_false)
{
    MOCKER_CPP(&hcomm::CcuKernelMgr::Init).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    hcomm::CcuResReq resReq{};
    for (uint8_t i = 0; i < CCU_MAX_IODIE_NUM; i++) {
        resReq.msReq[i] = 1;
    }
    MOCKER_CPP(&hcomm::CcuKernel::GetResourceRequest).stubs().will(returnValue(resReq));

    uint32_t devicePhyId = 0;
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devicePhyId);
    hcomm::CcuKernelArg arg;
    std::unique_ptr<hcomm::CcuKernel> kernel = std::make_unique<hcomm::CcuKernel>(arg);
    CcuResPack *resPack = new CcuResPack();
    CcuKernelHandle newHandle{0};
    HcclResult ret = kernelMgr.Register(std::move(kernel), *resPack, newHandle);
    EXPECT_EQ(ret, HcclResult::HCCL_E_UNAVAIL);
}