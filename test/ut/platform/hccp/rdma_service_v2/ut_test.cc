/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

extern "C" {
#include "ut_dispatch.h"
}

#include <stdio.h>
#include <mockcpp/mockcpp.hpp>
#include "gtest/gtest.h"
#include "tc_ut_rs_ub.h"

using namespace std;

class RS : public testing::Test
{
protected:
   static void SetUpTestCase()
    {
        std::cout << "\033[36m--RoCE RS SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--RoCE RS TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
	 GlobalMockObject::verify();
    }
};

TEST_M(RS, tc_rs_ub_ctx_ext_jetty_delete);
// TEST_M(RS, tc_rs_ub_ctx_ext_jetty_create);
// TEST_M(RS, tc_rs_ub_stars_va_munmap_batch);
// TEST_M(RS, tc_rs_ub_ctx_jfc_create_ext);
// TEST_M(RS, tc_rs_ub_delete_jfc_ext);
