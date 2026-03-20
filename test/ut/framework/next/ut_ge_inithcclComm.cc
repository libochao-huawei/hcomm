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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "hccl/hcom.h"
#include "hcom_common.h"

class GEInitHcclCommTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "GEInitHcclCommTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "GEInitHcclCommTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in GEInitHcclCommTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in GEInitHcclCommTest TearDown" << std::endl;
    }
};

TEST_F(GEInitHcclCommTest, ut_HcomInitByString_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    std::string path = "/path";
    std::string identify = "0"

    HcclResult ret = HcomInitByFile(path.c_str(), identify.c_str());
}