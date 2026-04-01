/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * This SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../ut_hcomm_base.h"
#include "coll_comm.h"

class TestCollComm : public TestHcommCAdptBase {
public:
    void SetUp() override {
        TestHcommCAdptBase::SetUp();
    }
    void TearDown() override {
        TestHcommCAdptBase::TearDown();
    }
};

TEST_F(TestCollComm, Ut_TestCollCommInit_When_RankGraphNullptr_Return_HCCL_E_PTR)
{
    hccl::CollComm collComm(nullptr, 0, "test_comm", hccl::ManagerCallbacks{});
    HcclMem cclBuffer = {};
    HcclCommConfig config = {};
    HcclResult ret = collComm.Init(nullptr, nullptr, cclBuffer, &config);
    EXPECT_EQ(ret, HCCL_E_PTR);
}
