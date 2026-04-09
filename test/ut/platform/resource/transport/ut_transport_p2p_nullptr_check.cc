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

// 测试 RxAsync 函数中 localSendReadyNotify_ 为 nullptr 的情况
// 这将覆盖空指针检查代码：if (localSendReadyNotify_ != nullptr && localSendReadyNotify_->ptr() != nullptr)
TEST_F(TransportP2pNullPtrCheckTest, RxAsync_LocalSendReadyNotifyIsNull_SkipSignalWait)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置 localSendReadyNotify_ 为 nullptr
    transP2p.localSendReadyNotify_ = nullptr;
    
    // 设置 linkAttribute 不支持目的端发起，避免进入数据传输分支
    machinePara.linkAttribute = 0x1;  // 清除 0x2 标志位

    Stream stream;
    u8 dstData[256];
    
    // 当 localSendReadyNotify_ 为 nullptr 时，应该跳过 SignalWait 调用
    HcclResult ret = transP2p.RxAsync(UserMemType::INPUT_MEM, 0, dstData, sizeof(dstData), stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 RxAsync 函数中 localSendReadyNotify_->ptr() 为 nullptr 的情况
TEST_F(TransportP2pNullPtrCheckTest, RxAsync_LocalSendReadyNotifyPtrIsNull_SkipSignalWait)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 创建一个 LocalIpcNotify 对象但不初始化 ptr
    transP2p.localSendReadyNotify_ = std::make_shared<LocalIpcNotify>();
    // ptr() 将返回 nullptr
    
    // 设置 linkAttribute 不支持目的端发起，避免进入数据传输分支
    machinePara.linkAttribute = 0x1;

    Stream stream;
    u8 dstData[256];
    
    // 当 localSendReadyNotify_->ptr() 为 nullptr 时，应该跳过 SignalWait 调用
    HcclResult ret = transP2p.RxAsync(UserMemType::INPUT_MEM, 0, dstData, sizeof(dstData), stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 TxAsync 函数中 remoteSendReadyNotify_ 为 nullptr 的情况
TEST_F(TransportP2pNullPtrCheckTest, TxAsync_RemoteSendReadyNotifyIsNull_SkipSignalRecord)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置 remoteSendReadyNotify_ 为 nullptr
    transP2p.remoteSendReadyNotify_ = nullptr;
    
    // 设置 src 为 nullptr，避免进入数据传输分支
    // 这样可以直接测试 SignalRecord 被跳过的逻辑
    machinePara.linkAttribute = 0x1;

    Stream stream;
    
    // 当 remoteSendReadyNotify_ 为 nullptr 时，应该跳过 SignalRecord 调用
    HcclResult ret = transP2p.TxAsync(UserMemType::INPUT_MEM, 0, nullptr, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 TxAsync 函数中 remoteSendReadyNotify_->ptr() 为 nullptr 的情况
TEST_F(TransportP2pNullPtrCheckTest, TxAsync_RemoteSendReadyNotifyPtrIsNull_SkipSignalRecord)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 创建一个 RemoteNotify 对象但不初始化 ptr
    transP2p.remoteSendReadyNotify_ = std::make_shared<RemoteNotify>();
    // ptr() 将返回 nullptr
    
    // 设置 src 为 nullptr，避免进入数据传输分支
    machinePara.linkAttribute = 0x1;

    Stream stream;
    
    // 当 remoteSendReadyNotify_->ptr() 为 nullptr 时，应该跳过 SignalRecord 调用
    HcclResult ret = transP2p.TxAsync(UserMemType::INPUT_MEM, 0, nullptr, 0, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试 TxAsync 单个版本中 GetRemoteMem 返回 nullptr 的情况
TEST_F(TransportP2pNullPtrCheckTest, TxAsync_GetRemoteMemReturnsNull_ReturnError)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置 linkAttribute 不支持目的端发起，进入数据传输分支
    machinePara.linkAttribute = 0x1;

    Stream stream;
    u8 srcData[256];
    
    // 设置 remote memory 为 nullptr，GetRemoteMem 会返回 nullptr
    transP2p.remoteInputPtr_ = nullptr;
    transP2p.remoteOutputPtr_ = nullptr;
    
    // 当 GetRemoteMem 返回 nullptr 时，CHK_PTR_NULL 应该返回错误
    HcclResult ret = transP2p.TxAsync(UserMemType::INPUT_MEM, 0, srcData, sizeof(srcData), stream);
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试 TxAsync vector 版本中 GetRemoteMem 返回 nullptr 的情况
TEST_F(TransportP2pNullPtrCheckTest, TxAsyncVector_GetRemoteMemReturnsNull_ReturnError)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置 linkAttribute 不支持目的端发起，进入数据传输分支
    machinePara.linkAttribute = 0x1;

    Stream stream;
    u8 srcData[256];
    u8 dstData[256];
    
    std::vector<TxMemoryInfo> txMems;
    TxMemoryInfo memInfo;
    memInfo.src = srcData;
    memInfo.dstMemType = UserMemType::INPUT_MEM;
    memInfo.dstOffset = 0;
    memInfo.len = sizeof(srcData);
    txMems.push_back(memInfo);
    
    // 设置 remote memory 为 nullptr
    transP2p.remoteInputPtr_ = nullptr;
    transP2p.remoteOutputPtr_ = nullptr;
    
    // 当 GetRemoteMem 返回 nullptr 时，CHK_PTR_NULL 应该返回错误
    HcclResult ret = transP2p.TxAsync(txMems, stream);
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试 RxAsync 单个版本中 GetRemoteMem 返回 nullptr 的情况
TEST_F(TransportP2pNullPtrCheckTest, RxAsync_GetRemoteMemReturnsNull_ReturnError)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置 linkAttribute 支持目的端发起，进入数据传输分支
    machinePara.linkAttribute = 0x2;

    Stream stream;
    u8 dstData[256];
    
    // 设置 localSendReadyNotify_ 为 nullptr，跳过 SignalWait
    transP2p.localSendReadyNotify_ = nullptr;
    
    // 设置 remote memory 为 nullptr，这样 GetRemoteMem 会返回 nullptr
    // 然后 RxAsync 中的 CHK_PTR_NULL(srcMemPtr) 会检查到并返回错误
    transP2p.remoteInputPtr_ = nullptr;
    transP2p.remoteOutputPtr_ = nullptr;
    
    // Mock HcclD2DMemcpyAsync 避免实际调用
    MOCKER(HcclD2DMemcpyAsync)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    // 当 GetRemoteMem 返回 nullptr 时，CHK_PTR_NULL 应该返回错误
    HcclResult ret = transP2p.RxAsync(UserMemType::INPUT_MEM, 0, dstData, sizeof(dstData), stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
    
    GlobalMockObject::verify();
}

// 测试 RxAsync vector 版本中 GetRemoteMem 返回 nullptr 的情况
TEST_F(TransportP2pNullPtrCheckTest, RxAsyncVector_GetRemoteMemReturnsNull_ReturnError)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    // 设置 linkAttribute 支持目的端发起，进入数据传输分支
    machinePara.linkAttribute = 0x2;

    Stream stream;
    u8 srcData[256];
    u8 dstData[256];
    
    std::vector<RxMemoryInfo> rxMems;
    RxMemoryInfo memInfo;
    memInfo.dst = dstData;
    memInfo.srcMemType = UserMemType::INPUT_MEM;
    memInfo.srcOffset = 0;
    memInfo.len = sizeof(dstData);
    rxMems.push_back(memInfo);
    
    // 设置 localSendReadyNotify_ 为 nullptr，跳过 SignalWait
    transP2p.localSendReadyNotify_ = nullptr;
    
    // 设置 remote memory 为 nullptr
    transP2p.remoteInputPtr_ = nullptr;
    transP2p.remoteOutputPtr_ = nullptr;
    
    // 当 GetRemoteMem 返回 nullptr 时，CHK_PTR_NULL 应该返回错误
    HcclResult ret = transP2p.RxAsync(rxMems, stream);
    EXPECT_NE(ret, HCCL_SUCCESS);
}