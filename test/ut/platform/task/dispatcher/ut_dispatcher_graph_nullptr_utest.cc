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
#include <mockcpp/mockcpp.hpp>

#include "dispatcher_graph_pub.h"
#include "hccl_common.h"
#include "llt_hccl_stub_mc2.h"

using namespace std;
using namespace hccl;

class DispatcherGraph_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "DispatcherGraph_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "DispatcherGraph_UT TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "DispatcherGraph_UT Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "DispatcherGraph_UT Test TearDown" << std::endl;
    }
};

// InlineReduceAsync 测试用例
TEST_F(DispatcherGraph_UT, ut_InlineReduceAsync_src_nullptr)
{
    void *src = nullptr;
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *dst = reinterpret_cast<void *>(0x3000);

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->InlineReduceAsync(src, count, dataType, redOp, stream, dst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_InlineReduceAsync_dst_nullptr)
{
    void *src = reinterpret_cast<void *>(0x1000);
    u64 count = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *dst = nullptr;

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->InlineReduceAsync(src, count, dataType, redOp, stream, dst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_InlineReduceAsync_count_zero)
{
    void *src = reinterpret_cast<void *>(0x1000);
    u64 count = 0;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *dst = reinterpret_cast<void *>(0x3000);

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->InlineReduceAsync(src, count, dataType, redOp, stream, dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete dispatcher;
}

// SetGraphDescVectorReduce 测试用例
TEST_F(DispatcherGraph_UT, ut_SetGraphDescVectorReduce_src_nullptr)
{
    void *src = nullptr;
    void *dst = reinterpret_cast<void *>(0x3000);
    int count = 1024;
    void *addrListDevMemPtr = reinterpret_cast<void *>(0x4000);
    void *funcAddr = reinterpret_cast<void *>(0x5000);
    uint32_t numBlocks = 4;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->SetGraphDescVectorReduce(src, dst, count, addrListDevMemPtr, funcAddr, numBlocks, dataType, redOp, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_SetGraphDescVectorReduce_dst_nullptr)
{
    void *src = reinterpret_cast<void *>(0x1000);
    void *dst = nullptr;
    int count = 1024;
    void *addrListDevMemPtr = reinterpret_cast<void *>(0x4000);
    void *funcAddr = reinterpret_cast<void *>(0x5000);
    uint32_t numBlocks = 4;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->SetGraphDescVectorReduce(src, dst, count, addrListDevMemPtr, funcAddr, numBlocks, dataType, redOp, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_SetGraphDescVectorReduce_addrListDevMemPtr_nullptr)
{
    void *src = reinterpret_cast<void *>(0x1000);
    void *dst = reinterpret_cast<void *>(0x3000);
    int count = 1024;
    void *addrListDevMemPtr = nullptr;
    void *funcAddr = reinterpret_cast<void *>(0x5000);
    uint32_t numBlocks = 4;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->SetGraphDescVectorReduce(src, dst, count, addrListDevMemPtr, funcAddr, numBlocks, dataType, redOp, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_SetGraphDescVectorReduce_funcAddr_nullptr)
{
    void *src = reinterpret_cast<void *>(0x1000);
    void *dst = reinterpret_cast<void *>(0x3000);
    int count = 1024;
    void *addrListDevMemPtr = reinterpret_cast<void *>(0x4000);
    void *funcAddr = nullptr;
    uint32_t numBlocks = 4;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->SetGraphDescVectorReduce(src, dst, count, addrListDevMemPtr, funcAddr, numBlocks, dataType, redOp, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_SetGraphDescVectorReduce_count_zero)
{
    void *src = reinterpret_cast<void *>(0x1000);
    void *dst = reinterpret_cast<void *>(0x3000);
    int count = 0;
    void *addrListDevMemPtr = reinterpret_cast<void *>(0x4000);
    void *funcAddr = reinterpret_cast<void *>(0x5000);
    uint32_t numBlocks = 4;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->SetGraphDescVectorReduce(src, dst, count, addrListDevMemPtr, funcAddr, numBlocks, dataType, redOp, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete dispatcher;
}

// TailVectorReduce 测试用例
TEST_F(DispatcherGraph_UT, ut_TailVectorReduce_tailSrc1_nullptr)
{
    void *tailSrc1 = nullptr;
    void *tailSrc2 = reinterpret_cast<void *>(0x2000);
    u64 tailCount = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *tailDst = reinterpret_cast<void *>(0x3000);

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->TailVectorReduce(tailSrc1, tailSrc2, tailCount, dataType, redOp, stream, tailDst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_TailVectorReduce_tailSrc2_nullptr)
{
    void *tailSrc1 = reinterpret_cast<void *>(0x1000);
    void *tailSrc2 = nullptr;
    u64 tailCount = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *tailDst = reinterpret_cast<void *>(0x3000);

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->TailVectorReduce(tailSrc1, tailSrc2, tailCount, dataType, redOp, stream, tailDst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_TailVectorReduce_tailDst_nullptr)
{
    void *tailSrc1 = reinterpret_cast<void *>(0x1000);
    void *tailSrc2 = reinterpret_cast<void *>(0x2000);
    u64 tailCount = 1024;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *tailDst = nullptr;

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->TailVectorReduce(tailSrc1, tailSrc2, tailCount, dataType, redOp, stream, tailDst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherGraph_UT, ut_TailVectorReduce_tailCount_zero)
{
    void *tailSrc1 = reinterpret_cast<void *>(0x1000);
    void *tailSrc2 = reinterpret_cast<void *>(0x2000);
    u64 tailCount = 0;
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *tailDst = reinterpret_cast<void *>(0x3000);

    DispatcherGraph *dispatcher = new (std::nothrow) DispatcherGraph(1);
    HcclResult ret = dispatcher->TailVectorReduce(tailSrc1, tailSrc2, tailCount, dataType, redOp, stream, tailDst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete dispatcher;
}