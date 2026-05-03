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
#include "transport_device_ibverbs_pub.h"
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

class TransportDeviceIbverbs_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportDeviceIbverbs_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportDeviceIbverbs_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TransportDeviceIbverbs_UT, TransportDeviceIbverbs_Constructor)
{
    DispatcherPub dispatcherTemp(s32(0));
    std::unique_ptr<NotifyPool> notifyPoolTemp;
    notifyPoolTemp.reset(new (std::nothrow) NotifyPool());
    MachinePara machineParaTemp;
    machineParaTemp.deviceLogicId = 0;
    TransportDeviceIbverbsData transDevIbverbsData;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(3000);
    
    std::shared_ptr<TransportDeviceIbverbs> ibverbs = std::make_shared<TransportDeviceIbverbs>(
        &dispatcherTemp, notifyPoolTemp, machineParaTemp, timeout, transDevIbverbsData);
    EXPECT_NE(ibverbs, nullptr);
}
