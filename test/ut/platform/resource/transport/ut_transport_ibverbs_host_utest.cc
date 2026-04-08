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

#ifndef private
#define private public
#define protected public
#endif

#include "transport_ibverbs_pub.h"
#include "mem_host_pub.h"
#include "sal.h"
#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportIbverbsHost_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsHost_UT SetUpTestCase--\033[0m" << std::endl;
    }
    
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportIbverbsHost_UT TearDownTestCase--\033[0m" << std::endl;
    }
    
    virtual void SetUp()
    {
        // 初始化dispatcher
        dispatcher = new (std::nothrow) DispatcherPub(s32(1));
        
        // 初始化notifyPool
        notifyPool.reset(new (std::nothrow) NotifyPool());
        
        // 初始化MachinePara
        machinePara.deviceLogicId = 0;
        machinePara.localUserrank = 0;
        machinePara.remoteUserrank = 1;
        machinePara.collectiveId = "test_collective";
        
        timeout = std::chrono::milliseconds(1000);
        
        std::cout << "TransportIbverbsHost_UT SetUp" << std::endl;
    }
    
    virtual void TearDown()
    {
        delete dispatcher;
        std::cout << "TransportIbverbsHost_UT TearDown" << std::endl;
        GlobalMockObject::verify();
    }
    
    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
};

// 测试GetRemoteMem函数 - 空指针检查（第103行修改）
TEST_F(TransportIbverbsHost_UT, GetRemoteMem_null_remotePtr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    void **remotePtr = nullptr;
    HcclResult ret = transIbverbs.GetRemoteMem(UserMemType::INPUT_MEM, remotePtr);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试TxSendNotifyWqe函数 - 空指针检查（第111-112行修改）
TEST_F(TransportIbverbsHost_UT, TxSendNotifyWqe_null_memMsg_addr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    MemMsg memMsg = {};
    memMsg.addr = nullptr;
    void *srcMemPtr = malloc(100);
    u64 srcMemSize = 100;
    Stream stream;
    
    HcclResult ret = transIbverbs.TxSendNotifyWqe(memMsg, srcMemPtr, srcMemSize, stream);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(srcMemPtr);
}

TEST_F(TransportIbverbsHost_UT, TxSendNotifyWqe_null_srcMemPtr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    MemMsg memMsg = {};
    memMsg.addr = malloc(100);
    void *srcMemPtr = nullptr;
    u64 srcMemSize = 100;
    Stream stream;
    
    HcclResult ret = transIbverbs.TxSendNotifyWqe(memMsg, srcMemPtr, srcMemSize, stream);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(memMsg.addr);
}

TEST_F(TransportIbverbsHost_UT, TxSendNotifyWqe_both_null)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    MemMsg memMsg = {};
    memMsg.addr = nullptr;
    void *srcMemPtr = nullptr;
    u64 srcMemSize = 100;
    Stream stream;
    
    HcclResult ret = transIbverbs.TxSendNotifyWqe(memMsg, srcMemPtr, srcMemSize, stream);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试RegUserMem函数 - 空指针检查（第120行修改）
TEST_F(TransportIbverbsHost_UT, RegUserMem_null_memPtr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    u8 *exchangeDataPtr = reinterpret_cast<u8*>(malloc(100));
    u64 exchangeDataBlankSize = 100;
    
    HcclResult ret = transIbverbs.RegUserMem(MemType::USER_INPUT_MEM, exchangeDataPtr, exchangeDataBlankSize);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(exchangeDataPtr);
}

// 测试RegCustomUserMemWithMsg函数 - 空指针检查（第128-129行修改）
TEST_F(TransportIbverbsHost_UT, RegCustomUserMemWithMsg_null_addr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    u8 *exchangeDataPtr = reinterpret_cast<u8*>(malloc(100));
    u64 exchangeDataBlankSize = 100;
    void *addr = nullptr;
    u64 size = 100;
    MemMsg memMsg = {};
    
    HcclResult ret = transIbverbs.RegCustomUserMemWithMsg(addr, size, memMsg, exchangeDataPtr, exchangeDataBlankSize);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(exchangeDataPtr);
}

TEST_F(TransportIbverbsHost_UT, RegCustomUserMemWithMsg_null_exchangeDataPtr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    u8 *exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 100;
    void *addr = malloc(100);
    u64 size = 100;
    MemMsg memMsg = {};
    
    HcclResult ret = transIbverbs.RegCustomUserMemWithMsg(addr, size, memMsg, exchangeDataPtr, exchangeDataBlankSize);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(addr);
}

TEST_F(TransportIbverbsHost_UT, RegCustomUserMemWithMsg_both_null)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    u8 *exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 100;
    void *addr = nullptr;
    u64 size = 100;
    MemMsg memMsg = {};
    
    HcclResult ret = transIbverbs.RegCustomUserMemWithMsg(addr, size, memMsg, exchangeDataPtr, exchangeDataBlankSize);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试GetIndOpRemoteAddr函数 - 空指针检查（第137行修改）
TEST_F(TransportIbverbsHost_UT, GetIndOpRemoteAddr_null_exchangeDataPtr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    u8 *exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 100;
    
    HcclResult ret = transIbverbs.GetIndOpRemoteAddr(exchangeDataPtr, exchangeDataBlankSize);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试GetRemoteNotifyAddr函数 - 空指针检查（第145行修改）
TEST_F(TransportIbverbsHost_UT, GetRemoteNotifyAddr_null_exchangeDataPtr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    u8 *exchangeDataPtr = nullptr;
    u64 exchangeDataBlankSize = 100;
    MemMsg memMsg = {};
    
    HcclResult ret = transIbverbs.GetRemoteNotifyAddr(exchangeDataPtr, exchangeDataBlankSize, memMsg);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试WriteCommon函数 - 空指针检查（第166-167行修改）
TEST_F(TransportIbverbsHost_UT, WriteCommon_null_remoteAddr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    void *remoteAddr = nullptr;
    void *localAddr = malloc(100);
    u64 length = 100;
    Stream stream;
    WqeType wqeType = WqeType::WQE_TYPE_DATA;
    WrAuxInfo aux = {};
    
    HcclResult ret = transIbverbs.WriteCommon(remoteAddr, localAddr, length, stream, wqeType, aux);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(localAddr);
}

TEST_F(TransportIbverbsHost_UT, WriteCommon_null_localAddr)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    void *remoteAddr = malloc(100);
    void *localAddr = nullptr;
    u64 length = 100;
    Stream stream;
    WqeType wqeType = WqeType::WQE_TYPE_DATA;
    WrAuxInfo aux = {};
    
    HcclResult ret = transIbverbs.WriteCommon(remoteAddr, localAddr, length, stream, wqeType, aux);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(remoteAddr);
}

TEST_F(TransportIbverbsHost_UT, WriteCommon_both_null)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    void *remoteAddr = nullptr;
    void *localAddr = nullptr;
    u64 length = 100;
    Stream stream;
    WqeType wqeType = WqeType::WQE_TYPE_DATA;
    WrAuxInfo aux = {};
    
    HcclResult ret = transIbverbs.WriteCommon(remoteAddr, localAddr, length, stream, wqeType, aux);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试GetLocalRdmaNotify函数 - 智能指针检查修复（第154行和第158行修改）
TEST_F(TransportIbverbsHost_UT, GetLocalRdmaNotify_success)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    // Mock notify对象 - 使用reset方式设置
    transIbverbs.ackNotify_.reset(new (std::nothrow) LocalIpcNotify());
    transIbverbs.dataNotify_.reset(new (std::nothrow) LocalIpcNotify());
    transIbverbs.dataAckNotify_.reset(new (std::nothrow) LocalIpcNotify());
    
    std::vector<HcclSignalInfo> rdmaNotify;
    HcclResult ret = transIbverbs.GetLocalRdmaNotify(rdmaNotify);
    
    // 验证智能指针非空
    EXPECT_NE(transIbverbs.ackNotify_, nullptr);
    EXPECT_NE(transIbverbs.dataNotify_, nullptr);
    EXPECT_NE(transIbverbs.dataAckNotify_, nullptr);
    
    // 由于GetNotifyData是成员函数，无法使用MOCKER
    // 这里主要测试智能指针检查逻辑是否正确
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_GE(rdmaNotify.size(), 3);
}

// 测试构造函数
TEST_F(TransportIbverbsHost_UT, constructor)
{
    TransportIbverbs transIbverbs(dispatcher, notifyPool, machinePara, timeout);
    
    EXPECT_EQ(transIbverbs.dispatcher_, dispatcher);
    EXPECT_NE(transIbverbs.notifyPool_, nullptr);
    EXPECT_EQ(transIbverbs.machinePara_.deviceLogicId, 0);
}