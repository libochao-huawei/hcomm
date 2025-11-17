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
#include <cstdio>

#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"

#define private public
#include "all_gather_nb_pub.h"
#undef private

#include "sal.h"
#include "comm_ring_pub.h"
#include "alg_template_base_pub.h"
using namespace std;
using namespace hccl;

class AllGatherNBInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllGatherNBInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllGatherNBInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllGatherNBInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllGatherNBInterTest::dispatcher = nullptr;
TEST_F(AllGatherNBInterTest, destructor_D0)
{
   s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    AllGatherNB * tempAlg = new AllGatherNB(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}

#if 1
//对应代码中，ranksize ==1
TEST_F(AllGatherNBInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;
    u32 rank = 0;
    u32 rankSize = 1;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AllGatherNB * tempAlg = new AllGatherNB(dispatcher);

    DeviceMem output =  DeviceMem::alloc(128*2*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*2*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);
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
#endif

#if 1
//对应代码中，ranksize ==1
TEST_F(AllGatherNBInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;
    u32 rank = 0;
    u32 rankSize = 1;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AllGatherNB * tempAlg = new AllGatherNB(dispatcher);

    DeviceMem output =  DeviceMem::alloc(128*2*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*2*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);

    std::vector< std::shared_ptr<Transport> > empty_link;
    ret = tempAlg->RunAsync(rank, rankSize, empty_link);
    EXPECT_NE(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif

#if 1
//对应代码中，ranksize ==1
TEST_F(AllGatherNBInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;
    u32 rank = 0;
    u32 rankSize = 1;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AllGatherNB * tempAlg = new AllGatherNB(dispatcher);

    DeviceMem output;

    DeviceMem input =  DeviceMem::alloc(128*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*2*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_NE(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif

#if 1
//对应代码中，ranksize ==1
TEST_F(AllGatherNBInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;
    u32 rank = 0;
    u32 rankSize = 1;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AllGatherNB * tempAlg = new AllGatherNB(dispatcher);

    DeviceMem output =  DeviceMem::alloc(128*2*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*2*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_RESERVED, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);
    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
#endif

