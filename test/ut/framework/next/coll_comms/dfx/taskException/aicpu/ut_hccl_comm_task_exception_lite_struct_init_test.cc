/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for the details. You may not use this file exception with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "hcclCommTaskException.h"

class HcclCommTaskExceptionLiteStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommTaskExceptionLiteStructInitTest, Ut_EventSummaryStructInit_Expect_ZeroInitialized)
{
    struct event_summary event = {};
    EXPECT_EQ(event.dst_engine, TS_CPU);
    EXPECT_EQ(event.policy, ONLY);
    EXPECT_EQ(event.pid, 0U);
}

TEST_F(HcclCommTaskExceptionLiteStructInitTest, Ut_EventSummaryBraceInit_Expect_AllFieldsZero)
{
    struct event_summary event = {};
    EXPECT_EQ(event.dst_engine, static_cast<engine_type>(0));
    EXPECT_EQ(event.policy, static_cast<event_policy>(0));
}

TEST_F(HcclCommTaskExceptionLiteStructInitTest, Ut_EventSummarySetFields_Expect_FieldsSetCorrectly)
{
    struct event_summary event = {};
    event.dst_engine = TS_CPU;
    event.policy = ONLY;
    event.pid = 1234;
    EXPECT_EQ(event.dst_engine, TS_CPU);
    EXPECT_EQ(event.policy, ONLY);
    EXPECT_EQ(event.pid, 1234U);
}
