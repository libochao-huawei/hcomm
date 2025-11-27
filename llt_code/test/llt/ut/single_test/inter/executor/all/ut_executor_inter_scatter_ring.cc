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
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "scatter_ring_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "reducer_pub.h"
#include "sender_pub.h"

using namespace std;
using namespace hccl;

class ScatterInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ScatterInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ScatterInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher ScatterInterTest::dispatcherPtr = nullptr;
DispatcherPub *ScatterInterTest::dispatcher = nullptr;


#if 1
TEST_F(ScatterInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    ScatterRing* tempAlg = new ScatterRing(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
TEST_F(ScatterInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_00_combine";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);
    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }


    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(1);

    DeviceMem input =  DeviceMem::alloc(1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output,  output, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// 带入slicevector，并且vector[i] size都不同
TEST_F(ScatterInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_01_combine";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }


    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(100);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 80;
    slice2.offset =90;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 190;
    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    ret = tempAlg->Prepare(input, input, output,100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// 一个ranksize，input == output
TEST_F(ScatterInterTest, run_async_02)
{
     s32 ret = HCCL_SUCCESS;

    std::string collect_id = "run_async_02";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);
    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ScatterRing* tempAlg = new ScatterRing(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->Prepare(input, input, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
//  chunksize不够128字节均分
TEST_F(ScatterInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_03_combine";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 80;
    slice2.offset =90;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 190;
    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// ranksize ==1 分支?
TEST_F(ScatterInterTest, run_async_04)
{
     s32 ret = HCCL_SUCCESS;

    std::string collect_id = "run_async_04";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);
    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    DeviceMem input =  DeviceMem::alloc(9);
    DeviceMem output =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ScatterRing* tempAlg = new ScatterRing(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->Prepare(input, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
// 3个rank，input != ouput ,root 节点
TEST_F(ScatterInterTest, run_async_05)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_05_combine";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 80;
    slice2.offset =90;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 190;
    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    ret = tempAlg->Prepare(input, output, output, 300, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(0, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// 3个rank，input != ouput ,最后一个节点
TEST_F(ScatterInterTest, run_async_06)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_06_combine";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;

    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(2, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif

TEST_F(ScatterInterTest, run_async_linkerr_1)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_linkerr_1";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;

    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(2, 4, link);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

TEST_F(ScatterInterTest, run_async_linkerr_2)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_linkerr_1";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;

    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(0, 4, link);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

void run_async_test_scatter_ring(DispatcherPub *dispatcher, u32 rank, u32 rankSize, u32 root, s32 nLinks = -1,
    std::vector<u32> nicRankList = {0, 1, 2, 3, 4, 5, 6, 7})
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(nLinks);
    for (int i = 0; i < nLinks; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }


    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(128*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 128*rankSize, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, root,
        std::vector<Slice>(0), 0, nicRankList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    if(nLinks == rankSize) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else {
        EXPECT_NE(ret, HCCL_SUCCESS);
    }

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

// ranksize=1
TEST_F(ScatterInterTest, run_async_000)
{
    run_async_test_scatter_ring(dispatcher, /*rank=*/0, /*rankSize=*/1, /*root=*/0);
}

// ranksize>1, links.size() < rankSize
TEST_F(ScatterInterTest, run_async_001)
{
    run_async_test_scatter_ring(dispatcher, /*rank=*/0, /*rankSize=*/4, /*root=*/0, /*nLinks=*/3);
}

// rankszie=8, root=0, rank=0
TEST_F(ScatterInterTest, run_async_002)
{
    run_async_test_scatter_ring(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/0);
}

// rankszie=8, root=0, rank=1
TEST_F(ScatterInterTest, run_async_003)
{
    run_async_test_scatter_ring(dispatcher, /*rank=*/1, /*rankSize=*/8, /*root=*/0);
}

// rankszie=8, root=0, rank=7
TEST_F(ScatterInterTest, run_async_004)
{
    run_async_test_scatter_ring(dispatcher, /*rank=*/7, /*rankSize=*/8, /*root=*/0);
}

// rankszie=8, root=0, rank=0, nicRankList={0}
TEST_F(ScatterInterTest, run_async_006)
{
    std::vector<u32> nicRankList = {0};
    run_async_test_scatter_ring(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/0, /*nLinks=*/-1, nicRankList);
}

// rankszie=8, root=0, rank=0, nicRankList={1}
TEST_F(ScatterInterTest, run_async_007)
{
    std::vector<u32> nicRankList = {1};
    run_async_test_scatter_ring(dispatcher, /*rank=*/0, /*rankSize=*/8, /*root=*/0, /*nLinks=*/-1, nicRankList);
}

// rankszie=8, root=0, rank=1, nicRankList={0}
TEST_F(ScatterInterTest, run_async_008)
{
    std::vector<u32> nicRankList = {0};
    run_async_test_scatter_ring(dispatcher, /*rank=*/1, /*rankSize=*/8, /*root=*/0, /*nLinks=*/-1, nicRankList);
}

// rankszie=8, root=0, rank=1, nicRankList={1}
TEST_F(ScatterInterTest, run_async_009)
{
    std::vector<u32> nicRankList = {1};
    run_async_test_scatter_ring(dispatcher, /*rank=*/1, /*rankSize=*/8, /*root=*/0, /*nLinks=*/-1, nicRankList);
}
