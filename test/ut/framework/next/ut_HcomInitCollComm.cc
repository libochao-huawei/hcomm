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

#include "hcom_common.h"
#include "op_base_v2.h"

class HcomInitCollCommTest : public testing::Test
{
protected:
    virtual void SetUp() override {
        const char *fakeA5SocName = "Ascend950PR_958b";
        MOCKER(aclrtGetSocName).stubs().will(returnValue(fakeA5SocName));
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HcomInitCollCommTest, ut_HcomInitCollComm_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    HcomInfo &hcomInfo = HcomGetCtxHomInfo();
    void *commV2 = nullptr;
    MOCKER(&HcclGetCommNameV2).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclGetCclBuffer).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclGetRankGraphV2).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&hccl::hcclComm::InitCollComm).stubs().will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcomInitCollComm(0, &commV2, hcomInfo.pComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(hcomInfo.pComm, nullptr);
}

TEST_F(HcomInitCollCommTest, ut_HcomInitCollComm_When_commV2IsNullptr_Expect_ReturnIsHCCL_E_PARA)
{
    HcomInfo &hcomInfo = HcomGetCtxHomInfo();
    HcclResult ret = HcomInitCollComm(0, nullptr, hcomInfo.pComm);
    EXPECT_EQ(ret, HCCL_E_PTR);
}