/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#include "comm_base_pub.h"
#include "transport_base_pub.h"
#define private public
#define protected public
#include "reduce_recursive_hd_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "alg_template_register.h"

using namespace std;
using namespace hccl;

class ReduceRecursiveHDInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ReduceRecursiveHDInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ReduceRecursiveHDInterTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher ReduceRecursiveHDInterTest::dispatcherPtr = nullptr;
DispatcherPub *ReduceRecursiveHDInterTest::dispatcher = nullptr;

#if 1

TEST_F(ReduceRecursiveHDInterTest, destructor_D0)
{
    u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCE_RECURSIVE_HALVING_DOUBLING, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    ReduceRecursiveHalvingDoubling *alg = dynamic_cast<ReduceRecursiveHalvingDoubling *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr, reduceAttrBitMap);
}
#endif

#if 1
// input  或者output为空，返回异常
TEST_F(ReduceRecursiveHDInterTest, run_async_input_null)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x01;

    s32 root = 0;
    s32 rank = 0;
    s32 rankSize = 1;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRecursiveHalvingDoubling* tempAlg = new ReduceRecursiveHalvingDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output;
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_E_PTR);

    delete tempAlg;
}

// ranksize == 1
TEST_F(ReduceRecursiveHDInterTest, run_async_ranksize1)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x01;

    s32 root = 0;
    s32 rank = 0;
    s32 rankSize = 1;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRecursiveHalvingDoubling* tempAlg = new ReduceRecursiveHalvingDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif

#if 0
// root为0时，在root节点上的操作
TEST_F(ReduceRecursiveHDInterTest, run_async_root0_rank0)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 5000;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x00;

    s32 root = 0;
    s32 rank = 0;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRecursiveHalvingDoubling* tempAlg = new ReduceRecursiveHalvingDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}

// root 为0时，在rank1上的操作
TEST_F(ReduceRecursiveHDInterTest, run_async_root0_rank1)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 5000;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x00;

    s32 root = 0;
    s32 rank = 0;
    s32 rankSize = 13;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRecursiveHalvingDoubling* tempAlg = new ReduceRecursiveHalvingDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
// root =0, rank=2,与root奇偶性相同，并且处在第一部分
TEST_F(ReduceRecursiveHDInterTest, run_async_root0_rank2)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 5000;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x00;

    s32 root = 0;
    s32 rank = 2;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRecursiveHalvingDoubling* tempAlg = new ReduceRecursiveHalvingDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
// root = 0, 在rank12上的操作
TEST_F(ReduceRecursiveHDInterTest, run_async_root0_rank12)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 5000;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x00;

    s32 root = 0;
    s32 rank = 12;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ReduceRecursiveHalvingDoubling* tempAlg = new ReduceRecursiveHalvingDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif

