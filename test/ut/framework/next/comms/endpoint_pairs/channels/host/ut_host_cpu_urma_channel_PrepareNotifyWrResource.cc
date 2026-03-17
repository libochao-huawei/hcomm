/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
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

class UtHostCpuUrmaChannelPrepareNotifyWrResource : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        channel = std::make_unique<HostCpuUrmaChannel>();
        channel->fenceFlag_ = false;
        channel->tjetty_ = reinterpret_cast<urma_target_jetty_t*>(0x12345678);
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<HostCpuUrmaChannel> channel;
    uint32_t remoteNotifyIdx = 1;
    urma_jfs_wr_t notifyRecordWr{};
};

TEST_F(UtHostCpuUrmaChannelPrepareNotifyWrResource, 
       UT_PrepareNotifyWrResource_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    HcclResult ret = channel->PrepareNotifyWrResource(remoteNotifyIdx, notifyRecordWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyRecordWr.opcode, URMA_OPC_WRITE_IMM);
    EXPECT_EQ(notifyRecordWr.flag.bs.place_order, 1);
    EXPECT_EQ(notifyRecordWr.flag.bs.comp_order, 1);
    EXPECT_EQ(notifyRecordWr.flag.bs.fence, 0);
    EXPECT_EQ(notifyRecordWr.flag.bs.solicited_enable, 1);
    EXPECT_EQ(notifyRecordWr.user_ctx, 0);
    EXPECT_EQ(notifyRecordWr.rw.notify_data, remoteNotifyIdx);
    EXPECT_EQ(notifyRecordWr.send.imm_data, remoteNotifyIdx);
    EXPECT_EQ(notifyRecordWr.next, nullptr);
    EXPECT_EQ(channel->fenceFlag_, false);
}

TEST_F(UtHostCpuUrmaChannelPrepareNotifyWrResource, 
       UT_PrepareNotifyWrResource_When_FenceFlagTrue_Expect_ReturnHCCL_SUCCESS)
{
    channel->fenceFlag_ = true;
    
    HcclResult ret = channel->PrepareNotifyWrResource(remoteNotifyIdx, notifyRecordWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyRecordWr.flag.bs.place_order, 2);
    EXPECT_EQ(notifyRecordWr.flag.bs.fence, 1);
    EXPECT_EQ(channel->fenceFlag_, false);
}

TEST_F(UtHostCpuUrmaChannelPrepareNotifyWrResource, 
       UT_PrepareNotifyWrResource_When_RemoteNotifyIdxZero_Expect_ReturnHCCL_SUCCESS)
{
    remoteNotifyIdx = 0;
    
    HcclResult ret = channel->PrepareNotifyWrResource(remoteNotifyIdx, notifyRecordWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyRecordWr.rw.notify_data, 0);
    EXPECT_EQ(notifyRecordWr.send.imm_data, 0);
}

TEST_F(UtHostCpuUrmaChannelPrepareNotifyWrResource, 
       UT_PrepareNotifyWrResource_When_RemoteNotifyIdxMax_Expect_ReturnHCCL_SUCCESS)
{
    remoteNotifyIdx = UINT32_MAX;
    
    HcclResult ret = channel->PrepareNotifyWrResource(remoteNotifyIdx, notifyRecordWr);
    
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyRecordWr.rw.notify_data, UINT32_MAX);
    EXPECT_EQ(notifyRecordWr.send.imm_data, UINT32_MAX);
}
