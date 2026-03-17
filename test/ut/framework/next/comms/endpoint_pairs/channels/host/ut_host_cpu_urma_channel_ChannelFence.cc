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

class UtHostCpuUrmaChannelChannelFence : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        channel = std::make_unique<HostCpuUrmaChannel>();
        channel->jfc_ = reinterpret_cast<urma_jfc_t*>(0x12345678);
        channel->fenceFlag_ = false;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<HostCpuUrmaChannel> channel;
};

TEST_F(UtHostCpuUrmaChannelChannelFence, 
       UT_ChannelFence_When_WqeNumZero_Expect_ReturnHCCL_SUCCESS)
{
    channel->wqeNum_ = 0;
    
    HcclResult ret = channel->ChannelFence();
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->fenceFlag_, true);
}

TEST_F(UtHostCpuUrmaChannelChannelFence, 
       UT_ChannelFence_When_WqeNumOne_Expect_ReturnHCCL_SUCCESS)
{
    channel->wqeNum_ = 1;
    urma_cr_cr wc{};
    wc.status = URMA_CR_SUCCESS;
    MOCKER_CPP(&urma_poll_jfc).stubs().will(returnValue(1));
    
    HcclResult ret = channel->ChannelFence();
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->wqeNum_, 0);
    EXPECT_EQ(channel(channel->fenceFlag_, true));
}

TEST_F(UtHostCpuUrmaChannelChannelFence, 
       UT_ChannelFence_When_WqeNumMultiple_Expect_ReturnHCCL_SUCCESS)
{
    channel->wqeNum_ = 3;
    urma_cr_cr wc[3];
    for (int i = 0; i < 3; i++) {
        wc[i].status = URMA_CR_SUCCESS;
    }
    int callCount = 0;
    MOCKER_CPP(&urma_poll_jfc).stubs().willDo([&](urma_jfc_t*, uint32_t, urma_cr_t*) -> int32_t {
        callCount++;
        if (callCount == 1) {
            return 1;
        }
        return 3;
    });
    
    HcclResult ret = channel->ChannelFence();
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->wqeNum_, 0);
    EXPECT_EQ(channel->fenceFlag_, true);
}

TEST_F(UtHostCpuUrmaChannelChannelFence, 
       UT_ChannelFence_When_WcStatusNotSuccess_Expect_ReturnHCCL_E_NETWORK)
{
    channel->wqeNum_ = 1;
    urma_cr_cr wc{};
    wc.status = URMA_CR_ERROR;
    MOCKER_CPP(&urma_poll_jfc).stubs().will(returnValue(1));
    
    HcclResult ret = channel->ChannelFence();
    
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

TEST_F(UtHostCpuUrmaChannelChannelFence, 
       UT_ChannelFence_When_PollReturnsMoreThanExpected_Expect_ReturnHCCL_E_INTERNAL)
{
    channel->wqeNum_ = 1;
    urma_cr_cr wc[2];
    for (int i = 0; i < 2; i++) {
        wc[i].status = URMA_CR_SUCCESS;
    }
    MOCKER_CPP(&urma_poll_jfc).stubs().will(returnValue(2));
    
    HcclResult ret = channel->ChannelFence();
    
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
