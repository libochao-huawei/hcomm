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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include <cstring>
#include <vector>

#include "hcomm_c_adpt.h"
#include "resource_entities.h"
#include "notifys/mem_notify.h"

using namespace hccl;

class TestServiceTest : public testing::Test {
protected:
    void SetUp() override
    {
        // Create CpuThread via public API (AICPU + CPU → CpuThread)
        ThreadConfig config = {};
        config.notifyNumPerThread = kNotifyNum;
        HcclResult ret = HcommThreadAllocWithConfig(
            COMM_ENGINE_AICPU, 1, config, THREAD_TYPE_CPU, &threadHandle_);
        ASSERT_EQ(ret, HCCL_SUCCESS);
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
    }

    static constexpr uint32_t kNotifyNum = 2;
    ThreadHandle threadHandle_{};
};

static HcclResult TestService(void *args, uint64_t argsSizeByte)
{
    return HCCL_SUCCESS;
}

TEST_F(TestServiceTest, Ut_TaskServiceRegister_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER_CPP(&MemNotify::Wait)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    ThreadServiceHandle serviceHandle;
    HcclResult ret = HcommThreadServiceRegister(threadHandle_, TestService, &serviceHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcommThreadServiceUnregister(threadHandle_, serviceHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
