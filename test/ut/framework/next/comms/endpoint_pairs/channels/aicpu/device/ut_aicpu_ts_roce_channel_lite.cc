/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "aicpu_ts_roce_channel_lite.h"
#include "rma_buffer_lite.h"
#include "rmt_rma_buffer_lite.h"
#include "notify_lite.h"
#include "dev_rdma_conn_lite.h"

#define private public

using namespace Hccl;

class AicpuTsRoceChannelLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AicpuTsRoceChannelLiteTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AicpuTsRoceChannelLiteTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AicpuTsRoceChannelLiteTest SetUP" << std::endl;
        
        uniqueId_.resize(1024);
        size_t offset = 0;
        
        u32 notifyNum = 2;
        u32 bufferNum = 1;
        u32 connNum = 1;
        
        memcpy(uniqueId_.data() + offset, &notifyNum, sizeof(notifyNum));
        offset += sizeof(notifyNum);
        memcpy(uniqueId_.data() + offset, &bufferNum, sizeof(bufferNum));
        offset += sizeof(bufferNum);
        memcpy(uniqueId_.data() + offset, &connNum, sizeof(connNum));
        offset += sizeof(connNum);
        
        for (u32 i = 0; i < notifyNum; i++) {
            u64 addr = 0x1000 + i * 0x100;
            u64 size = 0x100;
            u32 key = 0x100 + i;
            memcpy(uniqueId_.data() + offset, &addr, sizeof(addr));
            offset += sizeof(addr);
            memcpy(uniqueId_.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
            memcpy(uniqueId_.data() + offset, &key, sizeof(key));
            offset += sizeof(key);
        }
        
        for (u32 i = 0; i < notifyNum; i++) {
            u64 addr = 0x2000 + i * 0x100;
            u64 size = 0x100;
            u32 rkey = 0x200 + i;
            memcpy(uniqueId_.data() + offset, &addr, sizeof(addr));
            offset += sizeof(addr);
            memcpy(uniqueId_.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
            memcpy(uniqueId_.data() + offset, &rkey, sizeof(rkey));
            offset += sizeof(rkey);
        }
        
        u64 notifyValueAddr = 0x3000;
        u64 notifyValueSize = 0x200;
        u32 notifyValueKey = 0x300;
        memcpy(uniqueId_.data() + offset, &notifyValueAddr, sizeof(notifyValueAddr));
        offset += sizeof(notifyValueAddr);
        memcpy(uniqueId_.data() + offset, &notifyValueSize, sizeof(notifyValueSize));
        offset += sizeof(notifyValueSize);
        memcpy(uniqueId_.data() + offset, &notifyValueKey, sizeof(notifyValueKey));
        offset += sizeof(notifyValueKey);
        
        for (u32 i = 0; i < bufferNum; i++) {
            u64 addr = 0x4000 + i * 0x200;
            u64 size = 0x200;
            u32 lkey = 0x400 + i;
            u32 rkey = 0x500 + i;
            memcpy(uniqueId_.data() + offset, &addr, sizeof(addr));
            offset += sizeof(addr);
            memcpy(uniqueId_.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
            memcpy(uniqueId_.data() + offset, &lkey, sizeof(lkey));
            offset += sizeof(lkey);
            memcpy(uniqueId_.data() + offset, &rkey, sizeof(rkey));
            offset += sizeof(rkey);
        }
        
        for (u32 i = 0; i < bufferNum; i++) {
            u64 addr = 0x5000 + i * 0x200;
            u64 size = 0x200;
            u32 rkey = 0x600 + i;
            memcpy(uniqueId_.data() + offset, &addr, sizeof(addr));
            offset += sizeof(addr);
            memcpy(uniqueId_.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
            memcpy(uniqueId_.data() + offset, &rkey, sizeof(rkey));
            offset += sizeof(rkey);
        }
        
        for (u32 i = 0; i < connNum; i++) {
            u32 qpn = 1 + i;
            u32 cqn = 2 + i;
            memcpy(uniqueId_.data() + offset, &qpn, sizeof(qpn));
            offset += sizeof(qpn);
            memcpy(uniqueId_.data() + offset, &cqn, sizeof(cqn));
            offset += sizeof(cqn);
        }
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AicpuTsRoceChannelLiteTest TearDown" << std::endl;
    }
    
    std::vector<char> uniqueId_;
};

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_Construct_Expect_Success)
{
    std::cout << "Start Ut_When_Construct_Expect_Success" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    std::cout << "End Ut_When_Construct_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    std::string desc = channelLite.Describe();
    EXPECT_FALSE(desc.empty());
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_NotifyNum_Expect_Correct)
{
    std::cout << "Start Ut_When_NotifyNum_Expect_Correct" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.notifyNum_, 2);
    
    std::cout << "End Ut_When_NotifyNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_BufferNum_Expect_Correct)
{
    std::cout << "Start Ut_When_BufferNum_Expect_Correct" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.bufferNum_, 1);
    
    std::cout << "End Ut_When_BufferNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_ConnNum_Expect_Correct)
{
    std::cout << "Start Ut_When_ConnNum_Expect_Correct" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.connNum_, 1);
    
    std::cout << "End Ut_When_ConnNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_LocalNotifies_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_LocalNotifies_Expect_NotEmpty" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.localNotifies_.size(), 2u);
    
    std::cout << "End Ut_When_LocalNotifies_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_RemoteNotifies_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_RemoteNotifies_Expect_NotEmpty" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.remoteNotifies_.size(), 2u);
    
    std::cout << "End Ut_When_RemoteNotifies_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_LocBufferVec_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_LocBufferVec_Expect_NotEmpty" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.locBufferVec_.size(), 1u);
    
    std::cout << "End Ut_When_LocBufferVec_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_RmtBufferVec_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_RmtBufferVec_Expect_NotEmpty" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.rmtBufferVec_.size(), 1u);
    
    std::cout << "End Ut_When_RmtBufferVec_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_ConnVec_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_ConnVec_Expect_NotEmpty" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.connVec_.size(), 1u);
    
    std::cout << "End Ut_When_ConnVec_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_ConnUniqueIdVec_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_ConnUniqueIdVec_Expect_NotEmpty" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_EQ(channelLite.connUniqueIdVec_.size(), 1u);
    
    std::cout << "End Ut_When_ConnUniqueIdVec_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_NotifyValueBuffer_Expect_NotNull)
{
    std::cout << "Start Ut_When_NotifyValueBuffer_Expect_NotNull" << std::endl;
    
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    EXPECT_NE(channelLite.notifyValueBuffer_, nullptr);
    
    std::cout << "End Ut_When_NotifyValueBuffer_Expect_NotNull" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_EmptyUniqueId_Expect_EmptyContainers)
{
    std::cout << "Start Ut_When_EmptyUniqueId_Expect_EmptyContainers" << std::endl;
    
    std::vector<char> emptyId;
    AicpuTsRoceChannelLite channelLite(emptyId);
    
    EXPECT_EQ(channelLite.notifyNum_, 0u);
    EXPECT_EQ(channelLite.bufferNum_, 0u);
    EXPECT_EQ(channelLite.connNum_, 0u);
    EXPECT_EQ(channelLite.localNotifies_.size(), 0u);
    EXPECT_EQ(channelLite.remoteNotifies_.size(), 0u);
    EXPECT_EQ(channelLite.locBufferVec_.size(), 0u);
    EXPECT_EQ(channelLite.rmtBufferVec_.size(), 0u);
    EXPECT_EQ(channelLite.connVec_.size(), 0u);
    
    std::cout << "End Ut_When_EmptyUniqueId_Expect_EmptyContainers" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_MultipleInstances_Expect_Independent)
{
    std::cout << "Start Ut_When_MultipleInstances_Expect_Independent" << std::endl;
    
    AicpuTsRoceChannelLite channelLite1(uniqueId_);
    AicpuTsRoceChannelLite channelLite2(uniqueId_);
    
    EXPECT_EQ(channelLite1.notifyNum_, channelLite2.notifyNum_);
    EXPECT_EQ(channelLite1.bufferNum_, channelLite2.bufferNum_);
    EXPECT_EQ(channelLite1.connNum_, channelLite2.connNum_);
    
    std::cout << "End Ut_When_MultipleInstances_Expect_Independent" << std::endl;
}
