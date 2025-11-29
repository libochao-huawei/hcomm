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
#include "all_reduce_hd_optim_pub.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
#include "dispatcher_pub.h"
#include "alg_template_register.h"

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

class AllReduceHDOptimTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceHDOptimTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceHDOptimTest TearDown--\033[0m" << std::endl;
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
    std::vector<Stream> subStreams = {stream, stream};
    std::shared_ptr<LocalNotify> mainSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> mainSignals = {mainSignal, mainSignal};
    std::shared_ptr<LocalNotify> subSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> subSignals = {subSignal, subSignal};

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher AllReduceHDOptimTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceHDOptimTest::dispatcher = nullptr;

TEST_F(AllReduceHDOptimTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS; 
    std::string                      collect_id = "test-destructor"; 
    std::vector<RankInfo> para_vector(1);
    AllReduceHDOptim *tempAlg = new AllReduceHDOptim(dispatcher);
    tempAlg->Prepare(1, subStreams, mainSignals, subSignals, userRank, &opInfo, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

TEST_F(AllReduceHDOptimTest, run_async_05)
{
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclReduceAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
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
    
    AllReduceHDOptim *tempAlg = new AllReduceHDOptim(dispatcher);
    tempAlg->Prepare(1, subStreams, mainSignals, subSignals, userRank, &opInfo, false);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = tempAlg->Prepare(output, output, input, 1, HCCL_DATA_TYPE_INT8, stream, HcclReduceOp::HCCL_REDUCE_MAX, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 5, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

TEST_F(AllReduceHDOptimTest, run_async_45)
{
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&HcclReduceAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
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
    
    AllReduceHDOptim *tempAlg = new AllReduceHDOptim(dispatcher);
    tempAlg->Prepare(1, subStreams, mainSignals, subSignals, userRank, &opInfo, false);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = tempAlg->Prepare(output, output, input, 1, HCCL_DATA_TYPE_INT8, stream, HcclReduceOp::HCCL_REDUCE_MAX, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(4, 5, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

TEST_F(AllReduceHDOptimTest, AllReduceHDOptim_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_HD_OPTIM, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    Stream streamInstance = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> meshStreams = {streamInstance, streamInstance, streamInstance};
    std::shared_ptr<LocalNotify> signalPtr{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> meshSignal = {signalPtr, signalPtr, signalPtr};
    std::vector<std::shared_ptr<LocalNotify>> meshSignalAux = {signalPtr, signalPtr, signalPtr};
    bool aicpu = false;
    u32 userRank = 0;
    HcomCollOpInfo opInfo{0};

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(
        reduceAttrBitMap, meshStreams, meshSignal, meshSignalAux, userRank, &opInfo, aicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceHDOptim *alg = dynamic_cast<AllReduceHDOptim *>(tempAlg.get());

	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_EQ(alg->opInfo_, &opInfo);
    EXPECT_EQ(alg->aicpu_, aicpu);

    EXPECT_EQ(alg->meshStreams_.size(), meshStreams.size());
    for (u32 i = 0; i < meshStreams.size(); ++i) {
        EXPECT_EQ(alg->meshStreams_[i].id(), meshStreams[i].id());
    }

    EXPECT_EQ((*(alg->meshSignal_)).size(), meshSignal.size());
    for (u32 i = 0; i < meshSignal.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignal_))[i], meshSignal[i]);
    }

     EXPECT_EQ((*(alg->meshSignalAux_)).size(), meshSignalAux.size());
    for (u32 i = 0; i < meshSignalAux.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignalAux_))[i], meshSignalAux[i]);
    }
}

