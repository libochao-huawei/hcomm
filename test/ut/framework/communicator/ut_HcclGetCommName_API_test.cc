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

class HcclGetCommNameTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
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

TEST_F(HcclGetCommNameTest, Ut_HcclGetCommName_When_CommNameIsNull_Expect_ReturnIsHCCL_E_PTR)
{
}

TEST_F(HcclGetCommNameTest, Ut_HcclGetCommName_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    char *commName = new char[ROOTINFO_INDENTIFIER_MAX_LENGTH];

    HcclResult ret = HcclGetCommName(comm, commName);
    EXPECT_EQ(ret, HCCL_E_PTR);

    delete[] commName;
}

TEST_F(HcclGetCommNameTest, HcclGetCommName_When_InputNoInit_Expect_ReturnIsHCCL_E_PTR)
{
    char *commName = nullptr;

    HcclResult ret = HcclGetCommName(&comm, commName);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetCommNameTest, HcclGetCommName_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
}

HcclResult HandleHcclResPackReq(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal);
TEST_F(HcclGetCommNameTest, ut_HandleHcclResPackReq)
{
    HcclChannelDesc desc[4];
    HcclResult ret = HcclChannelDescInit(desc, 4);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    desc[0].remoteRank = 1;
    desc[0].channelProtocol = CommProtocol::COMM_PROTOCOL_ROCE;
    desc[0].notifyNum = 1;
    EXPECT_EQ(desc[0].roceAttr.queueNum, 0xFFFFFFFF);
    EXPECT_EQ(desc[0].roceAttr.retryCnt, 0xFFFFFFFF);
    EXPECT_EQ(desc[0].roceAttr.retryInterval, 0xFFFFFFFF);
    EXPECT_EQ(desc[0].roceAttr.tc, 0xFF);
    EXPECT_EQ(desc[0].roceAttr.sl, 0xFF);

    ret = HandleHcclResPackReq(desc[0], desc[1]);
    EXPECT_EQ(desc[1].roceAttr.queueNum, 1);
    EXPECT_EQ(desc[1].roceAttr.retryCnt, 7);
    EXPECT_EQ(desc[1].roceAttr.retryInterval, 20);
    EXPECT_EQ(desc[1].roceAttr.tc, 132);
    EXPECT_EQ(desc[1].roceAttr.sl, 4);

    desc[2].remoteRank = 1;
    desc[2].channelProtocol = CommProtocol::COMM_PROTOCOL_ROCE;
    desc[2].notifyNum = 1;
    desc[2].roceAttr.queueNum = 2;
    desc[2].roceAttr.retryCnt = 2;
    desc[2].roceAttr.retryInterval = 2;
    desc[2].roceAttr.tc = 2;
    desc[2].roceAttr.sl = 2;
    ret = HandleHcclResPackReq(desc[2], desc[3]);
    EXPECT_EQ(desc[3].roceAttr.queueNum, 2);
    EXPECT_EQ(desc[3].roceAttr.retryCnt, 2);
    EXPECT_EQ(desc[3].roceAttr.retryInterval, 2);
    EXPECT_EQ(desc[3].roceAttr.tc, 2);
    EXPECT_EQ(desc[3].roceAttr.sl, 2);
}