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
#include <mockcpp/mokc.h>

#define private public
#define protected public
#include "hccl_group.h"
#include "hccl_group_utils.h"
#undef protected
#undef private

using namespace hccl;

class HcclGroupTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "HcclGroupTest SetUp" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "HcclGroupTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in HcclGroupTest SetUp" << std::endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        std::cout << "A Test case in HcclGroupTest TearDown" << std::endl;
    }
};

TEST_F(HcclGroupTest, Ut_HcclGroupStart_When_Call_Expect_Return_Success)
{
    HcclResult ret = HcclGroupStart();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    EXPECT_EQ(1, hcclGroupDepth);
}

TEST_F(HcclGroupTest, Ut_HcclGroupEnd_When_Call_Expect_Return_Success)
{
    HcclGroupStart();
    HcclResult ret = HcclGroupEnd();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    EXPECT_EQ(0, hcclGroupDepth);
}
