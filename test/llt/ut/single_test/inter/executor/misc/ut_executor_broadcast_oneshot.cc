/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
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
#define protected public
#include "alg_template_base_pub.h"
#include "alg_template_register.h"
#include "broadcast_oneshot_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
#include "dispatcher_pub.h"

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

class BroadcastHDTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--BroadcastHDTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--BroadcastHDTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        opInfo.inputAddr  = inputMem.ptr();
        opInfo.outputAddr = inputMem.ptr();
        opInfo.count      = 360;
        opInfo.dataType   = HCCL_DATA_TYPE_FP32;
        opInfo.reduceOp   = HCCL_REDUCE_SUM;
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
    DeviceMem inputMem  = DeviceMem::alloc(360);
    HcomCollOpInfo opInfo;
    u32 userRank   = 0;
    Stream stream = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> subStreams = {stream};
    std::shared_ptr<LocalNotify> mainSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> mainSignals = {mainSignal};
    std::shared_ptr<LocalNotify> subSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> subSignals = {subSignal};

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher BroadcastHDTest::dispatcherPtr = nullptr;
DispatcherPub *BroadcastHDTest::dispatcher = nullptr;

TEST_F(BroadcastHDTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS; 
    std::string                      collect_id = "test-destructor"; 
    std::vector<RankInfo> para_vector(1);
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_HD, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(BroadcastHDTest, run_async_05)
{
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_00_combine";
    std::vector<RankInfo>     para_vector(1);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    DeviceMem output = DeviceMem::alloc(100);

    DeviceMem input = DeviceMem::alloc(100);

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(5);

    for (int i = 0; i < 5; i++) {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }
    
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_HD, dispatcher);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = tempAlg->Prepare(output, output, input, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0,
        subStreams, mainSignals, subSignals, userRank, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 5, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(BroadcastHDTest, run_async_35)
{
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_00_combine";
    std::vector<RankInfo>     para_vector(1);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    DeviceMem output = DeviceMem::alloc(100);

    DeviceMem input = DeviceMem::alloc(100);

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(5);

    for (int i = 0; i < 5; i++) {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }
    
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_HD, dispatcher);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = tempAlg->Prepare(output, output, input, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0,
        subStreams, mainSignals, subSignals, 1, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(3, 5, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(BroadcastHDTest, run_async_45)
{
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_00_combine";
    std::vector<RankInfo>     para_vector(1);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    DeviceMem output = DeviceMem::alloc(100);

    DeviceMem input = DeviceMem::alloc(100);

    std::vector<std::shared_ptr<Transport>> link;
    link.resize(5);

    for (int i = 0; i < 5; i++) {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }
    
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_HD, dispatcher);


    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = tempAlg->Prepare(output, output, input, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0,
        subStreams, mainSignals, subSignals, 1, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(4, 5, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
