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
#include "dev_rdma_conn_lite.h"
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
        
        uniqueId_.resize(256);
        size_t offset = 0;
        
        u32 dmaMode = 0;
        memcpy(uniqueId_.data() + offset, &dmaMode, sizeof(dmaMode));
        offset += sizeof(dmaMode);
        
        RdmaSqContextLite sqContext;
        sqContext.qpn = 1;
        sqContext.sqVa = 0x10000;
        sqContext.wqeSize = 64;
        sqContext.depth = 128;
        sqContext.headAddr = 0x20000;
        sqContext.tailAddr = 0x20008;
        sqContext.sl = 0;
        sqContext.dbVa = 0x30000;
        sqContext.dbMode = 0;
        memcpy(uniqueId_.data() + offset, &sqContext, sizeof(sqContext));
        offset += sizeof(sqContext);
        
        RdmaCqContextLite cqContext;
        cqContext.cqn = 2;
        cqContext.cqVa = 0x40000;
        cqContext.cqeSize = 32;
        cqContext.cqDepth = 256;
        cqContext.headAddr = 0x50000;
        cqContext.tailAddr = 0x50008;
        cqContext.dbVa = 0x60000;
        cqContext.dbMode = 0;
        memcpy(uniqueId_.data() + offset, &cqContext, sizeof(cqContext));
        offset += sizeof(cqContext);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in RdmaConnLiteTest TearDown" << std::endl;
    }
    
    std::vector<char> uniqueId_;
};

TEST_F(RdmaConnLiteTest, Ut_When_Construct_Expect_Success)
{
    std::cout << "Start Ut_When_Construct_Expect_Success" << std::endl;
    
    RdmaConnLite connLite(uniqueId_);
    
    std::cout << "End Ut_When_Construct_Expect_Success" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    RdmaConnLite connLite(uniqueId_);
    
    std::string desc = connLite.Describe();
    EXPECT_FALSE(desc.empty());
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_DmaMode_Expect_Correct)
{
    std::cout << "Start Ut_When_DmaMode_Expect_Correct" << std::endl;
    
    RdmaConnLite connLite(uniqueId_);
    
    EXPECT_EQ(connLite.dmaMode_, 0u);
    
    std::cout << "End Ut_When_DmaMode_Expect_Correct" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_SqContext_Expect_Valid)
{
    std::cout << "Start Ut_When_SqContext_Expect_Valid" << std::endl;
    
    RdmaConnLite connLite(uniqueId_);
    
    EXPECT_EQ(connLite.sqContext.qpn, 1u);
    EXPECT_EQ(connLite.sqContext.sqVa, 0x10000ul);
    EXPECT_EQ(connLite.sqContext.wqeSize, 64u);
    EXPECT_EQ(connLite.sqContext.depth, 128u);
    EXPECT_EQ(connLite.sqContext.headAddr, 0x20000ul);
    EXPECT_EQ(connLite.sqContext.tailAddr, 0x20008ul);
    EXPECT_EQ(connLite.sqContext.sl, 0);
    EXPECT_EQ(connLite.sqContext.dbVa, 0x30000ul);
    EXPECT_EQ(connLite.sqContext.dbMode, 0);
    
    std::cout << "End Ut_When_SqContext_Expect_Valid" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_CqContext_Expect_Valid)
{
    std::cout << "Start Ut_When_CqContext_Expect_Valid" << std::endl;
    
    RdmaConnLite connLite(uniqueId_);
    
    EXPECT_EQ(connLite.cqContext.cqn, 2u);
    EXPECT_EQ(connLite.cqContext.cqVa, 0x40000ul);
    EXPECT_EQ(connLite.cqContext.cqeSize, 32u);
    EXPECT_EQ(connLite.cqContext.cqDepth, 256u);
    EXPECT_EQ(connLite.cqContext.headAddr, 0x50000ul);
    EXPECT_EQ(connLite.cqContext.tailAddr, 0x50008ul);
    EXPECT_EQ(connLite.cqContext.dbVa, 0x60000ul);
    EXPECT_EQ(connLite.cqContext.dbMode, 0);
    
    std::cout << "End Ut_When_CqContext_Expect_Valid" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_DmaModeNonZero_Expect_Correct)
{
    std::cout << "Start Ut_When_DmaModeNonZero_Expect_Correct" << std::endl;
    
    std::vector<char> testId = uniqueId_;
    u32 dmaMode = 1;
    memcpy(testId.data(), &dmaMode, sizeof(dmaMode));
    
    RdmaConnLite connLite(testId);
    
    EXPECT_EQ(connLite.dmaMode_, 1u);
    
    std::cout << "End Ut_When_DmaModeNonZero_Expect_Correct" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_LargeValues_Expect_Correct)
{
    std::cout << "Start Ut_When_LargeValues_Expect_Correct" << std::endl;
    
    std::vector<char> testId = uniqueId_;
    size_t offset = 0;
    
    u32 dmaMode = 2;
    memcpy(testId.data() + offset, &dmaMode, sizeof(dmaMode));
    offset += sizeof(dmaMode);
    
    RdmaSqContextLite sqContext;
    sqContext.qpn = UINT32_MAX;
    sqContext.sqVa = UINT64_MAX;
    sqContext.wqeSize = UINT32_MAX;
    sqContext.depth = UINT32_MAX;
    sqContext.headAddr = UINT64_MAX;
    sqContext.tailAddr = UINT64_MAX;
    sqContext.sl = UINT8_MAX;
    sqContext.dbVa = UINT64_MAX;
    sqContext.dbMode = INT8_MAX;
    memcpy(testId.data() + offset, &sqContext, sizeof(sqContext));
    offset += sizeof(sqContext);
    
    RdmaCqContextLite cqContext;
    cqContext.cqn = UINT32_MAX;
    cqContext.cqVa = UINT64_MAX;
    cqContext.cqeSize = UINT32_MAX;
    cqContext.cqDepth = UINT32_MAX;
    cqContext.headAddr = UINT64_MAX;
    cqContext.tailAddr = UINT64_MAX;
    cqContext.dbVa = UINT64_MAX;
    cqContext.dbMode = INT8_MAX;
    memcpy(testId.data() + offset, &cqContext, sizeof(cqContext));
    offset += sizeof(cqContext);
    
    RdmaConnLite connLite(testId);
    
    EXPECT_EQ(connLite.dmaMode_, 2u);
    EXPECT_EQ(connLite.sqContext.qpn, UINT32_MAX);
    EXPECT_EQ(connLite.cqContext.cqn, UINT32_MAX);
    
    std::cout << "End Ut_When_LargeValues_Expect_Correct" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_EmptyUniqueId_Expect_DefaultValues)
{
    std::cout << "Start Ut_When_EmptyUniqueId_Expect_DefaultValues" << std::endl;
    
    std::vector<char> emptyId;
    RdmaConnLite connLite(emptyId);
    
    EXPECT_EQ(connLite.dmaMode_, 0u);
    EXPECT_EQ(connLite.sqContext.qpn, 0u);
    EXPECT_EQ(connLite.cqContext.cqn, 0u);
    
    std::cout << "End Ut_When_EmptyUniqueId_Expect_DefaultValues" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_MultipleInstances_Expect_Independent)
{
    std::cout << "Start Ut_When_MultipleInstances_Expect_Independent" << std::endl;
    
    RdmaConnLite connLite1(uniqueId_);
    RdmaConnLite connLite2(uniqueId_);
    
    EXPECT_EQ(connLite1.dmaMode_, connLite2.dmaMode_);
    EXPECT_EQ(connLite1.sqContext.qpn, connLite2.sqContext.qpn);
    EXPECT_EQ(connLite1.cqContext.cqn, connLite2.cqContext.cqn);
    
    std::cout << "End Ut_When_MultipleInstances_Expect_Independent" << std::endl;
}

TEST_F(RdmaConnLiteTest, Ut_When_DbModeSw_Expect_Correct)
{
    std::cout << "Start Ut_When_DbModeSw_Expect_Correct" << std::endl;
    
    std::vector<char> testId = uniqueId_;
    size_t offset = sizeof(u32);
    
    RdmaSqContextLite sqContext;
    sqContext.qpn = 1;
    sqContext.sqVa = 0x10000;
    sqContext.wqeSize = 64;
    sqContext.depth = 128;
    sqContext.headAddr = 0x20000;
    sqContext.tailAddr = 0x20008;
    sqContext.sl = 0;
    sqContext.dbVa = 0x30000;
    sqContext.dbMode = 1;
    memcpy(testId.data() + offset, &sqContext, sizeof(sqContext));
    offset += sizeof(sqContext);
    
    RdmaCqContextLite cqContext;
    cqContext.cqn = 2;
    cqContext.cqVa = 0x40000;
    cqContext.cqeSize = 32;
    cqContext.cqDepth = 256;
    cqContext.headAddr = 0x50000;
    cqContext.tailAddr = 0x50008;
    cqContext.dbVa = 0x60000;
    cqContext.dbMode = 1;
    memcpy(testId.data() + offset, &cqContext, sizeof(cqContext));
    
    RdmaConnLite connLite(testId);
    
    EXPECT_EQ(connLite.sqContext.dbMode, 1);
    EXPECT_EQ(connLite.cqContext.dbMode, 1);
    
    std::cout << "End Ut_When_DbModeSw_Expect_Correct" << std::endl;
}
