/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"
#include "hccl_group_utils.h"

class HcclOpInfoTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
        MOCKER(GetExternalInputHcclEnableEntryLog).stubs().with(any()).will(returnValue(true));
    }

    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclOpInfoTest, Ut_HcclOpInfo_DefaultConstructor_Expect_AllMembersInitialized)
{
    hcclOpInfo info;

    EXPECT_EQ(info.coll, static_cast<HcclCMDType>(0));
    EXPECT_EQ(info.sendbuff, nullptr);
    EXPECT_EQ(info.recvbuff, nullptr);
    EXPECT_EQ(info.sendCount, 0);
    EXPECT_EQ(info.recvCount, 0);
    EXPECT_EQ(info.sendCounts, nullptr);
    EXPECT_EQ(info.recvCounts, nullptr);
    EXPECT_EQ(info.sdispls, nullptr);
    EXPECT_EQ(info.rdispls, nullptr);
    EXPECT_EQ(info.sendType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.recvType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.op, static_cast<HcclReduceOp>(0));
    EXPECT_EQ(info.root, 0);
    EXPECT_EQ(info.comm, nullptr);
    EXPECT_EQ(info.stream, nullptr);
}

TEST_F(HcclOpInfoTest, Ut_HcclOpInfo_BraceInitialization_Expect_SameAsDefaultConstructor)
{
    hcclOpInfo info = {};

    EXPECT_EQ(info.coll, static_cast<HcclCMDType>(0));
    EXPECT_EQ(info.sendbuff, nullptr);
    EXPECT_EQ(info.recvbuff, nullptr);
    EXPECT_EQ(info.sendCount, 0);
    EXPECT_EQ(info.recvCount, 0);
    EXPECT_EQ(info.sendCounts, nullptr);
    EXPECT_EQ(info.recvCounts, nullptr);
    EXPECT_EQ(info.sdispls, nullptr);
    EXPECT_EQ(info.rdispls, nullptr);
    EXPECT_EQ(info.sendType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.recvType, static_cast<HcclDataType>(0));
    EXPECT_EQ(info.op, static_cast<HcclReduceOp>(0));
    EXPECT_EQ(info.root, 0);
    EXPECT_EQ(info.comm, nullptr);
    EXPECT_EQ(info.stream, nullptr);
}

TEST_F(HcclOpInfoTest, Ut_HcclOpInfo_SetFields_Expect_FieldsSetCorrectly)
{
    hcclOpInfo info;

    info.coll = HcclCMDType::HCCL_CMD_BROADCAST;
    info.sendbuff = reinterpret_cast<const void *>(0x1000);
    info.recvbuff = reinterpret_cast<void *>(0x2000);
    info.sendCount = 100;
    info.recvCount = 100;
    info.root = 1;

    EXPECT_EQ(info.coll, HcclCMDType::HCCL_CMD_BROADCAST);
    EXPECT_EQ(info.sendbuff, reinterpret_cast<const void *>(0x1000));
    EXPECT_EQ(info.recvbuff, reinterpret_cast<void *>(0x2000));
    EXPECT_EQ(info.sendCount, 100);
    EXPECT_EQ(info.recvCount, 100);
    EXPECT_EQ(info.root, 1);
}