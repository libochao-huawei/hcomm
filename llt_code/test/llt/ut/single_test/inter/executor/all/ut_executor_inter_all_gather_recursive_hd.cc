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

#include "transport_base_pub.h"
#include "comm_base_pub.h"
#include "all_gather_recursive_hd_pub.h"
#include "sal.h"
#include "externalinput.h"

using namespace std;
using namespace hccl;

class AllGatherRecursiveHDInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        SetFftsSwitch(false);
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllGatherRecursiveHDInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllGatherRecursiveHDInterTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
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
HcclDispatcher AllGatherRecursiveHDInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllGatherRecursiveHDInterTest::dispatcher = nullptr;

#if 1
TEST_F(AllGatherRecursiveHDInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    AllGatherRecursiveHalvingDoubling* tempAlg = new AllGatherRecursiveHalvingDoubling(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif

#if 1
TEST_F(AllGatherRecursiveHDInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    u64 count = 9;
    u32 bytesPerData = 1;
    u64 dataBytes = count*bytesPerData;

    u32 root = INVALID_VALUE_RANKID;
    u32 rank = 0;
    u32 rankSize = 1;

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

    AllGatherRecursiveHalvingDoubling* tempAlg = new AllGatherRecursiveHalvingDoubling(dispatcher);

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

#if 1
TEST_F(AllGatherRecursiveHDInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    u64 count = 1025;
    u32 bytesPerData = 1;
    u64 dataBytes = count*bytesPerData;

    u32 root = INVALID_VALUE_RANKID;
    u32 rank = 0;
    u32 rankSize = 13;

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

    AllGatherRecursiveHalvingDoubling* tempAlg = new AllGatherRecursiveHalvingDoubling(dispatcher);

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
TEST_F(AllGatherRecursiveHDInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;

    u64 count = 1025;
    u32 bytesPerData = 1;
    u64 dataBytes = count*bytesPerData;

    u32 root = INVALID_VALUE_RANKID;
    u32 rank = 1;
    u32 rankSize = 13;

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

    AllGatherRecursiveHalvingDoubling* tempAlg = new AllGatherRecursiveHalvingDoubling(dispatcher);

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
TEST_F(AllGatherRecursiveHDInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    u64 count = 1025;
    u32 bytesPerData = 1;
    u64 dataBytes = count*bytesPerData;

    u32 root = INVALID_VALUE_RANKID;
    u32 rank = 11;
    u32 rankSize = 13;

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

    AllGatherRecursiveHalvingDoubling* tempAlg = new AllGatherRecursiveHalvingDoubling(dispatcher);

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
TEST_F(AllGatherRecursiveHDInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;

    u64 count = 1025;
    u32 bytesPerData = 1;
    u64 dataBytes = count*bytesPerData;

    u32 root = INVALID_VALUE_RANKID;
    u32 rank = 0;
    u32 rankSize = 13;

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

    AllGatherRecursiveHalvingDoubling* tempAlg = new AllGatherRecursiveHalvingDoubling(dispatcher);

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


