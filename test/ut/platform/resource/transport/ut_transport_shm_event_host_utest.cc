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

#include "transport_shm_event_pub.h"
#include "mem_host_pub.h"
#include "sal.h"
#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportShmEventHost_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportShmEventHost_UT SetUpTestCase--\033[0m" << std::endl;
    }
    
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportShmEventHost_UT TearDownTestCase--\033[0m" << std::endl;
    }
    
    virtual void SetUp()
    {
        dispatcher = reinterpret_cast<HcclDispatcher>(0x1000);
        notifyPool.reset(new (std::nothrow) NotifyPool());
        
        machinePara.deviceLogicId = 0;
        machinePara.localUserrank = 0;
        machinePara.remoteUserrank = 1;
        
        timeout = std::chrono::milliseconds(1000);
        
        std::cout << "TransportShmEventHost_UT SetUp" << std::endl;
    }
    
    virtual void TearDown()
    {
        std::cout << "TransportShmEventHost_UT TearDown" << std::endl;
        GlobalMockObject::verify();
    }
    
    HcclDispatcher dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
};

// 测试GetRemoteMem函数 - 空指针检查（第119行修改）
TEST_F(TransportShmEventHost_UT, GetRemoteMem_null_remotePtr)
{
    HcclIpAddress selfIp;
    HcclIpAddress peerIp;
    u32 peerPort = 1234;
    u32 selfPort = 5678;
    s32 deviceLogicId = 0;
    u32 role = 0;
    TransportResourceInfo transportResourceInfo;
    HcclRtContext rtCtx = nullptr;
    
    TransportShmEvent transShmEvent(dispatcher, notifyPool, machinePara, timeout,
                                   selfIp, peerIp, peerPort, selfPort, deviceLogicId, role,
                                   transportResourceInfo, rtCtx);
    
    void **remotePtr = nullptr;
    HcclResult ret = transShmEvent.GetRemoteMem(UserMemType::INPUT_MEM, remotePtr);
    
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// 测试FrontEnvelope函数 - 空指针检查（第234行修改）
TEST_F(TransportShmEventHost_UT, FrontEnvelope_null_quePtr)
{
    HcclIpAddress selfIp;
    HcclIpAddress peerIp;
    u32 peerPort = 1234;
    u32 selfPort = 5678;
    s32 deviceLogicId = 0;
    u32 role = 0;
    TransportResourceInfo transportResourceInfo;
    HcclRtContext rtCtx = nullptr;
    
    TransportShmEvent transShmEvent(dispatcher, notifyPool, machinePara, timeout,
                                   selfIp, peerIp, peerPort, selfPort, deviceLogicId, role,
                                   transportResourceInfo, rtCtx);
    
    // 设置localEnvelopeShmQue_为nullptr，这样quePtr会为nullptr
    transShmEvent.localEnvelopeShmQue_ = nullptr;
    
    HcclEnvelopePcie envelope;
    HcclResult ret = transShmEvent.FrontEnvelope(envelope);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试构造函数
TEST_F(TransportShmEventHost_UT, constructor)
{
    HcclIpAddress selfIp;
    HcclIpAddress peerIp;
    u32 peerPort = 1234;
    u32 selfPort = 5678;
    s32 deviceLogicId = 0;
    u32 role = 0;
    TransportResourceInfo transportResourceInfo;
    HcclRtContext rtCtx = nullptr;
    
    TransportShmEvent transShmEvent(dispatcher, notifyPool, machinePara, timeout,
                                   selfIp, peerIp, peerPort, selfPort, deviceLogicId, role,
                                   transportResourceInfo, rtCtx);
    
    EXPECT_NE(transShmEvent.notifyPool_, nullptr);
    EXPECT_EQ(transShmEvent.machinePara_.deviceLogicId, 0);
}