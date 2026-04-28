/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <sys/stat.h>
#include "comm_manager.h"

using namespace Hccl;

class CommManagerStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(CommManagerStructInitTest, Ut_StatStructInit_Expect_ZeroInitialized)
{
    struct stat fileStat = {};
    EXPECT_EQ(fileStat.st_dev, 0);
    EXPECT_EQ(fileStat.st_ino, 0);
    EXPECT_EQ(fileStat.st_nlink, 0);
    EXPECT_EQ(fileStat.st_mode, 0);
    EXPECT_EQ(fileStat.st_uid, 0);
    EXPECT_EQ(fileStat.st_gid, 0);
    EXPECT_EQ(fileStat.st_rdev, 0);
    EXPECT_EQ(fileStat.st_size, 0);
}

TEST_F(CommManagerStructInitTest, Ut_StatStructBraceInit_Expect_AllFieldsZero)
{
    struct stat fileStat = {};
    EXPECT_EQ(fileStat.st_atim.tv_sec, 0);
    EXPECT_EQ(fileStat.st_mtim.tv_sec, 0);
    EXPECT_EQ(fileStat.st_ctim.tv_sec, 0);
}
