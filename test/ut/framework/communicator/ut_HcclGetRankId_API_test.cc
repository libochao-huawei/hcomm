/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"

class HcclGetRankIdTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclGetRankIdTest, Ut_HcclGetRankId_WhenCommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    Ut_Device_Set(0);

    uint32_t rankId = 0;

    HcclResult ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetRankIdTest, Ut_HcclGetRankId_WhenRankIdIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    int devId = 0;
    int rankId = 0;

    Ut_Comm_Create(comm, devId, rankTableFileName, rankId);
    uint32_t *prankId = nullptr;

    HcclResult ret = HcclGetRankId(comm, prankId);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclGetRankIdTest, Ut_HcclGetRankId_When_1Server2Rank_Expect_ReturnHCCL_SUCCESS)
{
    int devId = 0;
    int _rankId = 0;

    Ut_Comm_Create(comm, devId, rankTableFileName, _rankId);
    uint32_t rankId = 0;

    HcclResult ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(_rankId, rankId);

    Ut_Comm_Destroy(comm);
}