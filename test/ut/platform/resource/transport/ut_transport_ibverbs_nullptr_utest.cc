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

#ifndef private
#define private public
#define protected public
#endif

#include "transport_ibverbs_pub.h"
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

class TransportIbverbsNullPtr_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsNullPtr_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsNullPtr_UT TearDown--\033[0m" << std::endl;
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
        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(localDeviceType))
            .will(returnValue(HCCL_SUCCESS));

        std::cout << "TransportIbverbsNullPtr_UT SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "TransportIbverbsNullPtr_UT TearDown" << std::endl;
        delete dispatcher;
        GlobalMockObject::verify();
    }

    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    MachinePara machinePara;
    std::chrono::milliseconds timeout{1000};
    DeviceMem inputMem;
    DeviceMem outputMem;
    DeviceType localDeviceType = DeviceType::DEVICE_TYPE_NPU;
};

// ============================================================================
// TransportIbverbs::TxPayLoad - 测试 CHK_PTR_NULL(dstMemPtr)
// ============================================================================
TEST_F(TransportIbverbsNullPtr_UT, TransportIbverbs_TxPayLoad_null_dstMemPtr)
{
    // Mock GetMemInfo返回nullptr以触发CHK_PTR_NULL(dstMemPtr)
    MOCKER(TransportIbverbs::GetMemInfo)
        .stubs()
        .with(any(), any(), any())
        .will(invoke([](UserMemType memType, void** memPtr, u64* memSize) {
            *memPtr = nullptr;  // 设置为nullptr
            *memSize = 1024;
            return HCCL_SUCCESS;
        }));

    // 创建TransportIbverbs对象
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    Stream stream;
    const void *src = inputMem.ptr();
    u64 len = 1024;
    u64 dstOffset = 0;
    UserMemType dstMemType = UserMemType::OUTPUT_MEM;
    WqeType wqeType = WqeType::WQE_TYPE_DATA;
    WrAuxInfo aux = {0};
    std::vector<WqeInfo> wqeInfoVec;

    // 期望返回HCCL_E_PTR错误
    HcclResult ret = transportIbverbs.TxPayLoad(dstMemType, dstOffset, src, len, wqeType, aux, wqeInfoVec);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TransportIbverbsNullPtr_UT, TransportIbverbs_TxPayLoad_valid_dstMemPtr)
{
    // Mock GetMemInfo返回有效指针
    void *validPtr = outputMem.ptr();
    MOCKER(TransportIbverbs::GetMemInfo)
        .stubs()
        .with(any(), any(), any())
        .will(invoke([validPtr](UserMemType memType, void** memPtr, u64* memSize) {
            *memPtr = validPtr;  // 设置为有效指针
            *memSize = 1024;
            return HCCL_SUCCESS;
        }));

    // Mock ConstructPayLoadWqe返回成功
    MOCKER(TransportIbverbs::ConstructPayLoadWqe)
        .stubs()
        .with(any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    // 创建TransportIbverbs对象
    TransportIbverbs transportIbverbs(dispatcher, notifyPool, machinePara, timeout);

    Stream stream;
    const void *src = inputMem.ptr();
    u64 len = 1024;
    u64 dstOffset = 0;
    UserMemType dstMemType = UserMemType::OUTPUT_MEM;
    WqeType wqeType = WqeType::WQE_TYPE_DATA;
    WrAuxInfo aux = {0};
    std::vector<WqeInfo> wqeInfoVec;

    // 期望返回HCCL_SUCCESS
    HcclResult ret = transportIbverbs.TxPayLoad(dstMemType, dstOffset, src, len, wqeType, aux, wqeInfoVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}