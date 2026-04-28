/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for the details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "transport_ibverbs.h"

class TransportIbverbsStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(TransportIbverbsStructInitTest, Ut_CombineQpHandleStructInit_Expect_ZeroInitialized)
{
    CombineQpHandle tmpCombineQpHandle = {};
    EXPECT_EQ(tmpCombineQpHandle.qpHandle, nullptr);
}

TEST_F(TransportIbverbsStructInitTest, Ut_CombineQpHandleBraceInit_Expect_AllFieldsZero)
{
    CombineQpHandle tmpCombineQpHandle = {};
    EXPECT_EQ(tmpCombineQpHandle.qpHandle, nullptr);
}

TEST_F(TransportIbverbsStructInitTest, Ut_CombineQpHandleSetFields_Expect_FieldsSetCorrectly)
{
    CombineQpHandle tmpCombineQpHandle = {};
    void* qpHandle = reinterpret_cast<void*>(0x1000);
    tmpCombineQpHandle.qpHandle = qpHandle;
    EXPECT_EQ(tmpCombineQpHandle.qpHandle, qpHandle);
}
