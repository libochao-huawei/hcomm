/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define private public
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <cstdio>
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "all_reduce_nhr_v1_pub.h"

#include "sal.h"
#include "comm_ring_pub.h"
#undef private
using namespace std;
using namespace hccl;

class StubTransportBase : public TransportBase
{
public:
    StubTransportBase(const HcclDispatcher dispatcher,
    MachinePara &machinePara,
    std::chrono::milliseconds timeout, void *remotePtr)
    : TransportBase(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machinePara, timeout), remotePtr_(remotePtr)
    {
    }

    HcclResult GetRemoteMem(UserMemType memType, void **remotePtr) {
        *remotePtr = remotePtr_;
        return HCCL_SUCCESS;
    }
private:
    void *remotePtr_;
};

class AllReduceNHRV1InterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceNHRV1InterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceNHRV1InterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllReduceNHRV1InterTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceNHRV1InterTest::dispatcher = nullptr;

TEST_F(AllReduceNHRV1InterTest, test_CalcVSlicesAndLinks)
{
    s32 ret = HCCL_SUCCESS;
    u32 rank = 1;
    u32 rankSize = 8;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    DeviceMem output =  DeviceMem::alloc(128);
    DeviceMem input =  DeviceMem::alloc(128*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*rankSize);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
        link[i]->pimpl_->transportAttr_.linkType = hccl::LinkType::LINK_HCCS_SW;
    }

    u64 reduceAttrBitMap = 0x01;
    AllReduceNHRV1* tempAlg = new AllReduceNHRV1(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slices;
    for (u32 i = 0; i < rankSize; i++) {
        Slice s;
        s.offset = 1;
        s.size = 1;
        slices.push_back(s);
    }

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        -1, slices);

    RingInfo info = tempAlg->GetRingInfo(rankSize);

    std::vector<Slice> vSlices;
    std::vector<LINK> vLinks;
    ret = tempAlg->CalcVSlicesAndLinks(rank, link, info, vLinks, vSlices);

    ret = rt_stream_synchronize(stream.ptr());
    delete tempAlg;
}

void run_async_test_allreduce_nhr_v1(DispatcherPub *dispatcher, u32 rank, u32 rankSize,bool staged, RunStage stage=RunStage::RUN_PREPARE)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(128*2*rankSize);

    DeviceMem input =  DeviceMem::alloc(128*2*rankSize);
    DeviceMem scratch =  DeviceMem::alloc(128*2*rankSize);

    s32 nLinks = -1;
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);
    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
        link[i]->pimpl_->transportAttr_.linkType = hccl::LinkType::LINK_HCCS_SW;
    }
    u64 reduceAttrBitMap = 0x01;
    AllReduceNHRV1 * tempAlg = new AllReduceNHRV1(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, scratch, 128, HCCL_DATA_TYPE_INT16, stream, HcclReduceOp::HCCL_REDUCE_SUM,
        0);
    ret = tempAlg->RunAsync(rank, rankSize, link);
    if(nLinks == rankSize) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else {
        EXPECT_NE(ret, HCCL_SUCCESS);
    }

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if(staged)
    {
    ret = tempAlg->RunAsyncStaged(rank, rankSize, link, stage);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    delete tempAlg;
}

#if 1
//对应代码中，ranksize ==1
TEST_F(AllReduceNHRV1InterTest, run_async_00)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/1,/*staged*/false);
}
#endif
#if 1
//对应代码中，ranksize ==8
TEST_F(AllReduceNHRV1InterTest, run_async_01)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/8,/*staged*/false);
}
#endif
#if 1
//对应代码中，ranksize ==8
TEST_F(AllReduceNHRV1InterTest, run_async_02)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/4, /*rankSize=*/8,/*staged*/false);
}
#endif

#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNHRV1InterTest, run_async_03)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/false);
}
#endif
#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNHRV1InterTest, run_async_04)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/true,/*Runstage*/RunStage::RUN_PREPARE);
}
#endif
#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNHRV1InterTest, run_async_05)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/true,/*Runstage*/RunStage::RUN_REDUCE_SCATTER);
}
#endif
#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNHRV1InterTest, run_async_06)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/true,/*Runstage*/RunStage::RUN_ALLREDUCE);
}
#endif
#if 1
//对应代码中，ranksize ==6
TEST_F(AllReduceNHRV1InterTest, run_async_07)
{
    run_async_test_allreduce_nhr_v1(dispatcher, /*rank=*/0, /*rankSize=*/6,/*staged*/true,/*Runstage*/RunStage::RUN_ALLGATHER);
}
#endif