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
#include "framework/next/comms/endpoint_pairs/channels/aicpu/device/dev_rdma_conn_lite.h"
#include "binary_stream.h"
#define private public

using namespace Hccl;

class RdmaConnLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "RdmaConnLiteTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "RdmaConnLiteTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in RdmaConnLiteTest SetUP" << std::endl;
        
        // 创建测试用的uniqueId
        BinaryStream binaryStream;
        
        // 写入dmaMode
        u32 dmaMode = 1;
        binaryStream << dmaMode;
        
        // 写入SQ context数据
        u32 qpn = 0x1234;
        u64 sqVa = 0x10000;
        u32 wqeSize = 64;
        u32 depth = 128;
        u64 headAddr = 0x20000;
        u64 tailAddr = 0x20008;
        u8 sl = 3;
        u64 dbVa = 0x30000;
        int8_t dbMode = 0;
        binaryStream << qpn << sqVa << wqeSize << depth << headAddr << tailAddr << sl << dbVa << dbMode;
        
        // 写入CQ context数据
        u32 cqn = 0x5678;
        u64 cqVa = 0x40000;
        u32 cqeSize = 32;
        u32 cqDepth = 256;
        u64 cqHeadAddr = 0x50000;
        u64 cqTailAddr = 0x50008;
        u64 cqDbVa = 0x60000;
        int8_t cqDbMode = 1;
        binaryStream << cqn << cqVa << cqeSize << cqDepth << cqHeadAddr << cqTailAddr << cqDbVa << cqDbMode;
        
        // 生成最终的uniqueId
        binaryStream.Dump(uniqueId_);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in RdmaConnLiteTest TearDown" << std::endl;
    }
    
    // 测试数据
    std::vector<char> uniqueId_;
};

TEST_F(RdmaConnLiteTest, Ut_When_Normal_Construction_Expect_Success)
{
    std::cout << "Start Ut_When_Normal_Construction_Expect_Success" << std::endl;
    
    // 创建RdmaConnLite实例
    RdmaConnLite rdmaConnLite(uniqueId_);
    
    // 验证解析结果
    EXPECT_EQ(rdmaConnLite.dmaMode_, 1);
    
    // 验证SQ context
    EXPECT_EQ(rdmaConnLite.sqContext.qpn, 0x1234);
    EXPECT_EQ(rdmaConnLite.sqContext.sqVa, 0x10000);
    EXPECT_EQ(rdmaConnLite.sqContext.wqeSize, 64);
    EXPECT_EQ(rdmaConnLite.sqContext.depth, 128);
    EXPECT_EQ(rdmaConnLite.sqContext.headAddr, 0x20000);
    EXPECT_EQ(rdmaConnLite.sqContext.tailAddr, 0x20008);
    EXPECT_EQ(rdmaConnLite.sqContext.sl, 3);
    EXPECT_EQ(rdmaConnLite.sqContext.dbVa, 0x30000);
    EXPECT_EQ(rdmaConnLite.sqContext.dbMode, 0);
    
    // 验证CQ context
    EXPECT_EQ(rdmaConnLite.cqContext.cqn, 0x5678);
    EXPECT_EQ(rdmaConnLite.cqContext.cqVa, 0x40000);
    EXPECT_EQ(rdmaConnLite.cqContext.cqeSize, 32);
    EXPECT_EQ(rdmaConnLite.cqContext.cqDepth, 256);
    EXPECT_EQ(rdmaConnLite.cqContext.headAddr, 0x50000);
    EXPECT_EQ(rdmaConnLite.cqContext.tailAddr, 0x50008);
    EXPECT_EQ(rdmaConnLite.cqContext.dbVa, 0x60000);
    EXPECT_EQ(rdmaConnLite.cqContext.dbMode, 1);
    
    std::cout << "End Ut_When_Normal_Construction_Expect_Success" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    // 创建RdmaConnLite实例
    RdmaConnLite rdmaConnLite(uniqueId_);
    
    // 测试Describe方法
    std::string desc = rdmaConnLite.Describe();
    EXPECT_FALSE(desc.empty());
    EXPECT_NE(desc.find("RdmaConnLite"), std::string::npos);
    EXPECT_NE(desc.find("QPN=0x1234"), std::string::npos);
    EXPECT_NE(desc.find("CQN=0x5678"), std::string::npos);
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_DmaMode_Zero_Expect_Success)
{
    std::cout << "Start Ut_When_DmaMode_Zero_Expect_Success" << std::endl;
    
    // 创建dmaMode为0的uniqueId
    BinaryStream binaryStream;
    u32 dmaMode = 0;
    binaryStream << dmaMode;
    
    // 写入SQ context数据
    u32 qpn = 0x1111;
    u64 sqVa = 0x1000;
    u32 wqeSize = 32;
    u32 depth = 64;
    u64 headAddr = 0x2000;
    u64 tailAddr = 0x2008;
    u8 sl = 0;
    u64 dbVa = 0x3000;
    int8_t dbMode = 1;
    binaryStream << qpn << sqVa << wqeSize << depth << headAddr << tailAddr << sl << dbVa << dbMode;
    
    // 写入CQ context数据
    u32 cqn = 0x2222;
    u64 cqVa = 0x4000;
    u32 cqeSize = 16;
    u32 cqDepth = 128;
    u64 cqHeadAddr = 0x5000;
    u64 cqTailAddr = 0x5008;
    u64 cqDbVa = 0x6000;
    int8_t cqDbMode = 0;
    binaryStream << cqn << cqVa << cqeSize << cqDepth << cqHeadAddr << cqTailAddr << cqDbVa << cqDbMode;
    
    std::vector<char> zeroDmaModeId;
    binaryStream.Dump(zeroDmaModeId);
    
    // 创建RdmaConnLite实例
    RdmaConnLite rdmaConnLite(zeroDmaModeId);
    
    // 验证解析结果
    EXPECT_EQ(rdmaConnLite.dmaMode_, 0);
    EXPECT_EQ(rdmaConnLite.sqContext.qpn, 0x1111);
    EXPECT_EQ(rdmaConnLite.cqContext.cqn, 0x2222);
    
    std::cout << "End Ut_When_DmaMode_Zero_Expect_Success" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_Large_Values_Expect_Success)
{
    std::cout << "Start Ut_When_Large_Values_Expect_Success" << std::endl;
    
    // 创建包含大值的uniqueId
    BinaryStream binaryStream;
    u32 dmaMode = 2;
    binaryStream << dmaMode;
    
    // 写入SQ context数据（使用大值）
    u32 qpn = UINT32_MAX;
    u64 sqVa = UINT64_MAX;
    u32 wqeSize = UINT32_MAX;
    u32 depth = UINT32_MAX;
    u64 headAddr = UINT64_MAX;
    u64 tailAddr = UINT64_MAX;
    u8 sl = UINT8_MAX;
    u64 dbVa = UINT64_MAX;
    int8_t dbMode = INT8_MAX;
    binaryStream << qpn << sqVa << wqeSize << depth << headAddr << tailAddr << sl << dbVa << dbMode;
    
    // 写入CQ context数据（使用大值）
    u32 cqn = UINT32_MAX;
    u64 cqVa = UINT64_MAX;
    u32 cqeSize = UINT32_MAX;
    u32 cqDepth = UINT32_MAX;
    u64 cqHeadAddr = UINT64_MAX;
    u64 cqTailAddr = UINT64_MAX;
    u64 cqDbVa = UINT64_MAX;
    int8_t cqDbMode = INT8_MAX;
    binaryStream << cqn << cqVa << cqeSize << cqDepth << cqHeadAddr << cqTailAddr << cqDbVa << cqDbMode;
    
    std::vector<char> largeValuesId;
    binaryStream.Dump(largeValuesId);
    
    // 创建RdmaConnLite实例
    RdmaConnLite rdmaConnLite(largeValuesId);
    
    // 验证解析结果
    EXPECT_EQ(rdmaConnLite.dmaMode_, 2);
    EXPECT_EQ(rdmaConnLite.sqContext.qpn, UINT32_MAX);
    EXPECT_EQ(rdmaConnLite.sqContext.sqVa, UINT64_MAX);
    EXPECT_EQ(rdmaConnLite.cqContext.cqn, UINT32_MAX);
    EXPECT_EQ(rdmaConnLite.cqContext.cqVa, UINT64_MAX);
    
    std::cout << "End Ut_When_Large_Values_Expect_Success" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_Describe_Format_Expect_Correct)
{
    std::cout << "Start Ut_When_Describe_Format_Expect_Correct" << std::endl;
    
    // 创建RdmaConnLite实例
    RdmaConnLite rdmaConnLite(uniqueId_);
    
    // 测试Describe方法的格式
    std::string desc = rdmaConnLite.Describe();
    
    // 验证描述字符串包含所有关键字段
    EXPECT_NE(desc.find("QPN="), std::string::npos);
    EXPECT_NE(desc.find("SQ_VA="), std::string::npos);
    EXPECT_NE(desc.find("WQE_SIZE="), std::string::npos);
    EXPECT_NE(desc.find("SQ_DEPTH="), std::string::npos);
    EXPECT_NE(desc.find("SQ_HEAD_ADDR="), std::string::npos);
    EXPECT_NE(desc.find("SQ_TAIL_ADDR="), std::string::npos);
    EXPECT_NE(desc.find("SL="), std::string::npos);
    EXPECT_NE(desc.find("SQ_DB_VA="), std::string::npos);
    EXPECT_NE(desc.find("SQ_DB_MODE="), std::string::npos);
    EXPECT_NE(desc.find("CQN="), std::string::npos);
    EXPECT_NE(desc.find("CQ_VA="), std::string::npos);
    EXPECT_NE(desc.find("CQE_SIZE="), std::string::npos);
    EXPECT_NE(desc.find("CQ_DEPTH="), std::string::npos);
    EXPECT_NE(desc.find("CQ_HEAD_ADDR="), std::string::npos);
    EXPECT_NE(desc.find("CQ_TAIL_ADDR="), std::string::npos);
    EXPECT_NE(desc.find("CQ_DB_VA="), std::string::npos);
    EXPECT_NE(desc.find("CQ_DB_MODE="), std::string::npos);
    
    std::cout << "End Ut_When_Describe_Format_Expect_Correct" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_Multiple_Instances_Expect_Independent)
{
    std::cout << "Start Ut_When_Multiple_Instances_Expect_Independent" << std::endl;
    
    // 创建第一个实例
    RdmaConnLite rdmaConnLite1(uniqueId_);
    
    // 创建第二个uniqueId
    BinaryStream binaryStream;
    u32 dmaMode = 3;
    binaryStream << dmaMode;
    
    u32 qpn = 0x9999;
    u64 sqVa = 0x70000;
    u32 wqeSize = 128;
    u32 depth = 512;
    u64 headAddr = 0x80000;
    u64 tailAddr = 0x80008;
    u8 sl = 5;
    u64 dbVa = 0x90000;
    int8_t dbMode = 1;
    binaryStream << qpn << sqVa << wqeSize << depth << headAddr << tailAddr << sl << dbVa << dbMode;
    
    u32 cqn = 0xaaaa;
    u64 cqVa = 0xa0000;
    u32 cqeSize = 64;
    u32 cqDepth = 1024;
    u64 cqHeadAddr = 0xb0000;
    u64 cqTailAddr = 0xb0008;
    u64 cqDbVa = 0xc0000;
    int8_t cqDbMode = 0;
    binaryStream << cqn << cqVa << cqeSize << cqDepth << cqHeadAddr << cqTailAddr << cqDbVa << cqDbMode;
    
    std::vector<char> uniqueId2;
    binaryStream.Dump(uniqueId2);
    
    // 创建第二个实例
    RdmaConnLite rdmaConnLite2(uniqueId2);
    
    // 验证两个实例相互独立
    EXPECT_EQ(rdmaConnLite1.dmaMode_, 1);
    EXPECT_EQ(rdmaConnLite1.sqContext.qpn, 0x1234);
    EXPECT_EQ(rdmaConnLite1.cqContext.cqn, 0x5678);
    
    EXPECT_EQ(rdmaConnLite2.dmaMode_, 3);
    EXPECT_EQ(rdmaConnLite2.sqContext.qpn, 0x9999);
    EXPECT_EQ(rdmaConnLite2.cqContext.cqn, 0xaaaa);
    
    std::cout << "End Ut_When_Multiple_Instances_Expect_Independent" << std::endl;
}
