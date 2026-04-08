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

#include "transport_roce_pub.h"
#include "mem_host_pub.h"
#include "sal.h"
#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportRoceHost_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportRoceHost_UT SetUpTestCase--\033[0m" << std::endl;
    }
    
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportRoceHost_UT TearDownTestCase--\033[0m" << std::endl;
    }
    
    virtual void SetUp()
    {
        dispatcher = reinterpret_cast<HcclDispatcher>(0x1000);
        notifyPool.reset(new (std::nothrow) NotifyPool());
        
        machinePara.deviceLogicId = 0;
        machinePara.localUserrank = 0;
        machinePara.remoteUserrank = 1;
        
        timeout = std::chrono::milliseconds(1000);
        
        std::cout << "TransportRoceHost_UT SetUp" << std::endl;
    }
    
    virtual void TearDown()
    {
        std::cout << "TransportRoceHost_UT TearDown" << std::endl;
        GlobalMockObject::verify();
    }
    
    HcclDispatcher dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
};

// 测试Init函数 - 返回值检查（第192行修改）
TEST_F(TransportRoceHost_UT, Init_hrtRaGetQpAttr_failed)
{
    HcclIpAddress selfIp;
    HcclIpAddress peerIp;
    u32 peerPort = 1234;
    u32 selfPort = 5678;
    TransportResourceInfo transportResourceInfo;
    
    TransportRoce transRoce(dispatcher, notifyPool, machinePara, timeout,
                           selfIp, peerIp, peerPort, selfPort, transportResourceInfo);
    
    // Mock必要的依赖
    QpHandle qpHandle = reinterpret_cast<QpHandle>(0x2000);
    transRoce.tagQpInfo_.qpHandle = qpHandle;
    
    // Mock hrtRaGetQpAttr返回失败
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));
    
    HcclResult ret = transRoce.Init();
    
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试RxAsync函数 - 返回值检查（第201行修改）
TEST_F(TransportRoceHost_UT, RxAsync_GenerateRecvMessage_failed)
{
    HcclIpAddress selfIp;
    HcclIpAddress peerIp;
    u32 peerPort = 1234;
    u32 selfPort = 5678;
    TransportResourceInfo transportResourceInfo;
    
    TransportRoce transRoce(dispatcher, notifyPool, machinePara, timeout,
                           selfIp, peerIp, peerPort, selfPort, transportResourceInfo);
    
    // Mock hrtGetStreamId返回成功，避免在第一个if分支失败
    MOCKER(hrtGetStreamId)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    // Mock GetSavedEnvelope返回true，确保能到达GenerateRecvMessage
    HcclEnvelopeSummary envelope;
    MOCKER(&hccl::TransportRoce::GetSavedEnvelope)
        .stubs()
        .with(outBound(envelope))
        .will(returnValue(true));
    
    // Mock GenerateRecvMessage返回失败
    HcclMessageInfo* tmpMsg;
    HcclStatus status;
    MOCKER(&hccl::TransportRoce::GenerateRecvMessage)
        .stubs()
        .with(envelope, outBound(tmpMsg), outBound(status))
        .will(returnValue(HCCL_E_INTERNAL));
    
    // Mock RecordNotifyWithReq返回成功
    HcclRequestInfo *requestNotify = nullptr;
    MOCKER(&hccl::TransportRoce::RecordNotifyWithReq)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    void *dst = malloc(100);
    Stream stream;
    HcclResult ret = transRoce.RxAsync(UserMemType::INPUT_MEM, 0, dst, 100, stream);
    
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    free(dst);
}

// 测试构造函数
TEST_F(TransportRoceHost_UT, constructor)
{
    HcclIpAddress selfIp;
    HcclIpAddress peerIp;
    u32 peerPort = 1234;
    u32 selfPort = 5678;
    TransportResourceInfo transportResourceInfo;
    
    TransportRoce transRoce(dispatcher, notifyPool, machinePara, timeout,
                           selfIp, peerIp, peerPort, selfPort, transportResourceInfo);
    
    EXPECT_NE(transRoce.notifyPool_, nullptr);
    EXPECT_EQ(transRoce.machinePara_.deviceLogicId, 0);
}