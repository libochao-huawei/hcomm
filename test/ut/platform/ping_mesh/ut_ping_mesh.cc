/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <thread>
#include <chrono>
#include <securec.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#define private public
#include "hccl_network_pub.h"
#include "network_manager_pub.h"
#include "ping_mesh.h"
#undef private

#include "adapter_hccp.h"
#include "adapter_tdt.h"
#include "adapter_hal.h"
#include "adapter_rts.h"
#include "adapter_rts_common.h"
#include "hdc_pub.h"
#include "profiler_base_pub.h"
#include "tsd/tsd_client.h"
#include "network/hccp_common.h"
#include "network/hccp_ping.h"
#include "externalinput.h"

using namespace hccl;

// 打桩函数
inline HcclResult hrtGetDeviceStub(s32 *deviceLogicId)
{
    *deviceLogicId = 1;
    return HCCL_SUCCESS;
}

inline HcclResult hrtGetDevicePhyIdByIndexStub(u32 deviceLogicId, u32 &devicePhyId, bool isRefresh = false)
{
    devicePhyId = deviceLogicId;
    return HCCL_SUCCESS;
}

inline HcclResult hrtOpenNetServiceStub(rtNetServiceOpenArgs *openArgs)
{
    // *(openArgs->subPid) = 1;
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaGetInterfaceVersionStub(unsigned int phyId, unsigned int interfaceOpcode, unsigned int* interfaceVersion)
{
    *interfaceVersion = 1;
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaPingInitStub(struct ping_init_attr *initAttr, struct ping_init_info *initInfo, void **pingHandle)
{
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    *pingHandle = pingMesh.get();
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaPingDeinitStub(void *pingHandle)
{
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaPingTargetAddStub(void *pingHandle, struct ping_target_info target[], uint32_t num)
{
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaPingTargetDelStub(void *pingHandle, struct ping_target_info target[], uint32_t num)
{
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaPingTaskStartStub(void *pingHandle, struct ping_task_attr *attr)
{
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaPingTaskStopStub(void *pingHandle)
{
    return HCCL_SUCCESS;
}

inline HcclResult hrtRaPingGetResultsStub(void *pingHandle, struct ping_target_result output[], uint32_t *num)
{
    char *payload = "pingmesh";

    for (int i = 0; i < *num; i++) {
        HcclIpAddress targetIp = HcclIpAddress(0x7F000002 + i * 3);
        output[i].result.state = static_cast<ping_result_state>(i % 3);
        output[i].result.summary.rtt_avg = 10;
        output[i].result.summary.rtt_max = 35;
        output[i].result.summary.rtt_min = 5;
        output[i].result.summary.send_cnt = 2;
        output[i].result.summary.recv_cnt = 0;
 
        output[i].remote_info.qp_info = {0};
        output[i].remote_info.ip.addr.s_addr = targetIp.GetBinaryAddress().addr.s_addr;
    }
    return HCCL_SUCCESS;
}

class PingMesh_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "PingMesh_UT SetUP" << std::endl;
    }
 
    static void TearDownTestCase()
    {
        std::cout << "PingMesh_UT TearDown" << std::endl;
    }
 
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MOCKER(hrtGetDevice)
        .stubs()
        .with(any())
        .will(invoke(hrtGetDeviceStub));

        MOCKER(hrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtOpenNetService)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtCloseNetService)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtRaGetInterfaceVersion)
        .stubs()
        .with(any())
        .will(invoke(hrtRaGetInterfaceVersionStub));

        MOCKER(hrtRaPingInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtRaPingDeinit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtRaPingTargetDel)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtRaPingTaskStart)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtRaPingTaskStop)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtRaPingGetResults)
        .stubs()
        .with(any())
        .will(invoke(hrtRaPingGetResultsStub));

        MOCKER(hrtRaIsLastUsed)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(HrtRaInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(HrtRaDeInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtRaSocketInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclSocket::Listen, HcclResult(HcclSocket::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclSocket::Listen, HcclResult(HcclSocket::*)(u32))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclSocket::Accept)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclSocket::Connect)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclSocket::DeInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&std::thread::detach)
        .stubs()
        .with(any())
        .will(ignoreReturnValue());

        MOCKER(hrtMemSyncCopyEx)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtHalMemCtl)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "PingMesh_UT Test SetUP" << std::endl;
    }
 
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "PingMesh_UT Test TearDown" << std::endl;
    }
};

TEST_F(PingMesh_UT, ut_PingMeshInit)
{
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 10;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;
}

TEST_F(PingMesh_UT, ut_PingMeshAddDelTarget)
{
    MOCKER_CPP(&PingMesh::RpingRecvTargetInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaPingTargetAdd)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 10;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 添加目标
    hccl::RpingInput target[10];
    char *payload = "pingmesh";
    for (u32 i = 0; i < 10; i++) {
        HcclIpAddress targetIp = HcclIpAddress(0x7F000002 + i);
        target[i].sip = targetIp;
        target[i].dip = targetIp;
        target[i].port = 13866;
        target[i].tc = 1;
        target[i].sl = 1;
        target[i].srcPort = 0;
        target[i].len = 9;
        memcpy_s(target[i].payload, target[i].len, payload, target[i].len);
    }
    int i = 0;
    pingMesh->pingHandle_ = &i;
    pingMesh->rpingState_ = RpingState::INITED;
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 120000;
    ret = pingMesh->HccnRpingAddTarget(deviceId, nodeNum, target, &config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 删除目标
    void *netCtx;
    ret = HcclNetOpenDev(&netCtx, NicType::DEVICE_NIC_TYPE, deviceId, deviceId, target[i].sip);
    std::shared_ptr<HcclSocket> socket = std::make_shared<HcclSocket>(netCtx, port);
    socket->Init();
    pingMesh->socketMaps_.insert({target[i].sip.GetReadableIP(), socket});
    ret = pingMesh->HccnRpingRemoveTarget(deviceId, nodeNum, target);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    HcclNetCloseDev(netCtx);
}

TEST_F(PingMesh_UT, ut_PingMeshAddDelTargetPatch)
{
    MOCKER_CPP(&PingMesh::RpingRecvTargetInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaPingTargetAdd)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 1;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 添加目标
    hccl::RpingInput target[1];
    char *payload = "pingmesh";
    for (u32 i = 0; i < 1; i++) {
        HcclIpAddress targetIp = HcclIpAddress(0x7F000002 + i);
        target[i].sip = targetIp;
        target[i].dip = targetIp;
        target[i].port = 13866;
        target[i].tc = 1;
        target[i].sl = 1;
        target[i].srcPort = 0;
        target[i].len = 9;
        memcpy_s(target[i].payload, target[i].len, payload, target[i].len);
    }
    int i = 0;
    pingMesh->pingHandle_ = &i;
    pingMesh->rpingState_ = RpingState::INITED;
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 120000;
    ret = pingMesh->HccnRpingAddTarget(deviceId, nodeNum, target, &config);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = pingMesh->HccnRpingAddTarget(deviceId, nodeNum, target, &config);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 删除目标
    void *netCtx;
    ret = HcclNetOpenDev(&netCtx, NicType::DEVICE_NIC_TYPE, deviceId, deviceId, target[i].sip);
    std::shared_ptr<HcclSocket> socket = std::make_shared<HcclSocket>(netCtx, port);
    socket->Init();
    pingMesh->socketMaps_.insert({target[i].sip.GetReadableIP(), socket});
    ping_qp_info rdmainfo { 0 };
    pingMesh->rdmaInfoMaps_.insert({target[i].sip.GetReadableIP(), rdmainfo});
    ret = pingMesh->HccnRpingRemoveTarget(deviceId, nodeNum, target);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclNetCloseDev(netCtx);
}

TEST_F(PingMesh_UT, ut_PingMeshAddDelTargetPatch1)
{
    MOCKER_CPP(&PingMesh::RpingRecvTargetInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_NOT_FOUND));

    MOCKER(hrtRaPingTargetAdd)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 1;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 添加目标
    hccl::RpingInput target[1];
    char *payload = "pingmesh";
    for (u32 i = 0; i < 1; i++) {
        HcclIpAddress targetIp = HcclIpAddress(0x7F000002 + i);
        target[i].sip = targetIp;
        target[i].dip = targetIp;
        target[i].port = 13866;
        target[i].tc = 1;
        target[i].sl = 1;
        target[i].srcPort = 0;
        target[i].len = 100;
        memcpy_s(target[i].payload, 9, payload, 9);
    }
    int i = 0;
    pingMesh->pingHandle_ = &i;
    pingMesh->rpingState_ = RpingState::INITED;
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 120000;
    ret = pingMesh->HccnRpingAddTarget(deviceId, nodeNum, target, &config);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(PingMesh_UT, ut_PingMeshAddDelTargetPatch2)
{
    MOCKER_CPP(&PingMesh::RpingRecvTargetInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaPingTargetAdd)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_NOT_FOUND));
    
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 1;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 添加目标
    hccl::RpingInput target[1];
    char *payload = "pingmesh";
    for (u32 i = 0; i < 1; i++) {
        HcclIpAddress targetIp = HcclIpAddress(0x7F000002 + i);
        target[i].sip = targetIp;
        target[i].dip = targetIp;
        target[i].port = 13866;
        target[i].tc = 1;
        target[i].sl = 1;
        target[i].srcPort = 0;
        target[i].len = 100;
        memcpy_s(target[i].payload, 9, payload, 9);
    }
    int i = 0;
    pingMesh->pingHandle_ = &i;
    pingMesh->rpingState_ = RpingState::INITED;
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 120000;    
    ret = pingMesh->HccnRpingAddTarget(deviceId, nodeNum, target, &config);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(PingMesh_UT, ut_PingMeshAddDelTargetPatch3)
{
    MOCKER_CPP(&PingMesh::RpingRecvTargetInfo)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaPingTargetAdd)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(memcpy_s)
    .stubs()
    .with(any())
    .will(returnValue(1));
    
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 1;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 添加目标
    hccl::RpingInput target[1];
    char *payload = "pingmesh";
    for (u32 i = 0; i < 1; i++) {
        HcclIpAddress targetIp = HcclIpAddress(0x7F000002 + i);
        target[i].sip = targetIp;
        target[i].dip = targetIp;
        target[i].port = 13866;
        target[i].tc = 1;
        target[i].sl = 1;
        target[i].srcPort = 0;
        target[i].len = 1;
        memcpy_s(target[i].payload, 9, payload, 9);
    }
    int i = 0;
    pingMesh->pingHandle_ = &i;
    pingMesh->rpingState_ = RpingState::INITED;
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 120000;
    ret = pingMesh->HccnRpingAddTarget(deviceId, nodeNum, target, &config);
    EXPECT_EQ(ret, HCCL_E_MEMORY);
}

TEST_F(PingMesh_UT, ut_PingMeshStartStopPing)
{
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 10;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 启动任务
    int i = 0;
    pingMesh->pingHandle_ = &i;
    pingMesh->rpingState_ = RpingState::READY;
    pingMesh->rpingTargetNum_ = 2;
    ping_init_info &initInfo = pingMesh->initInfo_;
    initInfo.result.buffer_size = 4096 * 15;
    u32 pktNum = 10;
    u32 interval = 1;
    u32 timeout = 1000;
    ret = pingMesh->HccnRpingBatchPingStart(deviceId, pktNum, interval, timeout);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 中止任务
    ret = pingMesh->HccnRpingBatchPingStop(deviceId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(PingMesh_UT, ut_PingMeshGetResult)
{
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 10;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;

    // 初始化成功
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    auto ret = pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 获取结果
    hccl::RpingInput target[10];
    hccl::RpingOutput result[10];
    char *payload = "pingmesh";
    ping_qp_info rdmainfo {0};
    for (u32 i = 0; i < 10; i++) {
        HcclIpAddress targetIp = HcclIpAddress(0x7F000002 + i);
        pingMesh->rdmaInfoMaps_.insert({std::string(targetIp.GetReadableIP()), rdmainfo});
        target[i].sip = targetIp;
        target[i].dip = targetIp;
        target[i].port = 13866;
        target[i].tc = 1;
        target[i].sl = 1;
        target[i].srcPort = 0;
        target[i].len = 9;
        memcpy_s(target[i].payload, target[i].len, payload, target[i].len);
    }
    int i = 0;
    pingMesh->pingHandle_ = &i;
    pingMesh->rpingState_ = RpingState::RUN;
    pingMesh->rpingTargetNum_ = 10;
    ping_init_info &initInfo = pingMesh->initInfo_;
    initInfo.result.buffer_size = 4096 * 10;
    initInfo.result.payload_offset = 100;
    initInfo.result.header_size = 50;
    ret = pingMesh->HccnRpingGetResult(deviceId, nodeNum, target, result);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    pingMesh->isUsePayload_ = true;
    void *payloadout = nullptr;
    u32 pyaloadlenout = 0;
    ret = pingMesh->HccnRpingGetPayload(deviceId, &payloadout, &pyaloadlenout);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    pingMesh->isUsePayload_ = false;
    ret = pingMesh->HccnRpingGetPayload(deviceId, &payloadout, &pyaloadlenout);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(PingMesh_UT, ut_PingMeshStateCheck)
{
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 10;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;
    
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 未初始化状态
    pingMesh->rpingState_ = RpingState::UNINIT;
    hccl::RpingInput *target = nullptr;
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 120000;
    auto ret = pingMesh->HccnRpingAddTarget(deviceId, nodeNum, target, &config);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    u32 pktNum = 10;
    u32 interval = 1;
    u32 timeout = 1000;
    ret = pingMesh->HccnRpingBatchPingStart(deviceId, pktNum, interval, timeout);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = pingMesh->HccnRpingBatchPingStop(deviceId);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    // 初始化状态
    pingMesh->rpingState_ = RpingState::INITED;
    ret = pingMesh->HccnRpingBatchPingStart(deviceId, pktNum, interval, timeout);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    ret = pingMesh->HccnRpingBatchPingStop(deviceId);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);

    // 预备状态
    pingMesh->rpingState_ = RpingState::READY;
    ret = pingMesh->HccnRpingBatchPingStop(deviceId);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(PingMesh_UT, ut_PingMeshSendRecvInfo)
{
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 10;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;
    
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 发送成功
    ping_init_info initInfo {};
    std::shared_ptr<HcclSocket> socket = pingMesh->socket_;
    pingMesh->connThreadStop_.store(true);
    auto ret = pingMesh->RpingSendInitInfo(deviceId, port, ipAddr, initInfo, socket);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 接收成功
    MOCKER_CPP(&HcclSocket::GetStatus)
    .expects(atMost(1))
    .will(returnValue(HcclSocketStatus::SOCKET_OK));

    void *clientNetCtx = pingMesh->netCtx_;
    ret = pingMesh->RpingRecvTargetInfo(clientNetCtx, port, ipAddr, initInfo, 120000);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(PingMesh_UT, ut_PingMeshSendRecvInfoPatch)
{
    // 补充覆盖率
    u32 deviceId = 1;
    u32 mode = static_cast<u32>(LinkType::LINK_ROCE);
    HcclIpAddress ipAddr = HcclIpAddress(0x7F000001);
    u32 port = 13866;
    u32 nodeNum = 10;
    u32 bufferSize = 100U;
    u32 sl = 0;
    u32 tc = 0;
    
    std::shared_ptr<PingMesh> pingMesh;
    pingMesh.reset(new (std::nothrow) PingMesh());
    pingMesh->HccnRpingInit(deviceId, mode, ipAddr, port, nodeNum, bufferSize, sl, tc);
    ProcOpenArgs *openArgs = &(pingMesh->hccpProcessInfo_);
    int subpid = 1;
    openArgs->subPid = &subpid;
    openArgs->procType = TSD_SUB_PROC_HCCP;

    // 发送成功
    MOCKER_CPP(&HcclSocket::GetStatus)
    .stubs()
    .will(returnValue(HcclSocketStatus::SOCKET_CONNECTING));

    ping_init_info initInfo {};
    std::shared_ptr<HcclSocket> socket = pingMesh->socket_;
    std::thread backgroundTh(&PingMesh::RpingSendInitInfo, pingMesh, deviceId, port, ipAddr, initInfo, socket);
    usleep(1000);
    pingMesh->connThreadStop_.store(true);
    backgroundTh.join();
}
