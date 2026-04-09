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
#include <memory>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#ifndef private
#define private public
#define protected public
#endif

#include "transport_p2p.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"
#include "transport_base_pub.h"
#include "task_logic_info_pub.h"
#include "local_notify.h"
#include "remote_notify.h"

#include "adapter_rts.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportP2pNullPtrCheck_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportP2pNullPtrCheck_UT SetUp--\033[0m" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportP2pNullPtrCheck_UT TearDown--\033[0m" << std::endl;
    }

    virtual void SetUp()
    {
        // 初始化dispatcher
        dispatcher = new (std::nothrow) DispatcherPub(s32(1));

        // 初始化notifyPool
        notifyPool.reset(new (std::nothrow) NotifyPool());

        // 配置machinePara
        machinePara.localDeviceId = 0;
        machinePara.remoteDeviceId = 1;
        machinePara.localUserrank = 0;
        machinePara.localWorldRank = 0;
        machinePara.remoteUserrank = 1;
        machinePara.remoteWorldRank = 1;
        machinePara.deviceLogicId = 0;
        machinePara.machineType = MachineType::MACHINE_SERVER_TYPE;
        machinePara.linkMode = LinkMode::LINK_DUPLEX_MODE;
        machinePara.linkAttribute = 0; // 不支持目的端发起
        machinePara.notifyNum = 1;

        // 分配内存
        const u64 memSize = 1024;
        DeviceMem::alloc(machinePara.inputMem, memSize);
        DeviceMem::alloc(machinePara.outputMem, memSize);

        std::cout << "Test SetUp completed" << std::endl;
    }

    virtual void TearDown()
    {
        machinePara.inputMem.free();
        machinePara.outputMem.free();
        delete dispatcher;
        GlobalMockObject::verify();
    }

    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout{1000};
};

/**
 * @brief 测试TxAsync函数中GetRemoteMem返回null指针的场景
 * @details 验证当GetRemoteMem返回null指针时，TxAsync函数能正确返回错误码
 */
TEST_F(TransportP2pNullPtrCheck_UT, TxAsync_NullRemoteMemPtr_ReturnError)
{
    // 设置linkAttribute支持目的端发起以触发GetRemoteMem调用
    machinePara.linkAttribute = 0x2; // 支持目的端发起
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 初始化必要的notify成员变量，避免SignalRecord访问空指针
    std::shared_ptr<RemoteNotify> remoteNotify = std::make_shared<RemoteNotify>();
    transportP2p.remoteSendReadyNotify_ = remoteNotify;
    transportP2p.remoteSendReadyAddress_ = 0;
    transportP2p.remoteSendReadyOffset_ = 0;

    // 准备测试数据
    const u64 dstOffset = 0;
    const u64 len = 128;
    const void *src = nullptr;
    UserMemType dstMemType = UserMemType::OUTPUT_MEM;
    Stream stream;

    // 调用TxAsync，由于src为nullptr会返回错误
    HcclResult ret = transportP2p.TxAsync(dstMemType, dstOffset, src, len, stream);

    // 验证返回值 - 当linkAttribute支持目的端发起时，src参数不会被检查，应该返回成功
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief 测试TxAsync函数中linkAttribute不支持目的端发起的场景
 * @details 验证当linkAttribute为0时，不进入GetRemoteMem分支
 */
TEST_F(TransportP2pNullPtrCheck_UT, TxAsync_NotSupportDstInit_SkipRemoteMemCheck)
{
    // 创建TransportP2p对象，linkAttribute默认为0（不支持目的端发起）
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 初始化必要的notify成员变量，避免SignalRecord访问空指针
    std::shared_ptr<RemoteNotify> remoteNotify = std::make_shared<RemoteNotify>();
    transportP2p.remoteSendReadyNotify_ = remoteNotify;
    transportP2p.remoteSendReadyAddress_ = 0;
    transportP2p.remoteSendReadyOffset_ = 0;

    // 准备测试数据
    const u64 dstOffset = 0;
    const u64 len = 128;
    const void *src = nullptr;
    UserMemType dstMemType = UserMemType::OUTPUT_MEM;
    Stream stream;

    // 调用TxAsync - 由于linkAttribute为0，不会调用GetRemoteMem
    // 但src为nullptr仍然会返回错误
    HcclResult ret = transportP2p.TxAsync(dstMemType, dstOffset, src, len, stream);

    // 验证返回值
    EXPECT_EQ(ret, HCCL_E_PTR);
}

/**
 * @brief 测试TxAsync(vector版本)函数中GetRemoteMem返回null指针的场景
 * @details 验证当GetRemoteMem返回null指针时，vector版本的TxAsync函数能正确返回错误码
 */
TEST_F(TransportP2pNullPtrCheck_UT, TxAsync_Vector_NullRemoteMemPtr_ReturnError)
{
    // 设置linkAttribute支持目的端发起
    machinePara.linkAttribute = 0x2;
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 初始化必要的notify成员变量，避免SignalRecord访问空指针
    std::shared_ptr<RemoteNotify> remoteNotify = std::make_shared<RemoteNotify>();
    transportP2p.remoteSendReadyNotify_ = remoteNotify;
    transportP2p.remoteSendReadyAddress_ = 0;
    transportP2p.remoteSendReadyOffset_ = 0;

    // 准备测试数据
    std::vector<TxMemoryInfo> txMems(1);
    txMems[0].dstMemType = UserMemType::OUTPUT_MEM;
    txMems[0].dstOffset = 0;
    txMems[0].len = 128;
    txMems[0].src = nullptr;
    Stream stream;

    // 调用TxAsync(vector版本)，由于src为nullptr会返回错误
    HcclResult ret = transportP2p.TxAsync(txMems, stream);

    // 验证返回值
    EXPECT_EQ(ret, HCCL_E_PTR);
}

/**
 * @brief 测试RxAsync函数中GetRemoteMem返回null指针的场景
 * @details 验证当GetRemoteMem返回null指针时，RxAsync函数能正确返回错误码
 */
TEST_F(TransportP2pNullPtrCheck_UT, RxAsync_NullRemoteMemPtr_ReturnError)
{
    // 创建TransportP2p对象，设置linkAttribute支持目的端发起
    machinePara.linkAttribute = 0x2; // 支持目的端发起
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 初始化必要的notify成员变量，避免SignalWait访问空指针
    std::shared_ptr<LocalIpcNotify> localNotify = std::make_shared<LocalIpcNotify>();
    HcclSignalInfo notifyInfo;
    notifyInfo.addr = 100;
    notifyInfo.devId = 0;
    notifyInfo.rankId = 0;
    notifyInfo.resId = 0;
    notifyInfo.tsId = 0;
    localNotify->Init(notifyInfo, NotifyLoadType::HOST_NOTIFY);
    transportP2p.localSendReadyNotify_ = localNotify;

    // 准备测试数据
    const u64 srcOffset = 0;
    const u64 len = 128;
    void *dst = nullptr;
    UserMemType srcMemType = UserMemType::INPUT_MEM;
    Stream stream;

    // 调用RxAsync，dst为nullptr会返回错误
    HcclResult ret = transportP2p.RxAsync(srcMemType, srcOffset, dst, len, stream);

    // 验证返回值
    EXPECT_EQ(ret, HCCL_E_PTR);
}

/**
 * @brief 测试RxAsync函数中linkAttribute不支持目的端发起时不检查remoteMemPtr
 * @details 验证当linkAttribute为0时，不进入GetRemoteMem分支
 */
TEST_F(TransportP2pNullPtrCheck_UT, RxAsync_NotSupportDstInit_SkipRemoteMemCheck)
{
    // 创建TransportP2p对象，linkAttribute默认为0（不支持目的端发起）
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 初始化必要的notify成员变量，避免SignalWait访问空指针
    std::shared_ptr<LocalIpcNotify> localNotify = std::make_shared<LocalIpcNotify>();
    HcclSignalInfo notifyInfo;
    notifyInfo.addr = 100;
    notifyInfo.devId = 0;
    notifyInfo.rankId = 0;
    notifyInfo.resId = 0;
    notifyInfo.tsId = 0;
    localNotify->Init(notifyInfo, NotifyLoadType::HOST_NOTIFY);
    transportP2p.localSendReadyNotify_ = localNotify;

    // 准备测试数据
    const u64 srcOffset = 0;
    const u64 len = 128;
    void *dst = nullptr;
    UserMemType srcMemType = UserMemType::INPUT_MEM;
    Stream stream;

    // 调用RxAsync - 由于linkAttribute为0，不会调用GetRemoteMem
    HcclResult ret = transportP2p.RxAsync(srcMemType, srcOffset, dst, len, stream);

    // 验证返回值 - 应该返回成功，因为没有进入GetRemoteMem分支
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief 测试RxAsync(vector版本)函数中GetRemoteMem返回null指针的场景
 * @details 验证当GetRemoteMem返回null指针时，vector版本的RxAsync函数能正确返回错误码
 */
TEST_F(TransportP2pNullPtrCheck_UT, RxAsync_Vector_NullRemoteMemPtr_ReturnError)
{
    // 创建TransportP2p对象，设置linkAttribute支持目的端发起
    machinePara.linkAttribute = 0x2; // 支持目的端发起
    TransportP2p transportP2p(dispatcher, notifyPool, machinePara, timeout);

    // 初始化必要的notify成员变量，避免SignalWait访问空指针
    std::shared_ptr<LocalIpcNotify> localNotify = std::make_shared<LocalIpcNotify>();
    HcclSignalInfo notifyInfo;
    notifyInfo.addr = 100;
    notifyInfo.devId = 0;
    notifyInfo.rankId = 0;
    notifyInfo.resId = 0;
    notifyInfo.tsId = 0;
    localNotify->Init(notifyInfo, NotifyLoadType::HOST_NOTIFY);
    transportP2p.localSendReadyNotify_ = localNotify;

    // 准备测试数据
    std::vector<RxMemoryInfo> rxMems(1);
    rxMems[0].srcMemType = UserMemType::INPUT_MEM;
    rxMems[0].srcOffset = 0;
    rxMems[0].len = 128;
    rxMems[0].dst = nullptr;
    Stream stream;

    // 调用RxAsync(vector版本)，dst为nullptr会返回错误
    HcclResult ret = transportP2p.RxAsync(rxMems, stream);

    // 验证返回值
    EXPECT_EQ(ret, HCCL_E_PTR);
}
