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

class UtHostCpuUrmaChannelRead : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        channel = std::make_unique<HostCpuUrmaChannel>();
        channel->jfs_ = reinterpret_cast<urma_jfs_t*>(0x12345678);
        channel->tjetty_ = reinterpret_cast<urma_target_jetty_t*>(0x87654321);
        channel->fenceFlag_ = false;
        dstAddr = reinterpret_cast<void*>(0xABCDEF01);
        srcAddr = reinterpret_cast<void*>(0x12345678);
        len = 1024;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<HostCpuUrmaChannel> channel;
    void *dstAddr;
    void *srcAddr;
    uint64_t len;
};

TEST_F(UtHostCpuUrmaChannelRead, 
       UT_Read_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    channel->wqeNum_ = 1;
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->Read(dstAddr, srcAddr, len);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->wqeNum_, 2);
}

TEST_F(UtHostCpuUrmaChannelRead, 
       UT_Read_When_FenceFlagTrue_Expect_ReturnHCCL_SUCCESS)
{
    channel->fenceFlag_ = true;
    channel->wqeNum_ = 1;
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->Read(dstAddr, srcAddr, len);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->fenceFlag_, false);
}

TEST_F(UtHostCpuUrmaChannelRead, 
       UT_Read_When_LenZero_Expect_ReturnHCCL_SUCCESS)
{
    len = 0;
    channel->wqeNum_ = 1;
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->Read(dstAddr, srcAddr, len);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(UtHostCpuUrmaChannelRead, 
       UT_Read_When_UrmaPostJfsWr_Fail_Expect_ReturnHCCL_E_NETWORK)
{
    channel->wqeNum_ = 1;
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(-1));
    
    HcclResult ret = channel->Read(dstAddr, srcAddr, len);
    
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}
