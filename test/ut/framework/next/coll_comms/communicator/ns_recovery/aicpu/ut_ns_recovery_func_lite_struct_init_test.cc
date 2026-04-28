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
#include "ns_recovery_func_lite.h"

class NsRecoveryFuncLiteStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(NsRecoveryFuncLiteStructInitTest, Ut_TsdrvCtrlMsgStructInit_Expect_ZeroInitialized)
{
    struct tsdrv_ctrl_msg para = {};
    EXPECT_EQ(para.tsid, 0U);
    EXPECT_EQ(para.msg_len, 0U);
    EXPECT_EQ(para.msg, nullptr);
}

TEST_F(NsRecoveryFuncLiteStructInitTest, Ut_TsdrvCtrlMsgBraceInit_Expect_AllFieldsZero)
{
    struct tsdrv_ctrl_msg para = {};
    EXPECT_EQ(para.tsid, 0U);
    EXPECT_EQ(para.msg_len, 0U);
    EXPECT_EQ(para.msg, nullptr);
}

TEST_F(NsRecoveryFuncLiteStructInitTest, Ut_TsdrvCtrlMsgSetFields_Expect_FieldsSetCorrectly)
{
    struct tsdrv_ctrl_msg para = {};
    para.tsid = 100;
    para.msg_len = sizeof(void*);
    void* msg = reinterpret_cast<void*>(0x1000);
    para.msg = msg;
    EXPECT_EQ(para.tsid, 100U);
    EXPECT_EQ(para.msg_len, sizeof(void*));
    EXPECT_EQ(para.msg, reinterpret_cast<void*>(0x1000));
}
