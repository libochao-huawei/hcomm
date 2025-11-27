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
#include "alg_template_register.h"
#include "alltoallv_pairwise_pub.h"
#include "sal.h"

#define private public
#include "alltoallv_pairwise_pub.h"
#undef private


using namespace std;
using namespace hccl;

class AlltoAllVPairwiseTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AlltoAllVPairwiseTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AlltoAllVPairwiseTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AlltoAllVPairwiseTest::dispatcherPtr = nullptr;
DispatcherPub *AlltoAllVPairwiseTest::dispatcher = nullptr;

#if 1
TEST_F(AlltoAllVPairwiseTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";
    std::unique_ptr<AlgTemplateBase> executor = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_2_ALL_V_PAIRWISE, dispatcher);

    const u32 RANK_SIZE = 5;
    u64 counts[RANK_SIZE] = {100, 225, 0, 310, 100};
    u64 displs[RANK_SIZE] = {0,   100, 400, 400, 700};
    AlltoAllVBufferInfo sendBuffer;
    sendBuffer.mem = DeviceMem::alloc(1000);
    sendBuffer.counts = counts;
    sendBuffer.displs = displs;
    sendBuffer.dataType = HCCL_DATA_TYPE_FP32;

    AlltoAllVBufferInfo recvBuffer;
    recvBuffer.mem = DeviceMem::alloc(1000);
    recvBuffer.counts = counts;
    recvBuffer.displs = displs;
    recvBuffer.dataType = HCCL_DATA_TYPE_FP32;

    DeviceMem scratchInputMem =  DeviceMem::alloc(400);
    DeviceMem scratchOutputMem =  DeviceMem::alloc(400);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::map<u32, std::vector<u64>> rankSendDisplsMap = {};
    std::map<u32, std::vector<u64>> rankRecvDisplsMap = {};
    ret = executor->Prepare(sendBuffer, recvBuffer, scratchInputMem, scratchOutputMem, false, stream,
        HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE, rankSendDisplsMap, rankRecvDisplsMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > links;
    links.resize(RANK_SIZE);

    for (int i = 0; i < RANK_SIZE; i++)
    {
        links[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        links[i]->Init();
    }

    ret = executor->RunAsync(0, RANK_SIZE, links);

    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

TEST_F(AlltoAllVPairwiseTest, run_graph_alltoall)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    const u32 RANK_SIZE = 2;
    u64 counts[RANK_SIZE] = {100, 100};
    u64 displs[RANK_SIZE] = {0,   100};

    std::map<u32, std::vector<u64>> rankSendDisplsMap = {{0, {0, 100}}, {1, {0, 100}}};
    std::map<u32, std::vector<u64>> rankRecvDisplsMap = {{0, {0, 100}}, {1, {0, 100}}};
    std::unique_ptr<AlgTemplateBase> executor = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_2_ALL_V_PAIRWISE, dispatcher);

    AlltoAllVBufferInfo sendBuffer;
    sendBuffer.mem = DeviceMem::alloc(1000);
    sendBuffer.counts = counts;
    sendBuffer.displs = displs;
    sendBuffer.dataType = HCCL_DATA_TYPE_FP32;

    AlltoAllVBufferInfo recvBuffer;
    recvBuffer.mem = DeviceMem::alloc(1000);
    recvBuffer.counts = counts;
    recvBuffer.displs = displs;
    recvBuffer.dataType = HCCL_DATA_TYPE_FP32;

    DeviceMem scratchInputMem =  DeviceMem::alloc(400);
    DeviceMem scratchOutputMem =  DeviceMem::alloc(400);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = executor->Prepare(sendBuffer, recvBuffer, scratchInputMem, scratchOutputMem, true, stream,
                            HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB,
                            rankSendDisplsMap, rankRecvDisplsMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > links;
    links.resize(RANK_SIZE);

    for (int i = 0; i < RANK_SIZE; i++)
    {
        links[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        links[i]->Init();
    }

    ret = executor->RunAsync(0, RANK_SIZE, links);

    EXPECT_EQ(ret, HCCL_SUCCESS);
}
