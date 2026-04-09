/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#ifndef private
#define private public
#define protected public
#endif

#include "transport_ibverbs_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"
#include "adapter_hccp.h"
#include "network_manager_pub.h"
#include "externalinput.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportIbverbsCreateQp_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsCreateQp_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsCreateQp_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        // 初始化dispatcher
        dispatcher = new (std::nothrow) DispatcherPub(1);
        
        // 初始化notifyPool
        notifyPool.reset(new (std::nothrow) NotifyPool());

        // 初始化MachinePara
        machinePara.deviceLogicId = 0;
        machinePara.localDeviceId = 0;
        machinePara.remoteDeviceId = 1;
        machinePara.localUserrank = 0;
        machinePara.localWorldRank = 0;
        machinePara.remoteUserrank = 1;
        machinePara.remoteWorldRank = 1;
        machinePara.deviceType = DevType::DEV_TYPE_910B;
        machinePara.isAicpuModeEn = false;
        machinePara.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
        machinePara.qpMode = QPMode::NORMAL;
        machinePara.enableAtomicWrite = false;
        machinePara.isIndOp = false;
        machinePara.notifyNum = 1;
        machinePara.tc = 0;
        machinePara.sl = 0;
        
        // 设置IP地址 - 使用u32构造
        machinePara.localIpAddr = HcclIpAddress(0x7F000001); // 127.0.0.1
        machinePara.remoteIpAddr = HcclIpAddress(0x7F000001);

        // 设置源端口
        machinePara.srcPorts.push_back(10000);
        machinePara.srcPorts.push_back(10001);

        // 设置内存
        inputMem = DeviceMem::alloc(1024);
        outputMem = DeviceMem::alloc(1024);
        machinePara.inputMem = inputMem;
        machinePara.outputMem = outputMem;

        // Mock network manager
        RaResourceInfo raResourceInfo;
        IpSocket ipSocket;
        ipSocket.nicRdmaHandle = reinterpret_cast<RdmaHandle>(0x1234);
        raResourceInfo.nicSocketMap[machinePara.localIpAddr] = ipSocket;
        
        MOCKER(NetworkManager::GetInstance)
            .stubs()
            .will(returnValue(raResourceInfo));

        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
        delete dispatcher;
        GlobalMockObject::verify();
    }

    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    DeviceMem inputMem;
    DeviceMem outputMem;
};

// 测试CreateOneQp成功场景 - AICPU模式
TEST_F(TransportIbverbsCreateQp_UT, CreateOneQp_Success_AicpuMode)
{
    HcclResult ret;
    QpHandle qpHandle = nullptr;
    AiQpInfo aiQpInfo{};
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 1;
    bool useAicpu = true;
    u32 udpSport = 10000;

    // Mock HrtRaAiQpCreate
    QpHandle mockQpHandle = reinterpret_cast<QpHandle>(0x5678);
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock hrtRaGetQpAttr
    struct QpAttr mockQpAttr{};
    mockQpAttr.qpn = 12345;
    mockQpAttr.udpSport = 10000;
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateOneQp
    ret = transportIbverbs.CreateOneQp(qpMode, qpsPerConnection, qpHandle, aiQpInfo, useAicpu, udpSport);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(qpHandle, nullptr);
}

// 测试CreateOneQp成功场景 - 非AICPU模式
TEST_F(TransportIbverbsCreateQp_UT, CreateOneQp_Success_NonAicpuMode)
{
    HcclResult ret;
    QpHandle qpHandle = nullptr;
    AiQpInfo aiQpInfo{};
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 1;
    bool useAicpu = false;
    u32 udpSport = 10000;

    // Mock hrtRaQpCreateWithAttrs
    MOCKER(hrtRaQpCreateWithAttrs)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock hrtRaGetQpAttr
    struct QpAttr mockQpAttr{};
    mockQpAttr.qpn = 12345;
    mockQpAttr.udpSport = 10000;
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateOneQp
    ret = transportIbverbs.CreateOneQp(qpMode, qpsPerConnection, qpHandle, aiQpInfo, useAicpu, udpSport);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(qpHandle, nullptr);
}

// 测试CreateOneQp - SetQpAttrQos失败
TEST_F(TransportIbverbsCreateQp_UT, CreateOneQp_Fail_SetQpAttrQos)
{
    HcclResult ret;
    QpHandle qpHandle = nullptr;
    AiQpInfo aiQpInfo{};
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 1;
    bool useAicpu = true;
    u32 udpSport = 10000;

    // Mock HrtRaAiQpCreate success
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock hrtRaGetQpAttr fail
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_E_ROCE_CONNECT));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateOneQp
    ret = transportIbverbs.CreateOneQp(qpMode, qpsPerConnection, qpHandle, aiQpInfo, useAicpu, udpSport);

    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    EXPECT_EQ(qpHandle, nullptr);
}

// 测试CreateOneQp - HrtRaAiQpCreate失败
TEST_F(TransportIbverbsCreateQp_UT, CreateOneQp_Fail_HrtRaAiQpCreate)
{
    HcclResult ret;
    QpHandle qpHandle = nullptr;
    AiQpInfo aiQpInfo{};
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 1;
    bool useAicpu = true;
    u32 udpSport = 10000;

    // Mock HrtRaAiQpCreate fail
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue(HCCL_E_ROCE_CONNECT));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateOneQp
    ret = transportIbverbs.CreateOneQp(qpMode, qpsPerConnection, qpHandle, aiQpInfo, useAicpu, udpSport);

    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    EXPECT_EQ(qpHandle, nullptr);
}

// 测试CreateMultiQp成功场景
TEST_F(TransportIbverbsCreateQp_UT, CreateMultiQp_Success)
{
    HcclResult ret;
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 2;

    // Mock hrtRaAiQpCreate success
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock hrtRaGetQpAttr success
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateMultiQp
    ret = transportIbverbs.CreateMultiQp(qpMode, qpsPerConnection);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(transportIbverbs.multiCombineQpHandles_.size(), 2);
    EXPECT_EQ(transportIbverbs.combineAiQpInfos_.size(), 2);
}

// 测试CreateMultiQp - 第二个QP创建失败，回滚第一个QP
TEST_F(TransportIbverbsCreateQp_UT, CreateMultiQp_Fail_SecondQp_Rollback)
{
    HcclResult ret;
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 2;

    // Mock hrtRaAiQpCreate: first success, second fail
    u32 createCallCount = 0;
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue((createCallCount++ == 0) ? HCCL_SUCCESS : HCCL_E_ROCE_CONNECT));

    // Mock hrtRaGetQpAttr success for first QP
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock HrtRaQpDestroy for rollback
    MOCKER(HrtRaQpDestroy)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateMultiQp
    ret = transportIbverbs.CreateMultiQp(qpMode, qpsPerConnection);

    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    EXPECT_EQ(transportIbverbs.multiCombineQpHandles_.size(), 0);
    EXPECT_EQ(transportIbverbs.combineAiQpInfos_.size(), 0);
}

// 测试CreateMultiQp - 第一个QP创建失败
TEST_F(TransportIbverbsCreateQp_UT, CreateMultiQp_Fail_FirstQp)
{
    HcclResult ret;
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 2;

    // Mock hrtRaAiQpCreate fail
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue(HCCL_E_ROCE_CONNECT));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateMultiQp
    ret = transportIbverbs.CreateMultiQp(qpMode, qpsPerConnection);

    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    EXPECT_EQ(transportIbverbs.multiCombineQpHandles_.size(), 0);
    EXPECT_EQ(transportIbverbs.combineAiQpInfos_.size(), 0);
}

// 测试CreateMultiQp - SetQpAttrQos失败，回滚
TEST_F(TransportIbverbsCreateQp_UT, CreateMultiQp_Fail_SetQpAttrQos_Rollback)
{
    HcclResult ret;
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 2;

    // Mock hrtRaAiQpCreate success
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock hrtRaGetQpAttr fail
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_E_ROCE_CONNECT));

    // Mock HrtRaQpDestroy for rollback
    MOCKER(HrtRaQpDestroy)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateMultiQp
    ret = transportIbverbs.CreateMultiQp(qpMode, qpsPerConnection);

    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    EXPECT_EQ(transportIbverbs.multiCombineQpHandles_.size(), 0);
    EXPECT_EQ(transportIbverbs.combineAiQpInfos_.size(), 0);
}

// 测试CreateOneQp - 使用不同的udpSport
TEST_F(TransportIbverbsCreateQp_UT, CreateOneQp_DifferentUdpSport)
{
    HcclResult ret;
    QpHandle qpHandle1 = nullptr;
    QpHandle qpHandle2 = nullptr;
    AiQpInfo aiQpInfo1{};
    AiQpInfo aiQpInfo2{};
    s32 qpMode = OPBASE_QP_MODE_EXT;
    u32 qpsPerConnection = 1;
    bool useAicpu = true;
    u32 udpSport1 = 10000;
    u32 udpSport2 = 10001;

    // Mock hrtRaAiQpCreate
    MOCKER(hrtRaAiQpCreate)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock hrtRaGetQpAttr
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Create TransportIbverbs instance
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    // Call CreateOneQp with different udpSport
    ret = transportIbverbs.CreateOneQp(qpMode, qpsPerConnection, qpHandle1, aiQpInfo1, useAicpu, udpSport1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(qpHandle1, nullptr);

    ret = transportIbverbs.CreateOneQp(qpMode, qpsPerConnection, qpHandle2, aiQpInfo2, useAicpu, udpSport2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(qpHandle2, nullptr);
}