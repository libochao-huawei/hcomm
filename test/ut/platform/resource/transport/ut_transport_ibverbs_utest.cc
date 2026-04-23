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

#include "transport_device_ibverbs.h"
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

class TransportIbverbs_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportIbverbs_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportIbverbs_UT TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        // 初始化dispatcher
        dispatcher = new (std::nothrow) DispatcherPub(s32(0));

        // 初始化notifyPool
        std::unique_ptr<NotifyPool> notifyPool;
        notifyPool.reset(new (std::nothrow) NotifyPool());
        
        // 初始化MachinePara
        machinePara.deviceLogicId = 0;
        if (deviceMem.ptr() == nullptr) {
            DeviceMem::alloc(deviceMem, devSize);
        }
        machinePara.inputMem = deviceMem;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
        delete dispatcher;
        if (deviceMem.ptr() != nullptr){
            deviceMem.free();
        }
        GlobalMockObject::verify();
    }

    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    MachinePara machinePara;
    DeviceMem deviceMem;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(3000);
    int devSize = 1024;
};

TEST_F(TransportIbverbs_UT, RegUserMem)
{
    MOCKER_CPP(&TransportIbverbs::UseMultiQp).stubs().will(returnValue(false));
    HcclUs startut = TIME_NOW();
    s32 ret = HCCL_SUCCESS;
    
    std::shared_ptr<TransportIbverbs> ibverbs = std::make_shared<TransportIbverbs>(dispatcher, notifyPool, machinePara, timeout);
    std::vector<u8> exchangeDataForSend_;
    exchangeDataForSend_.resize(devSize);
    u64 exchangeDataBlankSize = devSize;
    u8 *exchangeDataPtr = exchangeDataForSend_.data();
    ret = ibverbs->RegUserMem(MemType::USER_INPUT_MEM, exchangeDataPtr, exchangeDataBlankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}