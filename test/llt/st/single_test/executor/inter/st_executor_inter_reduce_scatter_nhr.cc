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
#include "reduce_scatter_nhr_pub.h"
#include "alg_template_register.h"
#include "sal.h"
#include "comm_ring_pub.h"
using namespace std;
using namespace hccl;

class ReduceScatterNHRInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ReduceScatterNHRInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ReduceScatterNHRInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher ReduceScatterNHRInterTest::dispatcherPtr = nullptr;
DispatcherPub *ReduceScatterNHRInterTest::dispatcher = nullptr;

#if 1
TEST_F(ReduceScatterNHRInterTest, destructor_D0)
{
    std::string collect_id = "test-destructor";
    std::vector<RankInfo> para_vector(1);

    u64 reduceAttrBitMap = 0x01;
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_NHR, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare(reduceAttrBitMap, false));
}
#endif

void run_async_test_reducescatter_nhr(DispatcherPub *dispatcher, u32 rank, u32 rankSize, u32 root = -1)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
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

    u64 reduceAttrBitMap = 0x01;
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_NHR, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare(reduceAttrBitMap, false));

    DeviceMem output =  DeviceMem::alloc(128);
    DeviceMem input =  DeviceMem::alloc(128*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*rankSize);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT8, stream,
        HcclReduceOp::HCCL_REDUCE_SUM, root);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, ret);

    ret = tempAlg->RunAsync(rank, rankSize, link);

    ret =rt_stream_synchronize(stream.ptr());
}

#if 1
//对应代码中，ranksize=1
TEST_F(ReduceScatterNHRInterTest, run_async_00)
{
    run_async_test_reducescatter_nhr(dispatcher, /*rank=*/0, /*rankSize=*/1);
}
#endif

#if 1
// rankszie>1, root = rank
TEST_F(ReduceScatterNHRInterTest, run_async_01)
{
    run_async_test_reducescatter_nhr(dispatcher, /*rank=*/0, /*rankSize=*/8);
}
#endif

#if 1
// rankszie>1, root = rank
TEST_F(ReduceScatterNHRInterTest, run_async_02)
{
    run_async_test_reducescatter_nhr(dispatcher, /*rank=*/3, /*rankSize=*/8);
}
#endif

#if 1
// rankszie>1, root != rank
TEST_F(ReduceScatterNHRInterTest, run_async_03)
{
    run_async_test_reducescatter_nhr(dispatcher, /*rank=*/2, /*rankSize=*/7);
}
#endif

#if 1
// rankszie>1, root != rank
TEST_F(ReduceScatterNHRInterTest, run_async_04)
{
    run_async_test_reducescatter_nhr(dispatcher, /*rank=*/5, /*rankSize=*/7);
}
#endif
