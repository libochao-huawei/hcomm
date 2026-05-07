/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include "dev_rdma_conn_lite.h"
#include "binary_stream.h"

#define private public

using namespace Hccl;

static std::vector<char> BuildSqUniqueId(const RdmaSqContextLite &sqCtx)
{
    BinaryStream bs;
    bs << sqCtx.qpn;
    bs << sqCtx.sqVa;
    bs << sqCtx.wqeSize;
    bs << sqCtx.depth;
    bs << sqCtx.headAddr;
    bs << sqCtx.tailAddr;
    bs << sqCtx.sl;
    bs << sqCtx.dbVa;
    bs << sqCtx.dbMode;
    std::vector<char> result;
    bs.Dump(result);
    return result;
}

static std::vector<char> BuildCqUniqueId(const RdmaCqContextLite &cqCtx)
{
    BinaryStream bs;
    bs << cqCtx.cqn;
    bs << cqCtx.cqVa;
    bs << cqCtx.cqeSize;
    bs << cqCtx.cqDepth;
    bs << cqCtx.headAddr;
    bs << cqCtx.tailAddr;
    bs << cqCtx.dbVa;
    bs << cqCtx.dbMode;
    std::vector<char> result;
    bs.Dump(result);
    return result;
}

static std::vector<char> BuildDevRdmaConnLiteUniqueId(u32 dmaMode, const RdmaSqContextLite &sqCtx, const RdmaCqContextLite &cqCtx)
{
    BinaryStream binaryStream;
    binaryStream << dmaMode;
    
    std::vector<char> sqUniqueId = BuildSqUniqueId(sqCtx);
    binaryStream << sqUniqueId;
    
    std::vector<char> cqUniqueId = BuildCqUniqueId(cqCtx);
    binaryStream << cqUniqueId;
    
    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

static RdmaSqContextLite MakeDefaultSqContext()
{
    RdmaSqContextLite sq{};
    sq.qpn = 1;
    sq.sqVa = 0x10000;
    sq.wqeSize = 64;
    sq.depth = 128;
    sq.headAddr = 0x20000;
    sq.tailAddr = 0x20008;
    sq.sl = 0;
    sq.dbVa = 0x30000;
    sq.dbMode = 0;
    return sq;
}

static RdmaCqContextLite MakeDefaultCqContext()
{
    RdmaCqContextLite cq{};
    cq.cqn = 2;
    cq.cqVa = 0x40000;
    cq.cqeSize = 32;
    cq.cqDepth = 256;
    cq.headAddr = 0x50000;
    cq.tailAddr = 0x50008;
    cq.dbVa = 0x60000;
    cq.dbMode = 0;
    return cq;
}

class DevRdmaConnLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "DevRdmaConnLiteTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "DevRdmaConnLiteTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in DevRdmaConnLiteTest SetUP" << std::endl;
        sqCtx_ = MakeDefaultSqContext();
        cqCtx_ = MakeDefaultCqContext();
        uniqueId_ = BuildDevRdmaConnLiteUniqueId(0, sqCtx_, cqCtx_);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in DevRdmaConnLiteTest TearDown" << std::endl;
    }

    RdmaSqContextLite sqCtx_{};
    RdmaCqContextLite cqCtx_{};
    std::vector<char> uniqueId_;
};

TEST_F(DevRdmaConnLiteTest, Ut_When_Construct_Expect_Success)
{
    std::cout << "Start Ut_When_Construct_Expect_Success" << std::endl;
    
    DevRdmaConnLite connLite(uniqueId_);
    
    std::cout << "End Ut_When_Construct_Expect_Success" << std::endl;
}

TEST_F(DevRdmaConnLiteTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    DevRdmaConnLite connLite(uniqueId_);
    
    std::string desc = connLite.Describe();
    EXPECT_FALSE(desc.empty());
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}

TEST_F(DevRdmaConnLiteTest, Ut_When_DmaMode_Expect_Correct)
{
    std::cout << "Start Ut_When_DmaMode_Expect_Correct" << std::endl;
    
    DevRdmaConnLite connLite(uniqueId_);
    
    EXPECT_EQ(connLite.dmaMode_, 0u);
    
    std::cout << "End Ut_When_DmaMode_Expect_Correct" << std::endl;
}

TEST_F(DevRdmaConnLiteTest, Ut_When_SqContext_Expect_Valid)
{
    std::cout << "Start Ut_When_SqContext_Expect_Valid" << std::endl;
    
    DevRdmaConnLite connLite(uniqueId_);
    
    EXPECT_EQ(connLite.sqContext.qpn, sqCtx_.qpn);
    EXPECT_EQ(connLite.sqContext.sqVa, sqCtx_.sqVa);
    EXPECT_EQ(connLite.sqContext.wqeSize, sqCtx_.wqeSize);
    EXPECT_EQ(connLite.sqContext.depth, sqCtx_.depth);
    EXPECT_EQ(connLite.sqContext.headAddr, sqCtx_.headAddr);
    EXPECT_EQ(connLite.sqContext.tailAddr, sqCtx_.tailAddr);
    EXPECT_EQ(connLite.sqContext.sl, sqCtx_.sl);
    EXPECT_EQ(connLite.sqContext.dbVa, sqCtx_.dbVa);
    EXPECT_EQ(connLite.sqContext.dbMode, sqCtx_.dbMode);
    
    std::cout << "End Ut_When_SqContext_Expect_Valid" << std::endl;
}

TEST_F(DevRdmaConnLiteTest, Ut_When_CqContext_Expect_Valid)
{
    std::cout << "Start Ut_When_CqContext_Expect_Valid" << std::endl;
    
    DevRdmaConnLite connLite(uniqueId_);
    
    EXPECT_EQ(connLite.cqContext.cqn, cqCtx_.cqn);
    EXPECT_EQ(connLite.cqContext.cqVa, cqCtx_.cqVa);
    EXPECT_EQ(connLite.cqContext.cqeSize, cqCtx_.cqeSize);
    EXPECT_EQ(connLite.cqContext.cqDepth, cqCtx_.cqDepth);
    EXPECT_EQ(connLite.cqContext.headAddr, cqCtx_.headAddr);
    EXPECT_EQ(connLite.cqContext.tailAddr, cqCtx_.tailAddr);
    EXPECT_EQ(connLite.cqContext.dbVa, cqCtx_.dbVa);
    EXPECT_EQ(connLite.cqContext.dbMode, cqCtx_.dbMode);
    
    std::cout << "End Ut_When_CqContext_Expect_Valid" << std::endl;
}

TEST_F(DevRdmaConnLiteTest, Ut_When_DmaModeNonZero_Expect_Correct)
{
    std::cout << "Start Ut_When_DmaModeNonZero_Expect_Correct" << std::endl;
    
    std::vector<char> testId = BuildDevRdmaConnLiteUniqueId(1, sqCtx_, cqCtx_);
    DevRdmaConnLite connLite(testId);
    
    EXPECT_EQ(connLite.dmaMode_, 1u);
    
    std::cout << "End Ut_When_DmaModeNonZero_Expect_Correct" << std::endl;
}

TEST_F(DevRdmaConnLiteTest, Ut_When_LargeValues_Expect_Correct)
{
    std::cout << "Start Ut_When_LargeValues_Expect_Correct" << std::endl;
    
    RdmaSqContextLite sqMax{};
    sqMax.qpn = UINT32_MAX;
    sqMax.sqVa = UINT64_MAX;
    sqMax.wqeSize = UINT32_MAX;
    sqMax.depth = UINT32_MAX;
    sqMax.headAddr = UINT64_MAX;
    sqMax.tailAddr = UINT64_MAX;
    sqMax.sl = UINT8_MAX;
    sqMax.dbVa = UINT64_MAX;
    sqMax.dbMode = INT8_MAX;

    RdmaCqContextLite cqMax{};
    cqMax.cqn = UINT32_MAX;
    cqMax.cqVa = UINT64_MAX;
    cqMax.cqeSize = UINT32_MAX;
    cqMax.cqDepth = UINT32_MAX;
    cqMax.headAddr = UINT64_MAX;
    cqMax.tailAddr = UINT64_MAX;
    cqMax.dbVa = UINT64_MAX;
    cqMax.dbMode = INT8_MAX;

    std::vector<char> testId = BuildDevRdmaConnLiteUniqueId(2, sqMax, cqMax);
    DevRdmaConnLite connLite(testId);
    
    EXPECT_EQ(connLite.dmaMode_, 2u);
    EXPECT_EQ(connLite.sqContext.qpn, UINT32_MAX);
    EXPECT_EQ(connLite.cqContext.cqn, UINT32_MAX);
    
    std::cout << "End Ut_When_LargeValues_Expect_Correct" << std::endl;
}


TEST_F(DevRdmaConnLiteTest, Ut_When_MultipleInstances_Expect_Independent)
{
    std::cout << "Start Ut_When_MultipleInstances_Expect_Independent" << std::endl;
    
    DevRdmaConnLite connLite1(uniqueId_);
    DevRdmaConnLite connLite2(uniqueId_);
    
    EXPECT_EQ(connLite1.dmaMode_, connLite2.dmaMode_);
    EXPECT_EQ(connLite1.sqContext.qpn, connLite2.sqContext.qpn);
    EXPECT_EQ(connLite1.cqContext.cqn, connLite2.cqContext.cqn);
    
    std::cout << "End Ut_When_MultipleInstances_Expect_Independent" << std::endl;
}

TEST_F(DevRdmaConnLiteTest, Ut_When_DbModeSw_Expect_Correct)
{
    std::cout << "Start Ut_When_DbModeSw_Expect_Correct" << std::endl;
    
    RdmaSqContextLite sqSw = MakeDefaultSqContext();
    sqSw.dbMode = 1;
    RdmaCqContextLite cqSw = MakeDefaultCqContext();
    cqSw.dbMode = 1;
    
    std::vector<char> testId = BuildDevRdmaConnLiteUniqueId(0, sqSw, cqSw);
    DevRdmaConnLite connLite(testId);
    
    EXPECT_EQ(connLite.sqContext.dbMode, 1);
    EXPECT_EQ(connLite.cqContext.dbMode, 1);
    
    std::cout << "End Ut_When_DbModeSw_Expect_Correct" << std::endl;
}
