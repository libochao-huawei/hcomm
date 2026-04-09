/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
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

#define private public
#define protected public

#include "transport_p2p_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"
#include "remote_notify.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

#include "adapter_rts.h"

using namespace std;
using namespace hccl;

class TransportP2pNullPtrCheckTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportP2pNullPtrCheckTest SetUp--\033[0m" << std::endl;
        
        // 初始化notify
        std::vector<u8> data(NOTIFY_INFO_LENGTH, 0);
        
        localNotify.reset(new (std::nothrow) LocalIpcNotify());
        HcclResult ret = localNotify->Init(0, 0);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = localNotify->Serialize(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        
        remoteNotify.reset(new (std::nothrow) RemoteNotify());
        ret = remoteNotify->Init(data);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        ret = remoteNotify->Open();
        EXPECT_EQ(ret, HCCL_SUCCESS);
        
        // 初始化dispatcher
        HcclResult retInit = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (retInit != HCCL_SUCCESS || dispatcherPtr == nullptr) {
            std::cout << "Failed to init dispatcher" << std::endl;
            return;
        }
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
    }
    
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--TransportP2pNullPtrCheckTest TearDown--\033[0m" << std::endl;
    }
    
    virtual void SetUp()
    {
        // 初始化notifyPool
        notifyPool.reset(new (std::nothrow) NotifyPool());

        // 初始化machinePara
        machinePara.deviceLogicId = 0;
        machinePara.localDeviceId = 0;
        machinePara.remoteDeviceId = 1;
        machinePara.localUserrank = 0;
        machinePara.localWorldRank = 0;
        machinePara.remoteUserrank = 1;
        machinePara.remoteWorldRank = 1;
        machinePara.linkAttribute = 0x1;
        machinePara.machineType = MachineType::MACHINE_CLIENT_TYPE;
        machinePara.tag = "test_transport";
        machinePara.notifyNum = 0;
        machinePara.supportDataReceivedAck = true;

        // 分配input和output内存
        inputMem = DeviceMem::alloc(1024);
        machinePara.inputMem = inputMem;

        outputMem = DeviceMem::alloc(1024);
        machinePara.outputMem = outputMem;

        std::cout << "A Test SetUP" << std::endl;
    }
    
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
        inputMem.free();
        outputMem.free();
        GlobalMockObject::verify();
    }

    static std::shared_ptr<LocalIpcNotify> localNotify;
    static std::shared_ptr<RemoteNotify> remoteNotify;
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
    
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    DeviceMem inputMem;
    DeviceMem outputMem;
    std::chrono::milliseconds timeout{1000};
};

std::shared_ptr<LocalIpcNotify> TransportP2pNullPtrCheckTest::localNotify = nullptr;
std::shared_ptr<RemoteNotify> TransportP2pNullPtrCheckTest::remoteNotify = nullptr;
HcclDispatcher TransportP2pNullPtrCheckTest::dispatcherPtr = nullptr;
DispatcherPub *TransportP2pNullPtrCheckTest::dispatcher = nullptr;

// 测试TxAsync函数中remoteSendReadyNotify_为nullptr的情况
// 这将覆盖新增的空指针检查代码：if (remoteSendReadyNotify_ != nullptr && remoteSendReadyNotify_->ptr() != nullptr)
TEST_F(TransportP2pNullPtrCheckTest, TxAsync_NullRemoteSendReadyNotify_Success)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置remoteSendReadyNotify_为nullptr
    transP2p.remoteSendReadyNotify_ = nullptr;
    transP2p.remoteSendReadyAddress_ = 0x1000;
    transP2p.remoteSendReadyOffset_ = 0;

    // 设置有效的远程内存指针
    void* validPtr = reinterpret_cast<void*>(0x2000);
    transP2p.remoteOutputPtr_ = validPtr;
    transP2p.remoteOutputSize_ = 4096;

    Stream stream;
    u8 srcData[256];
    // 当remoteSendReadyNotify_为nullptr时，应该跳过SignalRecord调用
    HcclResult ret = transP2p.TxAsync(UserMemType::INPUT_MEM, 0, srcData, sizeof(srcData), stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试RxAsync函数中localSendReadyNotify_为nullptr的情况
// 这将覆盖新增的空指针检查代码：if (localSendReadyNotify_ != nullptr && localSendReadyNotify_->ptr() != nullptr)
TEST_F(TransportP2pNullPtrCheckTest, RxAsync_NullLocalSendReadyNotify_Success)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置localSendReadyNotify_为nullptr
    transP2p.localSendReadyNotify_ = nullptr;

    // 设置有效的远程内存指针
    void* validPtr = reinterpret_cast<void*>(0x2000);
    transP2p.remoteInputPtr_ = validPtr;
    transP2p.remoteInputSize_ = 4096;

    Stream stream;
    u8 dstData[256];
    // 当localSendReadyNotify_为nullptr时，应该跳过SignalWait调用
    HcclResult ret = transP2p.RxAsync(UserMemType::INPUT_MEM, 0, dstData, sizeof(dstData), stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试TxAsync(vector)函数中remoteSendReadyNotify_为nullptr的情况
TEST_F(TransportP2pNullPtrCheckTest, TxAsync_Vector_NullRemoteSendReadyNotify_Success)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置remoteSendReadyNotify_为nullptr
    transP2p.remoteSendReadyNotify_ = nullptr;
    transP2p.remoteSendReadyAddress_ = 0x1000;
    transP2p.remoteSendReadyOffset_ = 0;

    // 设置有效的远程内存指针
    void* validPtr = reinterpret_cast<void*>(0x2000);
    transP2p.remoteOutputPtr_ = validPtr;
    transP2p.remoteOutputSize_ = 4096;

    std::vector<TxMemoryInfo> txMems;
    u8 srcData[256];
    txMems.emplace_back(TxMemoryInfo{UserMemType::INPUT_MEM, 0, srcData, sizeof(srcData)});

    Stream stream;
    HcclResult ret = transP2p.TxAsync(txMems, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试RxAsync(vector)函数中localSendReadyNotify_为nullptr的情况
TEST_F(TransportP2pNullPtrCheckTest, RxAsync_Vector_NullLocalSendReadyNotify_Success)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置localSendReadyNotify_为nullptr
    transP2p.localSendReadyNotify_ = nullptr;

    // 设置有效的远程内存指针
    void* validPtr = reinterpret_cast<void*>(0x2000);
    transP2p.remoteInputPtr_ = validPtr;
    transP2p.remoteInputSize_ = 4096;

    std::vector<RxMemoryInfo> rxMems;
    u8 dstData[256];
    rxMems.emplace_back(RxMemoryInfo{UserMemType::INPUT_MEM, 0, dstData, sizeof(dstData)});

    Stream stream;
    HcclResult ret = transP2p.RxAsync(rxMems, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试TxAsync函数中GetRemoteMem返回nullptr的情况
// 这将覆盖新增的空指针检查代码：CHK_PTR_NULL(dstMemPtr)
TEST_F(TransportP2pNullPtrCheckTest, TxAsync_NullRemoteMemPtr_ExpectError)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置remoteSendReadyNotify_为nullptr以避免SignalRecord调用
    transP2p.remoteSendReadyNotify_ = nullptr;
    transP2p.remoteSendReadyAddress_ = 0x1000;
    transP2p.remoteSendReadyOffset_ = 0;

    // 设置远程内存指针为nullptr
    transP2p.remoteOutputPtr_ = nullptr;
    transP2p.remoteOutputSize_ = 4096;

    Stream stream;
    u8 srcData[256];
    // 当GetRemoteMem返回nullptr时，CHK_PTR_NULL应该检测到并返回错误
    HcclResult ret = transP2p.TxAsync(UserMemType::INPUT_MEM, 0, srcData, sizeof(srcData), stream);
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试RxAsync函数中GetRemoteMem返回nullptr的情况
// 这将覆盖新增的空指针检查代码：CHK_PTR_NULL(srcMemPtr)
TEST_F(TransportP2pNullPtrCheckTest, RxAsync_NullRemoteMemPtr_ExpectError)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置localSendReadyNotify_为nullptr以避免SignalWait调用
    transP2p.localSendReadyNotify_ = nullptr;

    // 设置远程内存指针为nullptr
    transP2p.remoteInputPtr_ = nullptr;
    transP2p.remoteInputSize_ = 4096;

    Stream stream;
    u8 dstData[256];
    // 当GetRemoteMem返回nullptr时，CHK_PTR_NULL应该检测到并返回错误
    HcclResult ret = transP2p.RxAsync(UserMemType::INPUT_MEM, 0, dstData, sizeof(dstData), stream);
    EXPECT_NE(ret, HCCL_SUCCESS);
}