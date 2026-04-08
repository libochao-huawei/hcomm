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

#include "transport_ibverbs.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"

#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportIbverbsNullptrCheck_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsNullptrCheck_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsNullptrCheck_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        // 初始化dispatcher
        dispatcher = new (std::nothrow) DispatcherPub(0);
        ASSERT_NE(dispatcher, nullptr);

        // 初始化notifyPool
        notifyPool.reset(new (std::nothrow) NotifyPool());

        // 初始化MachinePara
        machinePara.deviceLogicId = 0;
        machinePara.localUserrank = 0;
        machinePara.remoteUserrank = 1;
        machinePara.localWorldRank = 0;
        machinePara.remoteWorldRank = 1;
        machinePara.localDeviceId = 0;
        machinePara.remoteDeviceId = 1;
        machinePara.serverId = "server_0";
        machinePara.tag = "test_tag";
        machinePara.isAicpuModeEn = false;
        machinePara.isIndOp = false;
        machinePara.notifyNum = 1;
        machinePara.nicDeploy = NICDeployment::NIC_DEPLOYMENT_HOST;
        machinePara.deviceType = DevType::DEV_TYPE_910;

        // 初始化timeout
        timeout = std::chrono::milliseconds(30000);

        // Mock基本函数
        MOCKER(hrtGetDeviceType).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetNotifySize).stubs().will(returnValue(HCCL_SUCCESS));
    }
    virtual void TearDown()
    {
        delete dispatcher;
        GlobalMockObject::verify();
    }

    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
};

// Test GetRemoteMem with valid remotePtr
TEST_F(TransportIbverbsNullptrCheck_UT, GetRemoteMem_valid_remotePtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    // Initialize remote memory pointers
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_INPUT_MEM)].addr = (void*)0x1000;
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_INPUT_MEM)].len = 1024;
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_OUTPUT_MEM)].addr = (void*)0x2000;
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_OUTPUT_MEM)].len = 2048;
    
    void *remotePtr = nullptr;
    HcclResult ret = transport.GetRemoteMem(UserMemType::INPUT_MEM, &remotePtr);
    
    // Should succeed and return valid pointer
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(remotePtr, (void*)0x1000);
}

// Test TxSendNotifyWqe with nullptr memMsg.addr
TEST_F(TransportIbverbsNullptrCheck_UT, TxSendNotifyWqe_nullptr_memMsg_addr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    MemMsg memMsg;
    memMsg.addr = nullptr;
    memMsg.len = 1024;
    const void *srcMemPtr = (void*)0x3000;
    u64 srcMemSize = 512;
    Stream stream;
    
    HcclResult ret = transport.TxSendNotifyWqe(memMsg, srcMemPtr, srcMemSize, stream);
    
    // Should return HCCL_E_PTR due to nullptr check on memMsg.addr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test TxSendNotifyWqe with nullptr srcMemPtr
TEST_F(TransportIbverbsNullptrCheck_UT, TxSendNotifyWqe_nullptr_srcMemPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    MemMsg memMsg;
    memMsg.addr = (void*)0x1000;
    memMsg.len = 1024;
    const void *srcMemPtr = nullptr;
    u64 srcMemSize = 512;
    Stream stream;
    
    HcclResult ret = transport.TxSendNotifyWqe(memMsg, srcMemPtr, srcMemSize, stream);
    
    // Should return HCCL_E_PTR due to nullptr check on srcMemPtr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test TxSendNotifyWqe with valid pointers
TEST_F(TransportIbverbsNullptrCheck_UT, TxSendNotifyWqe_valid_pointers)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    // Initialize required members
    transport.combineQpHandles_.push_back(CombineQpHandle());
    transport.combineQpHandles_[0].qpHandle = (QpHandle)0x5000;
    
    MemMsg memMsg;
    memMsg.addr = (void*)0x1000;
    memMsg.len = 1024;
    const void *srcMemPtr = (void*)0x3000;
    u64 srcMemSize = 512;
    Stream stream;
    
    // Mock required functions
    MOCKER(hrtRaGetQpAttr).stubs().will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = transport.TxSendNotifyWqe(memMsg, srcMemPtr, srcMemSize, stream);
    
    // Note: May still fail due to other requirements, but nullptr checks should pass
    // The important thing is that it doesn't return HCCL_E_PTR from nullptr checks
    EXPECT_NE(ret, HCCL_E_PTR);
}

// Test RegUserMem with nullptr memPtr
TEST_F(TransportIbverbsNullptrCheck_UT, RegUserMem_nullptr_memPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    MemType memType = MemType::USER_INPUT_MEM;
    void *memPtr = nullptr;
    u64 memSize = 1024;
    u8* exchangeDataPtr = new u8[2048];
    u64 exchangeDataBlankSize = 2048;
    
    // Set up input memory pointer to nullptr
    machinePara.inputMem = DeviceMem(memPtr, memSize);
    
    HcclResult ret = transport.RegUserMem(memType, exchangeDataPtr, exchangeDataBlankSize);
    
    // Should return HCCL_E_PTR due to nullptr check on memPtr
    EXPECT_EQ(ret, HCCL_E_PTR);
    
    delete[] exchangeDataPtr;
}

// Test RegUserMem with valid memPtr
TEST_F(TransportIbverbsNullptrCheck_UT, RegUserMem_valid_memPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    MemType memType = MemType::USER_INPUT_MEM;
    void *memPtr = (void*)0x1000;
    u64 memSize = 1024;
    u8* exchangeDataPtr = new u8[2048];
    u64 exchangeDataBlankSize = 2048;
    
    // Set up input memory pointer
    machinePara.inputMem = DeviceMem(memPtr, memSize);
    
    // Mock required functions
    MOCKER(HrtRaMrReg).stubs().will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = transport.RegUserMem(memType, exchangeDataPtr, exchangeDataBlankSize);
    
    // Note: May still fail due to other requirements, but nullptr check should pass
    EXPECT_NE(ret, HCCL_E_PTR);
    
    delete[] exchangeDataPtr;
}

// Test RegCustomUserMemWithMsg with nullptr addr
TEST_F(TransportIbverbsNullptrCheck_UT, RegCustomUserMemWithMsg_nullptr_addr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    void *addr = nullptr;
    u64 size = 1024;
    MemMsg memMsg;
    memMsg.addr = (void*)0x1000;
    memMsg.len = 2048;
    u8 *exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 0;
    
    HcclResult ret = transport.RegCustomUserMemWithMsg(addr, size, memMsg, exchangeDataPtr, exchangeDataBlankSize);
    
    // Should return HCCL_E_PTR due to nullptr check on addr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RegCustomUserMemWithMsg with nullptr exchangeDataPtr
TEST_F(TransportIbverbsNullptrCheck_UT, RegCustomUserMemWithMsg_nullptr_exchangeDataPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    void *addr = (void*)0x1000;
    u64 size = 1024;
    MemMsg memMsg;
    memMsg.addr = (void*)0x2000;
    memMsg.len = 2048;
    u8 *exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 0;
    
    HcclResult ret = transport.RegCustomUserMemWithMsg(addr, size, memMsg, exchangeDataPtr, exchangeDataBlankSize);
    
    // Should return HCCL_E_PTR due to nullptr check on exchangeDataPtr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RegCustomUserMemWithMsg with valid pointers
TEST_F(TransportIbverbsNullptrCheck_UT, RegCustomUserMemWithMsg_valid_pointers)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    void *addr = (void*)0x1000;
    u64 size = 1024;
    MemMsg memMsg;
    memMsg.addr = (void*)0x2000;
    memMsg.len = 2048;
    u8 *exchangeDataPtr = new u8[4096];
    u64 exchangeDataBlankSize = 4096;
    
    // Mock required functions
    MOCKER(HrtRaMrReg).stubs().will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = transport.RegCustomUserMemWithMsg(addr, size, memMsg, exchangeDataPtr, exchangeDataBlankSize);
    
    // Note: May still fail due to other requirements, but nullptr checks should pass
    EXPECT_NE(ret, HCCL_E_PTR);
    
    delete[] exchangeDataPtr;
}

// Test GetIndOpRemoteAddr with nullptr exchangeDataPtr
TEST_F(TransportIbverbsNullptrCheck_UT, GetIndOpRemoteAddr_nullptr_exchangeDataPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    machinePara.isIndOp = true;
    transport.notifyNum_ = 1;
    
    u8* exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 0;
    
    HcclResult ret = transport.GetIndOpRemoteAddr(exchangeDataPtr, exchangeDataBlankSize);
    
    // Should return HCCL_E_PTR due to nullptr check on exchangeDataPtr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test GetIndOpRemoteAddr with valid exchangeDataPtr
TEST_F(TransportIbverbsNullptrCheck_UT, GetIndOpRemoteAddr_valid_exchangeDataPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    machinePara.isIndOp = true;
    transport.notifyNum_ = 1;
    
    u8* exchangeDataPtr = new u8[4096];
    u64 exchangeDataBlankSize = 4096;
    
    // Set up valid data
    u32 remoteDmemNum = 2;
    memcpy_s(exchangeDataPtr, sizeof(u32), &remoteDmemNum, sizeof(u32));
    
    HcclResult ret = transport.GetIndOpRemoteAddr(exchangeDataPtr, exchangeDataBlankSize);
    
    // Note: May still fail due to other requirements, but nullptr check should pass
    EXPECT_NE(ret, HCCL_E_PTR);
    
    delete[] exchangeDataPtr;
}

// Test GetRemoteNotifyAddr with nullptr exchangeDataPtr
TEST_F(TransportIbverbsNullptrCheck_UT, GetRemoteNotifyAddr_nullptr_exchangeDataPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    u8* exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 0;
    MemMsg memMsg;
    memMsg.addr = (void*)0x1000;
    memMsg.len = 1024;
    
    HcclResult ret = transport.GetRemoteNotifyAddr(exchangeDataPtr, exchangeDataBlankSize, memMsg);
    
    // Should return HCCL_E_PTR due to nullptr check on exchangeDataPtr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test GetRemoteNotifyAddr with valid exchangeDataPtr
TEST_F(TransportIbverbsNullptrCheck_UT, GetRemoteNotifyAddr_valid_exchangeDataPtr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    u8* exchangeDataPtr = new u8[4096];
    u64 exchangeDataBlankSize = 4096;
    MemMsg memMsg;
    memMsg.addr = (void*)0x1000;
    memMsg.len = 1024;
    
    // Set up valid data
    MemMsg remoteMemMsg;
    remoteMemMsg.addr = (void*)0x2000;
    remoteMemMsg.len = 2048;
    memcpy_s(exchangeDataPtr, sizeof(MemMsg), &remoteMemMsg, sizeof(MemMsg));
    
    HcclResult ret = transport.GetRemoteNotifyAddr(exchangeDataPtr, exchangeDataBlankSize, memMsg);
    
    // Note: May still fail due to other requirements, but nullptr check should pass
    EXPECT_NE(ret, HCCL_E_PTR);
    
    delete[] exchangeDataPtr;
}

// Test GetRemoteMem with different UserMemType values
TEST_F(TransportIbverbsNullptrCheck_UT, GetRemoteMem_all_memTypes)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    // Initialize remote memory pointers for all types
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_INPUT_MEM)].addr = (void*)0x1000;
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_INPUT_MEM)].len = 1024;
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_OUTPUT_MEM)].addr = (void*)0x2000;
    transport.remoteMemMsg_[static_cast<u32>(MemType::USER_OUTPUT_MEM)].len = 2048;
    
    std::vector<UserMemType> memTypes = {
        UserMemType::INPUT_MEM,
        UserMemType::OUTPUT_MEM
    };
    
    for (auto memType : memTypes) {
        void *remotePtr = nullptr;
        HcclResult ret = transport.GetRemoteMem(memType, &remotePtr);
        
        EXPECT_EQ(ret, HCCL_SUCCESS);
        EXPECT_NE(remotePtr, nullptr);
    }
}

// Test WriteCommon with nullptr remoteAddr
TEST_F(TransportIbverbsNullptrCheck_UT, WriteCommon_nullptr_remoteAddr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    void *remoteAddr = nullptr;
    void *localAddr = (void*)0x1000;
    u64 length = 1024;
    Stream stream;
    WqeType wqeType = WqeType::WQE_TYPE_DATA;
    struct WrAuxInfo aux = {0};
    
    HcclResult ret = transport.WriteCommon(remoteAddr, localAddr, length, stream, wqeType, aux);
    
    // Should return HCCL_E_PTR due to nullptr check on remoteAddr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test WriteCommon with nullptr localAddr
TEST_F(TransportIbverbsNullptrCheck_UT, WriteCommon_nullptr_localAddr)
{
    TransportIbverbs transport(dispatcher, notifyPool, machinePara, timeout);
    
    void *remoteAddr = (void*)0x2000;
    void *localAddr = nullptr;
    u64 length = 1024;
    Stream stream;
    WqeType wqeType = WqeType::WQE_TYPE_DATA;
    struct WrAuxInfo aux = {0};
    
    HcclResult ret = transport.WriteCommon(remoteAddr, localAddr, length, stream, wqeType, aux);
    
    // Should return HCCL_E_PTR due to nullptr check on localAddr
    EXPECT_EQ(ret, HCCL_E_PTR);
}