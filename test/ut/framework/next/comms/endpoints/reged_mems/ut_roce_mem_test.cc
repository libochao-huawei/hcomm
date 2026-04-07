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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "roce_mem.h"
#include "endpoint.h"
#include "hcomm_res.h"
#include "hcomm_c_adpt.h"

using namespace hcomm;

class RoceRegedMemMgrTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "RoceRegedMemMgrTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "RoceRegedMemMgrTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in RoceRegedMemMgrTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in RoceRegedMemMgrTest TearDown" << std::endl;
    }
};

TEST_F(RoceRegedMemMgrTest, Ut_GetParamsFromMemDesc_When_DescLenTooSmall_Expect_Return_Error)
{
    std::shared_ptr<RoceRegedMemMgr> roceRegedMemMgrPtr = std::make_shared<RoceRegedMemMgr>();
    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    
    char buffer[10];
    uint32_t descLen = 10;
    
    HcclResult ret = roceRegedMemMgrPtr->GetParamsFromMemDesc(buffer, descLen, endpointDesc, dto);
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
}

TEST_F(RoceRegedMemMgrTest, Ut_GetParamsFromMemDesc_When_DescLenEqualSize_Expect_Return_Success)
{
    std::shared_ptr<RoceRegedMemMgr> roceRegedMemMgrPtr = std::make_shared<RoceRegedMemMgr>();
    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    
    char buffer[sizeof(EndpointDesc)];
    uint32_t descLen = sizeof(EndpointDesc);
    
    MOCKER_CPP_VIRTUAL(dto,&Hccl::ExchangeRdmaBufferDto::Deserialize)
        .stubs();
    
    HcclResult ret = roceRegedMemMgrPtr->GetParamsFromMemDesc(buffer, descLen, endpointDesc, dto);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}
