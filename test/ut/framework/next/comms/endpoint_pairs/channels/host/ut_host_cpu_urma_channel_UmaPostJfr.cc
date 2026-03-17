/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to to License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#include "host_cpu_urma_channel.h"
#undef private

using namespace hcomm;

class UtHostCpuUrmaChannelUrmaPostJfr : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        channel = std::make_unique<HostCpuUrmaChannel>();
        channel->jfr_ = reinterpret_cast<urma_jfr_t*>(0x12345678);
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<HostCpuUrmaChannel> channel;
};

TEST_F(UtHostCpuUrmaChannelUrmaPostJfr, 
       UT_UrmaPostJfr_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->UrmaPostJfr();
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(UtHostCpuUrmaChannelUrmaPostJfr, 
       UT_UrmaPostJfr_When_UrmaPostJfrWr_Fail_Expect_ReturnHCCL_E_NETWORK)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(-1));
    
    HcclResult ret = channel->UrmaPostJfr();
    
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}
