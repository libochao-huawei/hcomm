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

class UtHostCpuUrmaChannelPrepareWriteWrResource : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        channel = std::make_unique<HostCpuUrmaChannel>();
        channel->fenceFlag_ = false;
        channel->tjetty_ = reinterpret_cast<urma_target_jetty_t*>(0x12345678);
        dstAddr = reinterpret_cast<void*>(0x87654321);
        srcAddr = reinterpret_cast<void*>(0xABCDEF01);
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
    urma_jfs_wr_t writeWithNotifyWr{};
};

TEST_F(UtHostCpuUrmaChannelPrepareWriteWrResource, 
       UT_PrepareWriteWrResource_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    HcclResult ret = channel->PrepareWriteWrResource(dstAddr, srcAddr, len, remoteNotifyIdx, writeWithNotifyWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(writeWithNotifyWr.opcode, URMA::URMA_OPC_WRITE_IMM);
    EXPECT_EQ(writeWithNotifyWr.flag.bs.place_order, 1);
    EXPECT_EQ(writeWithNotifyWr.flag.bs.comp_order, 1);
    EXPECT_EQ(writeWithNotifyWr.flag.bs.fence, 0);
    EXPECT_EQ(writeWithNotifyWr.user_ctx, 0);
    EXPECT_EQ(writeWithNotifyWr.rw.src.sge->addr, reinterpret_cast<uint64_t>(srcAddr));
    EXPECT_EQ(writeWithNotifyWr.rw.src.sge->len, len);
    EXPECT_EQ(writeWithNotifyWr.rw.dst.sge->addr, reinterpret_cast<uint64_t>(dstAddr));
    EXPECT_EQ(writeWithNotifyWr.rw.dst.sge->len, len);
    EXPECT_EQ(writeWithNotifyWr.rw.notify_data, remoteNotifyIdx);
    EXPECT_EQ(writeWithNotifyWr.send.imm_data, remoteNotifyIdx);
    EXPECT_EQ(writeWithNotifyWr.next, nullptr);
    EXPECT_EQ(channel->fenceFlag_, false);
}

TEST_F(UtHostCpuUrmaChannelPrepareWriteWrResource, 
       UT_PrepareWriteWrResource_When_FenceFlagTrue_Expect_ReturnHCCL_SUCCESS)
{
    channel->fenceFlag_ = true;
    
    HcclResult ret = channel->PrepareWriteWrResource(dstAddr, srcAddr, len, remoteNotifyIdx, writeWithNotifyWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(writeWithNotifyWr.flag.bs.place_order, 2);
    EXPECT_EQ(writeWithNotifyWr.flag.bs.fence, 1);
    EXPECT_EQ(channel->fenceFlag_, false);
}

TEST_F(UtHostCpuUrmaChannelPrepareWriteWrResource, 
       UT_PrepareWriteWrResource_When_LenZero_Expect_ReturnHCCL_SUCCESS)
{
    len = 0;
    
    HcclResult ret = channel->PrepareWriteWrResource(dstAddr, srcAddr, len, remoteNotifyIdx, writeWithNotifyWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(writeWithNotifyWr.rw.src.sge->len, 0);
    EXPECT_EQ(writeWithNotifyWr.rw.dst.sge->len, 0);
}

TEST_F(UtHostCpuUrmaChannelPrepareWriteWrResource, 
       UT_PrepareWriteWrResource_When_LenMaxUint32_Expect_ReturnHCCL_SUCCESS)
{
    len = UINT32_MAX;
    
    HcclResult ret = channel->PrepareWriteWrResource(dstAddr, srcAddr, len, remoteNotifyIdx, writeWithNotifyWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(writeWithNotifyWr.rw.src.sge->len, UINT32_MAX);
    EXPECT_EQ(writeWithNotifyWr.rw.dst.sge->len, UINT32_MAX);
}

TEST_F(UtHostCpuUrmaChannelPreparePrepareWriteWrResource, 
       UT_PrepareWriteWrResource_When_LenExceedsUint32_Expect_ReturnHCCL_E_PARA)
{
    len = static_cast<uint64_t>(UINT32_MAX) + 1;
    
    HcclResult ret = channel->PrepareWriteWrResource(dstAddr, srcAddr, len, remoteNotifyIdx, writeWithNotifyWr);
    
    EXPECT_EQ(ret, HCCL_E_PARA);
}
