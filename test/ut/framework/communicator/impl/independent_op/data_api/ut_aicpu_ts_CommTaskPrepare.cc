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
#include "dispatcher_aicpu.h"
#undef private

using namespace hccl;

class UtAicpuTsCommTaskPrepare : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().will(returnValue(HCCL_SUCCESS));
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsCommTaskPrepare, Ut_CommTaskPrepare_When_KeyIsNullAndKeyLenIsZero_Expect_ReturnHCCL_SUCCESS)
{
    char keyBuffer[] = "test_key";
    res = CommTaskPrepare(nullptr, 0);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsCommTaskPrepare, Ut_CommTaskPrepare_When_KeyIsValid_Expect_ReturnHCCL_SUCCESS)
{
    char keyBuffer[] = "test_key";
    uint32_t keyLen = strlen(keyBuffer);
    res = CommTaskPrepare(keyBuffer, keyLen);
    EXPECT_EQ(res, HCCL_SUCCESS);
}
