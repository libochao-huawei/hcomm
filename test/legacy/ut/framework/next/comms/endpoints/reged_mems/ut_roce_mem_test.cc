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
#include <mockcpp/mokc.h>

#define private public
#define protected public
#include "roce_mem.h"
#include "endpoint.h"
#undef protected
#undef private

using namespace hcomm;

class RoceRegedMemMgrTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "RoceRegedMemMgrTest SetUp" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "RoceRegedMemMgrTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in RoceRegedMemMgrTest SetUp" << std::endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        std::cout << "A Test case in RoceRegedMemMgrTest TearDown" << std::endl;
    }
};

TEST_F(RoceRegedMemMgrTest, get_params_from_mem_desc_desc_len_too_small)
{
    RoceRegedMemMgr roceRegedMemMgr;
    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    
    char buffer[10];
    uint32_t descLen = 10;
    
    HcclResult ret = roceRegedMemMgr.GetParamsFromMemDesc(buffer, descLen, endpointDesc, dto);
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
}

TEST_F(RoceRegedMemMgrTest, get_params_from_mem_desc_desc_len_equal_size)
{
    RoceRegedMemMgr roceRegedMemMgr;
    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    
    char buffer[sizeof(EndpointDesc)];
    uint32_t descLen = sizeof(EndpointDesc);
    
    HcclResult ret = roceRegedMemMgr.GetParamsFromMemDesc(buffer, descLen, endpointDesc, dto);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}
