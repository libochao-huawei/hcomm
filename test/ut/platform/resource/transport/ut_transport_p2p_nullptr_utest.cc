/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to License for details. You may not use this file except in compliance with License.
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

#include "transport_p2p_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"

#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_aicpu_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportP2pNullPtr_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportP2pNullPtr_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportP2pNullPtr_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        // 初始化dispatcher
        dispatcher = new (std::nothrow) DispatcherAiCpu(0);

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
        machinePara.deviceType = DeviceType::DEVICE_TYPE_NPU;
        machinePara.linkAttribute = 0;
        machinePara.linkMode = LinkMode::LINK_DUPLEX_MODE;
        machinePara.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machinePara.serverId = "server1";
        machinePara.tag = "test_transport";
        machinePara.notifyNum = 0;
        machinePara.isIndOp = false;
        machinePara.isAicpuModeEn = false;

        // 初始化inputMem和outputMem
        u32 memSize = 1024;
        inputMem = DeviceMem::alloc(memSize);
        outputMem = DeviceMem::alloc(memSize);
        machinePara.inputMem = inputMem;
        machinePara.outputMem = outputMem;

        // Mock必要的函数
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));

        std::cout << "TransportP2pNullPtr_UT SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "TransportP2pNullPtr_UT TearDown" << std::endl;
        delete dispatcher;
        GlobalMockObject::verify();
    }

    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    MachinePara machinePara;
    std::chrono::milliseconds timeout{1000};
    DeviceMem inputMem;
    DeviceMem outputMem;
    s32 portNum = 7;
};

// ============================================================================
// TransportP2p::TxAsync - 测试 CHK_PTR_NULL(dstMemPtr)
// ============================================================================
TEST_F(TransportP2pNullPtr_UT, TransportP2p_TxAsync_null_dstMemPtr)
{
    // 设置不支持目的端发起
    machinePara.linkAttribute = 0;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 设置远程内存指针为nullptr，触发CHK_PTR_NULL(dstMemPtr)
    transportP2p.remoteOutputPtr_ = nullptr;

    Stream stream;
    const void *src = outputMem.ptr();
    u64 len = 1024;
    u64 dstOffset = 0;
    UserMemType dstMemType = UserMemType::OUTPUT_MEM;

    // Mock SignalRecord返回成功
    MOCKER(TransportP2p::SignalRecord)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // 期望返回HCCL_E_PTR错误
    HcclResult ret = transportP2p.TxAsync(dstMemType, dstOffset, src, len, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TransportP2pNullPtr_UT, TransportP2p_TxAsync_valid_dstMemPtr)
{
    // 设置不支持目的端发起
    machinePara.linkAttribute = 0;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 设置远程内存指针为有效值
    transportP2p.remoteOutputPtr_ = outputMem.ptr();

    // Mock HcclD2DMemcpyAsync返回成功
    MOCKER(HcclD2DMemcpyAsync)
        .stubs()
        .with(any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // Mock SignalRecord返回成功
    MOCKER(TransportP2p::SignalRecord)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    Stream stream;
    const void *src = inputMem.ptr();
    u64 len = 1024;
    u64 dstOffset = 0;
    UserMemType dstMemType = UserMemType::OUTPUT_MEM;

    // 期望返回HCCL_SUCCESS
    HcclResult ret = transportP2p.TxAsync(dstMemType, dstOffset, src, len, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// ============================================================================
// TransportP2p::TxAsync(vector) - 测试 CHK_PTR_NULL(mem.src) 和 CHK_PTR_NULL(dstMemPtr)
// ============================================================================
TEST_F(TransportP2pNullPtr_UT, TransportP2p_TxAsync_vector_null_src)
{
    // 设置不支持目的端发起
    machinePara.linkAttribute = 0;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 设置远程内存指针为有效值
    transportP2p.remoteOutputPtr_ = outputMem.ptr();

    // Mock SignalRecord返回成功
    MOCKER(TransportP2p::SignalRecord)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    Stream stream;
    std::vector<TxMemoryInfo> txMems;
    TxMemoryInfo memInfo;
    memInfo.dstMemType = UserMemType::OUTPUT_MEM;
    memInfo.dstOffset = 0;
    memInfo.src = nullptr;  // 设置src为nullptr，触发CHK_PTR_NULL(mem.src)
    memInfo.len = 1024;
    txMems.push_back(memInfo);

    // 期望返回HCCL_E_PTR错误
    HcclResult ret = transportP2p.TxAsync(txMems, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TransportP2pNullPtr_UT, TransportP2p_TxAsync_vector_null_dstMemPtr)
{
    // 设置不支持目的端发起
    machinePara.linkAttribute = 0;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 设置远程内存指针为nullptr，触发CHK_PTR_NULL(dstMemPtr)
    transportP2p.remoteOutputPtr_ = nullptr;

    // Mock SignalRecord返回成功
    MOCKER(TransportP2p::SignalRecord)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    Stream stream;
    std::vector<TxMemoryInfo> txMems;
    TxMemoryInfo memInfo;
    memInfo.dstMemType = UserMemType::OUTPUT_MEM;
    memInfo.dstOffset = 0;
    memInfo.src = inputMem.ptr();
    memInfo.len = 1024;
    txMems.push_back(memInfo);

    // 期望返回HCCL_E_PTR错误
    HcclResult ret = transportP2p.TxAsync(txMems, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// ============================================================================
// TransportP2p::RxAsync - 测试 CHK_PTR_NULL(srcMemPtr)
// ============================================================================
TEST_F(TransportP2pNullPtr_UT, TransportP2p_RxAsync_null_srcMemPtr)
{
    // 设置支持目的端发起
    machinePara.linkAttribute = 0x2;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // Mock SignalWait返回成功
    MOCKER(DispatcherAiCpu::SignalWait)
        .stubs()
        .with(any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // 设置远程内存指针为nullptr，触发CHK_PTR_NULL(srcMemPtr)
    transportP2p.remoteInputPtr_ = nullptr;

    Stream stream;
    void *dst = outputMem.ptr();
    u64 len = 1024;
    u64 srcOffset = 0;
    UserMemType srcMemType = UserMemType::INPUT_MEM;

    // 期望返回HCCL_E_PTR错误
    HcclResult ret = transportP2p.RxAsync(srcMemType, srcOffset, dst, len, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TransportP2pNullPtr_UT, TransportP2p_RxAsync_valid_srcMemPtr)
{
    // 设置支持目的端发起
    machinePara.linkAttribute = 0x2;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // Mock SignalWait返回成功
    MOCKER(DispatcherAiCpu::SignalWait)
        .stubs()
        .with(any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // Mock HcclD2DMemcpyAsync返回成功
    MOCKER(HcclD2DMemcpyAsync)
        .stubs()
        .with(any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // 设置远程内存指针为有效值
    transportP2p.remoteInputPtr_ = inputMem.ptr();

    Stream stream;
    void *dst = outputMem.ptr();
    u64 len = 1024;
    u64 srcOffset = 0;
    UserMemType srcMemType = UserMemType::INPUT_MEM;

    // 期望返回HCCL_SUCCESS
    HcclResult ret = transportP2p.RxAsync(srcMemType, srcOffset, dst, len, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// ============================================================================
// TransportP2p::RxAsync(vector) - 测试 CHK_PTR_NULL(mem.dst) 和 CHK_PTR_NULL(srcMemPtr)
// ============================================================================
TEST_F(TransportP2pNullPtr_UT, TransportP2p_RxAsync_vector_null_dst)
{
    // 设置支持目的端发起
    machinePara.linkAttribute = 0x2;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // Mock SignalWait返回成功
    MOCKER(DispatcherAiCpu::SignalWait)
        .stubs()
        .with(any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // 设置远程内存指针为有效值
    transportP2p.remoteInputPtr_ = inputMem.ptr();

    Stream stream;
    std::vector<RxMemoryInfo> rxMems;
    RxMemoryInfo memInfo;
    memInfo.srcMemType = UserMemType::INPUT_MEM;
    memInfo.srcOffset = 0;
    memInfo.dst = nullptr;  // 设置dst为nullptr，触发CHK_PTR_NULL(mem.dst)
    memInfo.len = 1024;
    rxMems.push_back(memInfo);

    // 期望返回HCCL_E_PTR错误
    HcclResult ret = transportP2p.RxAsync(rxMems, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TransportP2pNullPtr_UT, TransportP2p_RxAsync_vector_null_srcMemPtr)
{
    // 设置支持目的端发起
    machinePara.linkAttribute = 0x2;

    // 创建TransportP2p对象
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // Mock SignalWait返回成功
    MOCKER(DispatcherAiCpu::SignalWait)
        .stubs()
        .with(any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // 设置远程内存指针为nullptr，触发CHK_PTR_NULL(srcMemPtr)
    transportP2p.remoteInputPtr_ = nullptr;

    Stream stream;
    std::vector<RxMemoryInfo> rxMems;
    RxMemoryInfo memInfo;
    memInfo.srcMemType = UserMemType::INPUT_MEM;
    memInfo.srcOffset = 0;
    memInfo.dst = outputMem.ptr();
    memInfo.len = 1024;
    rxMems.push_back(memInfo);

    // 期望返回HCCL_E_PTR错误
    HcclResult ret = transportP2p.RxAsync(rxMems, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}