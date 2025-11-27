/**
?* Copyright (c) 2025 Huawei Technologies Co., Ltd.
?* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
?* CANN Open Software License Agreement Version 2.0 (the "License").
?* Please refer to the License for details. You may not use this file except in compliance with the License.
?* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
?* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
?* See LICENSE in the root of the software repository for the full text of the License.
?*/

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include "hccl/base.h"
#include <hccl/hccl_types.h>
 
#include "comm_base_pub.h"
#define private public
#define protected public
#include "all_reduce_recursive_hd_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "transport_base_pub.h"
#include "externalinput.h"
#include "alg_template_register.h"

using namespace std;
using namespace hccl;

class AllReduceRecursiveHDInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        SetFftsSwitch(false);
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceRecursiveHDInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceRecursiveHDInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllReduceRecursiveHDInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceRecursiveHDInterTest::dispatcher = nullptr;

TEST_F(AllReduceRecursiveHDInterTest, destructor_D0)
{
    u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_RECURSIVE_HALVING_DOUBLING, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceRecursiveHalvingDoubling *alg = dynamic_cast<AllReduceRecursiveHalvingDoubling *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr, reduceAttrBitMap);
}

#if 1
TEST_F(AllReduceRecursiveHDInterTest, run_async_00)
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

    AllReduceRecursiveHalvingDoubling* tempAlg = new AllReduceRecursiveHalvingDoubling(dispatcher);
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
TEST_F(AllReduceRecursiveHDInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 1025;
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

    AllReduceRecursiveHalvingDoubling* tempAlg = new AllReduceRecursiveHalvingDoubling(dispatcher);
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

#if 1
TEST_F(AllReduceRecursiveHDInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 1025;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x00;

    s32 root = 0;
    s32 rank = 1;
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

    AllReduceRecursiveHalvingDoubling* tempAlg = new AllReduceRecursiveHalvingDoubling(dispatcher);
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

#if 0
TEST_F(AllReduceRecursiveHDInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 1025;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    u64 reduceAttrBitMap = 0x00;

    s32 root = 0;
    s32 rank = 11;
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

    AllReduceRecursiveHalvingDoubling* tempAlg = new AllReduceRecursiveHalvingDoubling(dispatcher);
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

#if 0
TEST_F(AllReduceRecursiveHDInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 1025;
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

    AllReduceRecursiveHalvingDoubling* tempAlg = new AllReduceRecursiveHalvingDoubling(dispatcher);
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


