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

#include "transport_p2p_pub.h"
#include "mem_host_pub.h"
#include "sal.h"
#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportP2pHost_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportP2pHost_UT SetUpTestCase--\033[0m" << std::endl;
    }
    
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportP2pHost_UT TearDownTestCase--\033[0m" << std::endl;
    }
    
    virtual void SetUp()
    {
        dispatcher = new (std::nothrow) DispatcherPub(s32(1));
        notifyPool.reset(new (std::nothrow) NotifyPool());
        
        machinePara.deviceLogicId = 0;
        machinePara.localUserrank = 0;
        machinePara.remoteUserrank = 1;
        
        timeout = std::chrono::milliseconds(1000);
        
        std::cout << "TransportP2pHost_UT SetUp" << std::endl;
    }
    
    virtual void TearDown()
    {
        delete dispatcher;
        std::cout << "TransportP2pHost_UT TearDown" << std::endl;
        GlobalMockObject::verify();
    }
    
    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
};

// 测试GetRemoteMem函数 - 空指针检查（第179行修改）
TEST_F(TransportP2pHost_UT, GetRemoteMem_null_remotePtr)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    void **remotePtr = nullptr;
    HcclResult ret = transP2p.GetRemoteMem(UserMemType::INPUT_MEM, remotePtr);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试构造函数
TEST_F(TransportP2pHost_UT, constructor)
{
    TransportP2p transP2p(dispatcher, notifyPool, machinePara, timeout);
    
    EXPECT_EQ(transP2p.dispatcher_, dispatcher);
    EXPECT_NE(transP2p.notifyPool_, nullptr);
    EXPECT_EQ(transP2p.machinePara_.deviceLogicId, 0);
}