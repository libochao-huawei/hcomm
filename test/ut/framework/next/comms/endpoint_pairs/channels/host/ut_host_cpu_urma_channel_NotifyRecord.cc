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

class UtHostCpuUrmaChannelNotifyRecord : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        channel = std::make_unique<HostCpuUrmaChannel>();
        channel->jfs_ = reinterpret_cast<urma_jfs_t*>(0x12345678);
        channel->tjetty_ = reinterpret_cast<urma_target_jetty_t*>(0x87654321);
        channel->fenceFlag_ = false;
        remoteNotifyIdx = 1;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<HostCpuUrmaChannel> channel;
    uint32_t remote remoteNotifyIdx;
};

TEST_F(UtHostCpuUrmaChannelNotifyRecord, 
       UT_NotifyRecord_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->NotifyRecord(remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->wqeNum_, 1);
}

TEST_F(UtHostCpuUrmaChannelNotifyRecord, 
       UT_NotifyRecord_When_FenceFlagTrue_Expect_ReturnHCCL_SUCCESS)
{
    channel->fenceFlag_ = true;
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->NotifyRecord(remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->fenceFlag_, false);
}

TEST_F(UtHostCpuUrmaChannelNotifyRecord, 
       UT_NotifyRecord_When_UrmaPostJfr_Fail_Expect_ReturnHCCL_E_INTERNAL)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(-1));
    
    HcclResult ret = channel->NotifyRecord(remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(UtHostCpuUrmaChannelNotifyRecord, 
       UT_NotifyRecord_When_UrmaPostJfsWr_Fail_Expect_ReturnHCCL_E_INTERNAL)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(-1));
    
    HcclResult ret = channel->NotifyRecord(remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(UtHostCpuUrmaChannelNotifyRecord, 
       UT_NotifyRecord_When_UrmaPostJfsWr_FailWithBadWr_Expect_ReturnHCCL_SUCCESS)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(1));
    
    HcclResult ret = channel->NotifyRecord(remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
};
