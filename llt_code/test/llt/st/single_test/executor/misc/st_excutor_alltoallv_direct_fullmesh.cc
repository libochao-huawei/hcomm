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
#include "common.h"
#define private public
#define protected public
#include "alg_template_base_pub.h"
#include "alltoallv_direct_fullmesh_pub.h"
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

class AlltoallDirectFullmeshTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--alltoallv_direct_fullmesh_Test SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--alltoallv_direct_fullmesh_Test TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        localSendRecvInfo.sendCounts.resize(userRankSize, 0);
        localSendRecvInfo.sendDispls.resize(userRankSize, 0);
        localSendRecvInfo.sendLength.resize(userRankSize, 0);
        localSendRecvInfo.sendOffset.resize(userRankSize, 0);

        localSendRecvInfo.recvCounts.resize(userRankSize, 0);
        localSendRecvInfo.recvDispls.resize(userRankSize, 0);
        localSendRecvInfo.recvLength.resize(userRankSize, 0);
        localSendRecvInfo.recvOffset.resize(userRankSize, 0);
        u64 curSendDispls = 0;
        u64 curSendOffset = 0;
        u64 curRecvDispls = 0;
        u64 curRecvOffset = 0;
        for (u32 j = 0; j < userRankSize; j++) {
            u64 curSendCounts = 16;
            u64 curSendLength = curSendCounts * SIZE_TABLE[HcclDataType::HCCL_DATA_TYPE_INT8];
            localSendRecvInfo.sendCounts[j] = curSendCounts;
            localSendRecvInfo.sendDispls[j] = curSendDispls;
            localSendRecvInfo.sendLength[j] = curSendLength;
            localSendRecvInfo.sendOffset[j] = curSendOffset;
            curSendDispls += curSendCounts;
            curSendOffset += curSendLength;

            u64 curRecvCounts = 16;
            u64 curRecvLength = curRecvCounts * SIZE_TABLE[HcclDataType::HCCL_DATA_TYPE_INT8];
            localSendRecvInfo.recvCounts[j] = curRecvCounts;
            localSendRecvInfo.recvDispls[j] = curRecvDispls;
            localSendRecvInfo.recvLength[j] = curRecvLength;
            localSendRecvInfo.recvOffset[j] = curRecvOffset;
            curRecvDispls += curRecvCounts;
            curRecvOffset += curRecvLength;
            HCCL_DEBUG("GetLocalSendRecvInfoforAlltoall rank[%u], sendCounts[%llu], sendDispls[%llu] "\
                "recvCounts[%llu], recvDispls[%llu]", userRank, localSendRecvInfo.sendCounts[j],
                localSendRecvInfo.sendDispls[j], localSendRecvInfo.recvCounts[j],
                localSendRecvInfo.recvDispls[j]);
        }
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
    DeviceMem inputMem  = DeviceMem::alloc(32);
    DeviceMem outputMem = DeviceMem::alloc(32);
    u32 userRank   = 0;
    u32 userRankSize = 2;
    SendRecvInfo localSendRecvInfo;
    u32 podNum = 2;
    u32 localPodDevNum = 1;
    u32 rankIdxInPod = 0;
    Stream                                    stream = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream>                       subStreams = {stream, stream, stream, stream};
    std::shared_ptr<LocalNotify>              mainSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> mainSignals = {mainSignal, mainSignal, mainSignal, mainSignal};
    std::shared_ptr<LocalNotify>              subSignal{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> subSignals = {subSignal, subSignal, subSignal, subSignal};

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher AlltoallDirectFullmeshTest::dispatcherPtr = nullptr;
DispatcherPub *AlltoallDirectFullmeshTest::dispatcher = nullptr;

#if 0
TEST_F(AlltoallDirectFullmeshTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string                      collect_id = "test_run_async_00_combine";
    std::vector<RankInfo>     para_vector(1);
    MachinePara               machinePara;
    std::chrono::milliseconds timeout;
    const std::string         tag;

    std::vector<std::shared_ptr<Transport>> links;
    links.resize(2);

    for (int i = 0; i < 2; i++) {
        links[i].reset(new (std::nothrow)
                          Transport(new (std::nothrow) TransportBase(dispatcher, nullptr, machinePara, timeout)));
        links[i]->Init();
    }

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalRecord, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u64,
        s32, bool, u64, u32)).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP_VIRTUAL(*dispatcher, &DispatcherPub::SignalWait, HcclResult(DispatcherPub::*)(HcclRtNotify, hccl::Stream &, u32, u32,
        s32, bool, u32, u32)).stubs().will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Transport::GetRemoteMem, HcclResult(Transport::*)(hccl::UserMemType, void**))
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcclD2DMemcpyAsync)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Wait)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Post)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(AlgTemplateBase::ExecEmptyTask)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    AlltoAllVDirectFullMesh *tempAlg = new AlltoAllVDirectFullMesh(dispatcher);

    DeviceMem output = DeviceMem::alloc(100);
    DeviceMem input = DeviceMem::alloc(100);

    AlgOpContext algOpContext;
    algOpContext.mc2Handler.stepSize = 1;

    PrepareData param;
    param.inputMem = inputMem;
    param.outputMem = outputMem;
    param.cclInMem = input;
    param.cclOutMem = output;
    param.workMode = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    param.stream = stream;
    param.userRank = userRank;
    param.userRankSize = userRankSize;
    param.linksPtr = &links;
    param.localSendRecvInfoPtr = &localSendRecvInfo;
    param.devNumInlocalPod = localPodDevNum;
    param.rankIdxInPod = rankIdxInPod;
    param.subStreamsPtr = &subStreams;
    param.signalPtr = &mainSignals;
    param.signalAuxPtr = &subSignals;
    param.algOpContext = algOpContext;

    ret = tempAlg->Prepare(param);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif