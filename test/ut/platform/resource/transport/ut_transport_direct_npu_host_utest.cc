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

#include "transport_direct_npu_pub.h"
#include "mem_host_pub.h"
#include "sal.h"
#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportDirectNpuHost_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportDirectNpuHost_UT SetUpTestCase--\033[0m" << std::endl;
    }
    
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportDirectNpuHost_UT TearDownTestCase--\033[0m" << std::endl;
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
        
        timeout = std::chrono::milliseconds(1000);
        
        std::cout << "TransportDirectNpuHost_UT SetUp" << std::endl;
    }
    
    virtual void TearDown()
    {
        delete dispatcher;
        std::cout << "TransportDirectNpuHost_UT TearDown" << std::endl;
        GlobalMockObject::verify();
    }
    
    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
};

// 测试GetMemInfo函数 - 空指针检查（第90-91行修改）
TEST_F(TransportDirectNpuHost_UT, GetMemInfo_null_dstMemPtr)
{
    TransportDirectNpu transDirectNpu(dispatcher, notifyPool, machinePara, timeout);
    
    void **dstMemPtr = nullptr;
    u64 dstMemSize = 0;
    HcclResult ret = transDirectNpu.GetMemInfo(UserMemType::INPUT_MEM, dstMemPtr, &dstMemSize);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TransportDirectNpuHost_UT, GetMemInfo_null_dstMemSize)
{
    TransportDirectNpu transDirectNpu(dispatcher, notifyPool, machinePara, timeout);
    
    void *dstMemPtr = nullptr;
    u64 *dstMemSizePtr = nullptr;
    HcclResult ret = transDirectNpu.GetMemInfo(UserMemType::INPUT_MEM, &dstMemPtr, dstMemSizePtr);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TransportDirectNpuHost_UT, GetMemInfo_both_null)
{
    TransportDirectNpu transDirectNpu(dispatcher, notifyPool, machinePara, timeout);
    
    void **dstMemPtr = nullptr;
    u64 *dstMemSizePtr = nullptr;
    HcclResult ret = transDirectNpu.GetMemInfo(UserMemType::INPUT_MEM, dstMemPtr, dstMemSizePtr);
    
    EXPECT_EQ(ret, HCCL_E_PTR);
}