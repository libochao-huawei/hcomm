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
#include "framework/next/comms/endpoint_pairs/channels/aicpu/device/aicpu_ts_roce_channel_lite.h" 
#include "binary_stream.h"
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
        
        // 创建测试用的uniqueId
        BinaryStream binaryStream;
        
        // 写入基本参数
        u32 notifyNum = 2;
        u32 bufferNum = 1;
        u32 connNum = 1;
        binaryStream << notifyNum << bufferNum << connNum;
        
        // 写入本地通知数据
        std::vector<char> locNotifyUniqueIds;
        BinaryStream locNotifyStream;
        for (u32 i = 0; i < notifyNum; ++i) {
            u64 addr = 0x1000 + i * 0x100;
            u64 size = 0x100;
            u32 lkey = 0x100 + i;
            locNotifyStream << addr << size << lkey;
        }
        locNotifyStream.Dump(locNotifyUniqueIds);
        binaryStream << locNotifyUniqueIds;
        
        // 写入远程通知数据
        std::vector<char> rmtNotifyUniqueIds;
        BinaryStream rmtNotifyStream;
        for (u32 i = 0; i < notifyNum; ++i) {
            u64 addr = 0x2000 + i * 0x100;
            u64 size = 0x100;
            u32 rkey = 0x200 + i;
            rmtNotifyStream << addr << size << rkey;
        }
        rmtNotifyStream.Dump(rmtNotifyUniqueIds);
        binaryStream << rmtNotifyUniqueIds;
        
        // 写入通知值缓冲区数据
        std::vector<char> notifyValueBufferUniqueIds;
        BinaryStream notifyValueStream;
        u64 notifyValueAddr = 0x3000;
        u64 notifyValueSize = 0x8;
        u32 notifyValueLkey = 0x300;
        notifyValueStream << notifyValueAddr << notifyValueSize << notifyValueLkey;
        notifyValueStream.Dump(notifyValueBufferUniqueIds);
        binaryStream << notifyValueBufferUniqueIds;
        
        // 写入本地缓冲区数据
        std::vector<char> locBufferUniqueIds;
        BinaryStream locBufferStream;
        for (u32 i = 0; i < bufferNum; ++i) {
            u64 addr = 0x4000 + i * 0x1000;
            u64 size = 0x1000;
            u32 lkey = 0x400 + i;
            locBufferStream << addr << size << lkey;
        }
        locBufferStream.Dump(locBufferUniqueIds);
        binaryStream << locBufferUniqueIds;
        
        // 写入远程缓冲区数据
        std::vector<char> rmtBufferUniqueIds;
        BinaryStream rmtBufferStream;
        for (u32 i = 0; i < bufferNum; ++i) {
            u64 addr = 0x5000 + i * 0x1000;
            u64 size = 0x1000;
            u32 rkey = 0x500 + i;
            rmtBufferStream << addr << size << rkey;
        }
        rmtBufferStream.Dump(rmtBufferUniqueIds);
        binaryStream << rmtBufferUniqueIds;
        
        // 写入连接数据
        std::vector<char> connUniqueIds;
        BinaryStream connStream;
        for (u32 i = 0; i < connNum; ++i) {
            u64 qpn = 0x600 + i;
            u64 psn = 0x700 + i;
            connStream << qpn << psn;
        }
        connStream.Dump(connUniqueIds);
        binaryStream << connUniqueIds;
        
        // 生成最终的uniqueId
        binaryStream.Dump(uniqueId_);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AicpuTsRoceChannelLiteTest TearDown" << std::endl;
    }
    
    // 测试数据
    std::vector<char> uniqueId_;
};

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_Normal_Construction_Expect_Success)
{
    std::cout << "Start Ut_When_Normal_Construction_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    // 验证解析结果
    EXPECT_EQ(channelLite.notifyNum_, 2);
    EXPECT_EQ(channelLite.bufferNum_, 1);
    EXPECT_EQ(channelLite.connNum_, 1);
    
    // 验证本地通知
    EXPECT_EQ(channelLite.localNotifies_.size(), 2);
    
    // 验证远程通知
    EXPECT_EQ(channelLite.remoteNotifies_.size(), 2);
    EXPECT_EQ(channelLite.remoteNotifies_[0].GetAddr(), 0x2000);
    EXPECT_EQ(channelLite.remoteNotifies_[0].GetSize(), 0x100);
    EXPECT_EQ(channelLite.remoteNotifies_[0].GetRkey(), 0x200);
    
    // 验证通知值缓冲区
    EXPECT_NE(channelLite.notifyValueBuffer_, nullptr);
    EXPECT_EQ(channelLite.notifyValueBuffer_->GetAddr(), 0x3000);
    EXPECT_EQ(channelLite.notifyValueBuffer_->GetSize(), 0x8);
    EXPECT_EQ(channelLite.notifyValueBuffer_->GetLkey(), 0x300);
    
    // 验证本地缓冲区
    EXPECT_EQ(channelLite.locBufferVec_.size(), 1);
    EXPECT_EQ(channelLite.locBufferVec_[0].GetAddr(), 0x4000);
    EXPECT_EQ(channelLite.locBufferVec_[0].GetSize(), 0x1000);
    EXPECT_EQ(channelLite.locBufferVec_[0].GetLkey(), 0x400);
    
    // 验证远程缓冲区
    EXPECT_EQ(channelLite.rmtBufferVec_.size(), 1);
    EXPECT_EQ(channelLite.rmtBufferVec_[0].GetAddr(), 0x5000);
    EXPECT_EQ(channelLite.rmtBufferVec_[0].GetSize(), 0x1000);
    EXPECT_EQ(channelLite.rmtBufferVec_[0].GetRkey(), 0x500);
    
    // 验证连接
    EXPECT_EQ(channelLite.connVec_.size(), 1);
    EXPECT_EQ(channelLite.connUniqueIdVec_.size(), 1);
    
    std::cout << "End Ut_When_Normal_Construction_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_GetRmaBufSlicelite_Expect_Correct)
{
    std::cout << "Start Ut_When_GetRmaBufSlicelite_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    // 获取本地缓冲区
    const RmaBufferLite &buffer = channelLite.locBufferVec_[0];
    
    // 测试GetRmaBufSlicelite
    RmaBufSliceLite slice = channelLite.GetRmaBufSlicelite(buffer);
    
    // 验证结果
    EXPECT_EQ(slice.addr, buffer.GetAddr());
    EXPECT_EQ(slice.size, buffer.GetSize());
    EXPECT_EQ(slice.lkey, buffer.GetLkey());
    EXPECT_EQ(slice.tokenId, 0); // tokenId 应该被设置为 0
    
    std::cout << "End Ut_When_GetRmaBufSlicelite_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_GetRmtRmaBufSliceLite_Expect_Correct)
{
    std::cout << "Start Ut_When_GetRmtRmaBufSliceLite_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    // 获取远程缓冲区
    const RmtRmaBufferLite &buffer = channelLite.rmtBufferVec_[0];
    
    // 测试GetRmtRmaBufSliceLite
    RmtRmaBufSliceLite slice = channelLite.GetRmtRmaBufSliceLite(buffer);
    
    // 验证结果
    EXPECT_EQ(slice.addr, buffer.GetAddr());
    EXPECT_EQ(slice.size, buffer.GetSize());
    EXPECT_EQ(slice.rkey, buffer.GetRkey());
    EXPECT_EQ(slice.tokenId, 0); // tokenId 应该被设置为 0
    EXPECT_EQ(slice.wrId, 0);     // wrId 应该被设置为 0
    
    std::cout << "End Ut_When_GetRmtRmaBufSliceLite_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_GetRmtNotifySliceLite_Expect_Correct)
{
    std::cout << "Start Ut_When_GetRmtNotifySliceLite_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    // 测试GetRmtNotifySliceLite
    RmtRmaBufSliceLite slice = channelLite.GetRmtNotifySliceLite(0);
    
    // 验证结果
    EXPECT_EQ(slice.addr, 0x2000);
    EXPECT_EQ(slice.size, 0x100);
    EXPECT_EQ(slice.rkey, 0x200);
    EXPECT_EQ(slice.tokenId, 0); // tokenId 应该被设置为 0
    EXPECT_EQ(slice.wrId, 0);     // wrId 应该被设置为 0
    
    std::cout << "End Ut_When_GetRmtNotifySliceLite_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_Zero_NotifyNum_Expect_Success)
{
    std::cout << "Start Ut_When_Zero_NotifyNum_Expect_Success" << std::endl;
    
    // 创建notifyNum为0的uniqueId
    BinaryStream binaryStream;
    u32 notifyNum = 0;
    u32 bufferNum = 1;
    u32 connNum = 1;
    binaryStream << notifyNum << bufferNum << connNum;
    
    // 其他数据保持不变
    std::vector<char> dummyData;
    BinaryStream dummyStream;
    dummyStream << (u64)0 << (u64)0 << (u32)0;
    dummyStream.Dump(dummyData);
    
    // 填充所有必需的字段
    for (int i = 0; i < 5; ++i) {
        binaryStream << dummyData;
    }
    
    std::vector<char> zeroNotifyId;
    binaryStream.Dump(zeroNotifyId);
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(zeroNotifyId);
    
    // 验证解析结果
    EXPECT_EQ(channelLite.notifyNum_, 0);
    EXPECT_EQ(channelLite.localNotifies_.size(), 0);
    EXPECT_EQ(channelLite.remoteNotifies_.size(), 0);
    
    std::cout << "End Ut_When_Zero_NotifyNum_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_Zero_BufferNum_Expect_Success)
{
    std::cout << "Start Ut_When_Zero_BufferNum_Expect_Success" << std::endl;
    
    // 创建bufferNum为0的uniqueId
    BinaryStream binaryStream;
    u32 notifyNum = 1;
    u32 bufferNum = 0;
    u32 connNum = 1;
    binaryStream << notifyNum << bufferNum << connNum;
    
    // 其他数据保持不变
    std::vector<char> dummyData;
    BinaryStream dummyStream;
    dummyStream << (u64)0 << (u64)0 << (u32)0;
    dummyStream.Dump(dummyData);
    
    // 填充所有必需的字段
    for (int i = 0; i < 5; ++i) {
        binaryStream << dummyData;
    }
    
    std::vector<char> zeroBufferId;
    binaryStream.Dump(zeroBufferId);
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(zeroBufferId);
    
    // 验证解析结果
    EXPECT_EQ(channelLite.bufferNum_, 0);
    EXPECT_EQ(channelLite.locBufferVec_.size(), 0);
    EXPECT_EQ(channelLite.rmtBufferVec_.size(), 0);
    
    std::cout << "End Ut_When_Zero_BufferNum_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_Zero_ConnNum_Expect_Success)
{
    std::cout << "Start Ut_When_Zero_ConnNum_Expect_Success" << std::endl;
    
    // 创建connNum为0的uniqueId
    BinaryStream binaryStream;
    u32 notifyNum = 1;
    u32 bufferNum = 1;
    u32 connNum = 0;
    binaryStream << notifyNum << bufferNum << connNum;
    
    // 其他数据保持不变
    std::vector<char> dummyData;
    BinaryStream dummyStream;
    dummyStream << (u64)0 << (u64)0 << (u32)0;
    dummyStream.Dump(dummyData);
    
    // 填充所有必需的字段
    for (int i = 0; i < 5; ++i) {
        binaryStream << dummyData;
    }
    
    std::vector<char> zeroConnId;
    binaryStream.Dump(zeroConnId);
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(zeroConnId);
    
    // 验证解析结果
    EXPECT_EQ(channelLite.connNum_, 0);
    EXPECT_EQ(channelLite.connVec_.size(), 0);
    EXPECT_EQ(channelLite.connUniqueIdVec_.size(), 0);
    
    std::cout << "End Ut_When_Zero_ConnNum_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_CheckConnVec_Expect_Correct)
{
    std::cout << "Start Ut_When_CheckConnVec_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    // 测试正常情况
    EXPECT_NO_THROW(channelLite.CheckConnVec("test"));
    
    // 测试空连接列表
    channelLite.connVec_.clear();
    EXPECT_THROW(channelLite.CheckConnVec("test"), InternalException);
    
    std::cout << "End Ut_When_CheckConnVec_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    // 测试Describe方法
    std::string desc = channelLite.Describe();
    EXPECT_FALSE(desc.empty());
    EXPECT_NE(desc.find("AicpuTsRoceChannelLite"), std::string::npos);
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_MultipleNotifyNum_Expect_Success)
{
    std::cout << "Start Ut_When_MultipleNotifyNum_Expect_Success" << std::endl;
    
    // 创建多个notifyNum的uniqueId
    BinaryStream binaryStream;
    u32 notifyNum = 5;
    u32 bufferNum = 2;
    u32 connNum = 1;
    binaryStream << notifyNum << bufferNum << connNum;
    
    // 写入本地通知数据
    std::vector<char> locNotifyUniqueIds;
    BinaryStream locNotifyStream;
    for (u32 i = 0; i < notifyNum; ++i) {
        u64 addr = 0x1000 + i * 0x100;
        u64 size = 0x100;
        u32 lkey = 0x100 + i;
        locNotifyStream << addr << size << lkey;
    }
    locNotifyStream.Dump(locNotifyUniqueIds);
    binaryStream << locNotifyUniqueIds;
    
    // 写入远程通知数据
    std::vector<char> rmtNotifyUniqueIds;
    BinaryStream rmtNotifyStream;
    for (u32 i = 0; i < notifyNum; ++i) {
        u64 addr = 0x2000 + i * 0x100;
        u64 size = 0x100;
        u32 rkey = 0x200 + i;
        rmtNotifyStream << addr << size << rkey;
    }
    rmtNotifyStream.Dump(rmtNotifyUniqueIds);
    binaryStream << rmtNotifyUniqueIds;
    
    // 写入通知值缓冲区数据
    std::vector<char> notifyValueBufferUniqueIds;
    BinaryStream notifyValueStream;
    u64 notifyValueAddr = 0x3000;
    u64 notifyValueSize = 0x8 * notifyNum;
    u32 notifyValueLkey = 0x300;
    notifyValueStream << notifyValueAddr << notifyValueSize << notifyValueLkey;
    notifyValueStream.Dump(notifyValueBufferUniqueIds);
    binaryStream << notifyValueBufferUniqueIds;
    
    // 写入本地缓冲区数据
    std::vector<char> locBufferUniqueIds;
    BinaryStream locBufferStream;
    for (u32 i = 0; i < bufferNum; ++i) {
        u64 addr = 0x4000 + i * 0x1000;
        u64 size = 0x1000;
        u32 lkey = 0x400 + i;
        locBufferStream << addr << size << lkey;
    }
    locBufferStream.Dump(locBufferUniqueIds);
    binaryStream << locBufferUniqueIds;
    
    // 写入远程缓冲区数据
    std::vector<char> rmtBufferUniqueIds;
    BinaryStream rmtBufferStream;
    for (u32 i = 0; i < bufferNum; ++i) {
        u64 addr = 0x5000 + i * 0x1000;
        u64 size = 0x1000;
        u32 rkey = 0x500 + i;
        rmtBufferStream << addr << size << rkey;
    }
    rmtBufferStream.Dump(rmtBufferUniqueIds);
    binaryStream << rmtBufferUniqueIds;
    
    // 写入连接数据
    std::vector<char> connUniqueIds;
    BinaryStream connStream;
    for (u32 i = 0; i < connNum; ++i) {
        u64 qpn = 0x600 + i;
        u64 psn = 0x700 + i;
        connStream << qpn << psn;
    }
    connStream.Dump(connUniqueIds);
    binaryStream << connUniqueIds;
    
    std::vector<char> multiNotifyId;
    binaryStream.Dump(multiNotifyId);
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(multiNotifyId);
    
    // 验证解析结果
    EXPECT_EQ(channelLite.notifyNum_, 5);
    EXPECT_EQ(channelLite.localNotifies_.size(), 5);
    EXPECT_EQ(channelLite.remoteNotifies_.size(), 5);
    
    std::cout << "End Ut_When_MultipleNotifyNum_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_MultipleBufferNum_Expect_Success)
{
    std::cout << "Start Ut_When_MultipleBufferNum_Expect_Success" << std::endl;
    
    // 创建多个bufferNum的uniqueId
    BinaryStream binaryStream;
    u32 notifyNum = 1;
    u32 bufferNum = 3;
    u32 connNum = 1;
    binaryStream << notifyNum << bufferNum << connNum;
    
    // 写入本地通知数据
    std::vector<char> locNotifyUniqueIds;
    BinaryStream locNotifyStream;
    u64 addr = 0x1000;
    u64 size = 0x100;
    u32 lkey = 0x100;
    locNotifyStream << addr << size << lkey;
    locNotifyStream.Dump(locNotifyUniqueIds);
    binaryStream << locNotifyUniqueIds;
    
    // 写入远程通知数据
    std::vector<char> rmtNotifyUniqueIds;
    BinaryStream rmtNotifyStream;
    addr = 0x2000;
    size = 0x100;
    u32 rkey = 0x200;
    rmtNotifyStream << addr << size << rkey;
    rmtNotifyStream.Dump(rmtNotifyUniqueIds);
    binaryStream << rmtNotifyUniqueIds;
    
    // 写入通知值缓冲区数据
    std::vector<char> notifyValueBufferUniqueIds;
    BinaryStream notifyValueStream;
    u64 notifyValueAddr = 0x3000;
    u64 notifyValueSize = 0x8;
    u32 notifyValueLkey = 0x300;
    notifyValueStream << notifyValueAddr << notifyValueSize << notifyValueLkey;
    notifyValueStream.Dump(notifyValueBufferUniqueIds);
    binaryStream << notifyValueBufferUniqueIds;
    
    // 写入本地缓冲区数据
    std::vector<char> locBufferUniqueIds;
    BinaryStream locBufferStream;
    for (u32 i = 0; i < bufferNum; ++i) {
        u64 addr = 0x4000 + i * 0x1000;
        u64 size = 0x1000;
        u32 lkey = 0x400 + i;
        locBufferStream << addr << size << lkey;
    }
    locBufferStream.Dump(locBufferUniqueIds);
    binaryStream << locBufferUniqueIds;
    
    // 写入远程缓冲区数据
    std::vector<char> rmtBufferUniqueIds;
    BinaryStream rmtBufferStream;
    for (u32 i = 0; i < bufferNum; ++i) {
        u64 addr = 0x5000 + i * 0x1000;
        u64 size = 0x1000;
        u32 rkey = 0x500 + i;
        rmtBufferStream << addr << size << rkey;
    }
    rmtBufferStream.Dump(rmtBufferUniqueIds);
    binaryStream << rmtBufferUniqueIds;
    
    // 写入连接数据
    std::vector<char> connUniqueIds;
    BinaryStream connStream;
    u64 qpn = 0x600;
    u64 psn = 0x700;
    connStream << qpn << psn;
    connStream.Dump(connUniqueIds);
    binaryStream << connUniqueIds;
    
    std::vector<char> multiBufferId;
    binaryStream.Dump(multiBufferId);
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(multiBufferId);
    
    // 验证解析结果
    EXPECT_EQ(channelLite.bufferNum_, 3);
    EXPECT_EQ(channelLite.locBufferVec_.size(), 3);
    EXPECT_EQ(channelLite.rmtBufferVec_.size(), 3);
    
    std::cout << "End Ut_When_MultipleBufferNum_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_MultipleConnNum_Expect_Success)
{
    std::cout << "Start Ut_When_MultipleConnNum_Expect_Success" << std::endl;
    
    // 创建多个connNum的uniqueId
    BinaryStream binaryStream;
    u32 notifyNum = 1;
    u32 bufferNum = 1;
    u32 connNum = 4;
    binaryStream << notifyNum << bufferNum << connNum;
    
    // 写入本地通知数据
    std::vector<char> locNotifyUniqueIds;
    BinaryStream locNotifyStream;
    u64 addr = 0x1000;
    u64 size = 0x100;
    u32 lkey = 0x100;
    locNotifyStream << addr << size << lkey;
    locNotifyStream.Dump(locNotifyUniqueIds);
    binaryStream << locNotifyUniqueIds;
    
    // 写入远程通知数据
    std::vector<char> rmtNotifyUniqueIds;
    BinaryStream rmtNotifyStream;
    addr = 0x2000;
    size = 0x100;
    u32 rkey = 0x200;
    rmtNotifyStream << addr << size << rkey;
    rmtNotifyStream.Dump(rmtNotifyUniqueIds);
    binaryStream << rmtNotifyUniqueIds;
    
    // 写入通知值缓冲区数据
    std::vector<char> notifyValueBufferUniqueIds;
    BinaryStream notifyValueStream;
    u64 notifyValueAddr = 0x3000;
    u64 notifyValueSize = 0x8;
    u32 notifyValueLkey = 0x300;
    notifyValueStream << notifyValueAddr << notifyValueSize << notifyValueLkey;
    notifyValueStream.Dump(notifyValueBufferUniqueIds);
    binaryStream << notifyValueBufferUniqueIds;
    
    // 写入本地缓冲区数据
    std::vector<char> locBufferUniqueIds;
    BinaryStream locBufferStream;
    addr = 0x4000;
    size = 0x1000;
    lkey = 0x400;
    locBufferStream << addr << size << lkey;
    locBufferStream.Dump(locBufferUniqueIds);
    binaryStream << locBufferUniqueIds;
    
    // 写入远程缓冲区数据
    std::vector<char> rmtBufferUniqueIds;
    BinaryStream rmtBufferStream;
    addr = 0x5000;
    size = 0x1000;
    rkey = 0x500;
    rmtBufferStream << addr << size << rkey;
    rmtBufferStream.Dump(rmtBufferUniqueIds);
    binaryStream << rmtBufferUniqueIds;
    
    // 写入连接数据
    std::vector<char> connUniqueIds;
    BinaryStream connStream;
    for (u32 i = 0; i < connNum; ++i) {
        u64 qpn = 0x600 + i;
        u64 psn = 0x700 + i;
        connStream << qpn << psn;
    }
    connStream.Dump(connUniqueIds);
    binaryStream << connUniqueIds;
    
    std::vector<char> multiConnId;
    binaryStream.Dump(multiConnId);
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(multiConnId);
    
    // 验证解析结果
    EXPECT_EQ(channelLite.connNum_, 4);
    EXPECT_EQ(channelLite.connVec_.size(), 4);
    EXPECT_EQ(channelLite.connUniqueIdVec_.size(), 4);
    
    std::cout << "End Ut_When_MultipleConnNum_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelLiteTest, Ut_When_GetRmtNotifySliceLite_OutOfRange_Expect_Exception)
{
    std::cout << "Start Ut_When_GetRmtNotifySliceLite_OutOfRange_Expect_Exception" << std::endl;
    
    // 创建AicpuTsRoceChannelLite实例
    AicpuTsRoceChannelLite channelLite(uniqueId_);
    
    // 测试越界访问
    EXPECT_THROW(channelLite.GetRmtNotifySliceLite(10), std::exception);
    
    std::cout << "End Ut_When_GetRmtNotifySliceLite_OutOfRange_Expect_Exception" << std::endl;
}
