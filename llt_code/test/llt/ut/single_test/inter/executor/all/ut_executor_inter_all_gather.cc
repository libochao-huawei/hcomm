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
#define private public
#define protected public
#include "alg_template_base_pub.h"
#include "all_gather_ring_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
using namespace std;
using namespace hccl;

class AllGatherInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllGatherInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllGatherInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllGatherInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllGatherInterTest::dispatcher = nullptr;

#if 1
TEST_F(AllGatherInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";
    std::vector<RankInfo> para_vector(1);

    AllGatherRing* tempAlg = new AllGatherRing(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif
#if 1
//用例中comminter类型需要改变为commcombine的对应类型
TEST_F(AllGatherInterTest, run_async_00)
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

    AllGatherRing* tempAlg = new AllGatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(3);

    DeviceMem input =  DeviceMem::alloc(1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
TEST_F(AllGatherInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_01_combine";

    std::vector<RankInfo> para_vector(2);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(2);
    for (int i = 0; i < 2; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AllGatherRing* tempAlg = new AllGatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(6);

    DeviceMem input =  DeviceMem::alloc(3);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(1, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
TEST_F(AllGatherInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_02_combine";

    std::vector<RankInfo> para_vector(3);
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

    AllGatherRing* tempAlg = new AllGatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);

    DeviceMem input =  DeviceMem::alloc(3);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
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
//chunksize更新
TEST_F(AllGatherInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_03_combine";

    std::vector<RankInfo> para_vector(3);
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

    AllGatherRing* tempAlg = new AllGatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);

    DeviceMem input =  DeviceMem::alloc(3);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
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
TEST_F(AllGatherInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_04_combine";

    std::vector<RankInfo> para_vector(3);
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

    AllGatherRing* tempAlg = new AllGatherRing(dispatcher);
    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(100);
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
    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0, slice );
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif


