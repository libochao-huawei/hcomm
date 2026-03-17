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

class UtHostCpuUrmaChannelNotifyWait : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        channel = std::make_unique<HostCpuUrmaChannel>();
        channel->jfc_ = reinterpret_cast<urma_jfc_t*>(0x12345678);
        channel->fenceFlag_ = false;
        localNotifyIdx = 1;
        timeout = 1000;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<HostCpuUrmaChannel> channel;
    uint32_t localNotifyIdx;
    uint32_t timeout;
};

TEST_F(UtHostCpuUrmaChannelNotifyWait, 
       UT_NotifyWait_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    urma_cr_cr wc{};
    wc.status = URMA_CR_SUCCESS;
    wc.imm_data = localNotifyIdx;
    MOCKER_CPP(&urma_poll_jfc).stubs().will(returnValue(1));
    
    HcclResult ret = channel->NotifyWait(localNotifyIdx, timeout);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(UtHostCpuUrmaChannelNotifyWait, 
       UT_NotifyWait_When_PollReturnsSuccess_Expect_ReturnHCCL_SUCCESS)
{
    urma_cr_cr wc{};
    wc.status = URMA_CR_SUCCESS;
    wc.imm_data = localNotifyIdx;
    MOCKER_CPP(&urma_poll_jfc).stubs().will(returnValue(1));
    
    HcclResult ret = channel->NotifyWait(localNotifyIdx, timeout);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(UtHostCpuUrmaChannelNotifyWait, 
       UT_NotifyWait_When_WcStatusNotSuccess_Expect_ReturnHCCL_E_NETWORK)
{
    urma_cr_cr wc{};
    wc.status = URMA_CR_ERROR;
    wc.imm_data = localNotifyIdx;
    MOCKER_CPP(&urma_poll_jfc).stubs().will(returnValue(1));
    
    HcclResult ret = channel->NotifyWait(localNotifyIdx, timeout);
    
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

TEST_F(UtHostCpuUrmaChannelNotifyWait, 
       UT_NotifyWait_When_ImmDataNotMatch_Expect_ContinuePolling)
{
    urma_cr_cr wc{};
    wc.status = URMA_CR_SUCCESS;
    wc.imm_data = localNotifyIdx + 1;
    int callCount = 0;
    MOCKER_CPP(&urma_poll_jfc).stubs().willDo([&](urma_jfc_t*, uint32_t, urma_cr_t*) -> int32_t {
        callCount++;
        if (callCount == 1) {
            return 1;
        }
        wc.imm_data = localNotifyIdx;
        return 1;
    });
    
    HcclResult ret = channel->NotifyWait(localNotifyIdx, timeout);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
