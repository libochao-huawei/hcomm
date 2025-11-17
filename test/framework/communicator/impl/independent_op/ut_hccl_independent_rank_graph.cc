/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "hccl_api.h"
#include "hccl_api_base_test.h"
#include "hccl_tbe_task.h"
 
constexpr const char* RANKTABLE_FILE_NAME = "./ut_independent_op_test.json";
class HcclIndependentOpRankGraphTest : public BaseInit {
public:
    void SetUp() override {
        MOCKER(HcclTbeTaskInit)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        UT_COMM_CREATE_DEFAULT(comm);
    }
    void TearDown() override {
        Ut_Comm_Destroy(comm);
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclIndependentOpRankGraphTest, Ut_CommGetLinks_When_CommIsNull_Expect_PtrError)
{
    HcclResult ret = CommGetLinks(nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}