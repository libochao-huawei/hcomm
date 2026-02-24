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
#include "hccl_api.h"
#include "log.h"
#include "hccl_comm_pub.h"
#include "hcomm_primitives.h"
#include <string>
#include "mockcpp/mockcpp.hpp"

#define private public

using namespace hccl;

class HcommBatchModeTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        UT_COMM_CREATE_DEFAULT(comm);
    }
    void TearDown() override
    {
        Ut_Comm_Destroy(comm);
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcommBatchModeTest, Ut_HcommBatchMode_When_TagNotExist_Expect_ERROR)
{
    int32_t ret1 = HcommBatchModeStart("Tag1");
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    int32_t ret2 = HcommBatchModeEnd("Tag2");
    EXPECT_EQ(ret2, HCCL_E_PARA);
}
