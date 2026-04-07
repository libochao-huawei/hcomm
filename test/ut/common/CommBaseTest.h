/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#pragma once

#include <string>
#include <gtest/gtest.h>

#include "hccl/hccl_types.h"
#include "hccl/hccl.h"

using namespace hccl;

/**
 * CommBaseTest 是所有需要通信域对象的测试类的基类，提供通信域创建和销毁等公共函数。
 */
class CommBaseTest : public testing::Test
{
protected:
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }

    void InitCommByRanktable(int rankId, const char* rankTableFileName) {
        EXPECT_TRUE(rankTableFileName != nullptr);

        // 设置device
        HcclResult ret = hrtSetDevice(rankId);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        // 初始化通信域
        ret = HcclCommInitClusterInfo(rankTableFileName, rankId, &comm);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    HcclComm comm;  // 通信域对象
};
