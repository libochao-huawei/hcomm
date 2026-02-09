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

class ReproTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();

        // 生成 ranktable
        UT_USE_1SERVER_1RANK_AS_DEFAULT;

        // 将enableEntryLog默认返回为true
        MOCKER(GetExternalInputHcclEnableEntryLog)
            .stubs()
            .with(any())
            .will(returnValue(true));
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(ReproTest, UT_1)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}

TEST_F(ReproTest, UT_2)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}

TEST_F(ReproTest, UT_3)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}

TEST_F(ReproTest, UT_4)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}

TEST_F(ReproTest, UT_5)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}

TEST_F(ReproTest, UT_6)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}

TEST_F(ReproTest, UT_7)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}

TEST_F(ReproTest, UT_8)
{
    Ut_Comm_Create(comm, 0, rankTableFileName, 0);

    Ut_Comm_Destroy(comm);
}
