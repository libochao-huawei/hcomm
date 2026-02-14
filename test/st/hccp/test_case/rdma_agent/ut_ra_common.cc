/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <mutex>
#include <thread>
#include <vector>
#include <mockcpp/mockcpp.hpp>
#include "gtest/gtest.h"
#include "hccp.h"
using namespace std;


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include "dl_hal_function.h"

#ifdef __cplusplus
}
#endif // __cplusplus

class RdmaAgentTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--RdmaAgentTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--RdmaAgentTest TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(RdmaAgentTest, utHelloHccp)
{
    printf("hello hccp!\n");

    GlobalMockObject::verify();
}
