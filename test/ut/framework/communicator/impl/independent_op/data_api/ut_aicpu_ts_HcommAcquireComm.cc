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

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#undef private

using namespace hccl;

class UtAicpuTsHcommAcquireComm : public testing::Test
{
protected:
    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommAcquireComm, Ut_HcommAcquireComm_When_CommId_IsNull_Expect_ReturnHCCL_E_PTR)
{
    res = HcommAcquireComm(nullptr);
    EXPECT_EQ(res, HCCL_E_PARA);
}
