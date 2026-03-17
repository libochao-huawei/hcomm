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

class UtHostCpuUrmaChannelWriteWithNotify : public testing::Test
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
        remoteNotifyIdx = 2;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<HostCpuUrmaChannel> channel;
    void *dstAddr;
    void *srcAddr;
    uint64_t len;
    uint32_t remoteNotifyIdx;
};

TEST_F(UtHostCpuUrmaChannelWriteWithNotify, 
       UT_WriteWithNotify_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->WriteWithNotify(dstAddr, srcAddr, len, remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->wqeNum_, 1);
}

TEST_F(UtHostCpuUrmaChannelWriteWithNotify, 
       UT_WriteWithNotify_When_FenceFlagTrue_Expect_ReturnHCCL_SUCCESS)
{
    channel->fenceFlag_ = true;
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->WriteWithNotify(dstAddr, srcAddr, len, remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(channel->fenceFlag_, false);
}

TEST_F(UtHostCpuUrmaChannelWriteWithNotify, 
       UT_WriteWithNotify_When_DstIsNull_Expect_ReturnHCCL_E_PTR)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->WriteWithNotify(nullptr, srcAddr, len, remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(UtHostCpuUrmaChannelWriteWithNotify, 
       UT_WriteWithNotify_When_SrcIsNull_Expect_ReturnHCCL_E_PTR)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    
    HcclResult ret = channel->WriteWithNotify(dstAddr, nullptr, len, remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(UtHostCpuUrmaChannelWriteWithNotify, 
       UT_WriteWithNotify_When_UrmaPostJfr_Fail_Expect_ReturnHCCL_E_INTERNAL)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(-1));
    
    HcclResult ret = channel->WriteWithNotify(dstAddr, srcAddr, len, remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(UtHostCpuUrmaChannelWriteWithNotify, 
       UT_WriteWithNotify_When_UrmaPostJfsWr_Fail_Expect_ReturnHCCL_E_INTERNAL)
{
    MOCKER_CPP(&urma_post_jfr_wr).stubs().will(returnValue(0));
    MOCKER_CPP(&urma_post_jfs_wr).stubs().will(returnValue(-1));
    
    HcclResult ret = channel->WriteWithNotify(dstAddr, srcAddr, len, remoteNotifyIdx);
    
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
