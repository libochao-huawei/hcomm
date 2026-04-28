/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for the details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <infiniband/verbs.h>
#include "orion_adapter_hccp.h"
#include "hccp_common.h"

class OrionAdapterHccpStructInitTest : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
        GlobalMockObject::verify();
    }
};

TEST_F(OrionAdapterHccpStructInitTest, Ut_IpInfoStructInit_Expect_ZeroInitialized)
{
    struct IpInfo vnicIpInfo = {};
    EXPECT_EQ(vnicIpInfo.ipv4, 0);
    EXPECT_EQ(vnicIpInfo.prefixLen, 0);
    EXPECT_EQ(vnicIpInfo.vlanId, 0);
}

TEST_F(OrionAdapterHccpStructInitTest, Ut_MrInfoTStructInit_Expect_ZeroInitialized)
{
    struct MrInfoT mrInfo = {};
    EXPECT_EQ(mrInfo.addr, nullptr);
    EXPECT_EQ(mrInfo.size, 0U);
    EXPECT_EQ(mrInfo.access, 0U);
}

TEST_F(OrionAdapterHccpStructInitTest, Ut_SendWrStructInit_Expect_ZeroInitialized)
{
    struct SendWr wr = {};
    EXPECT_EQ(wr.op, 0);
    EXPECT_EQ(wr.dstAddr, 0U);
    EXPECT_EQ(wr.sendFlag, 0);
    EXPECT_EQ(wr.bufNum, 0U);
    EXPECT_EQ(wr.bufList, nullptr);
}

TEST_F(OrionAdapterHccpStructInitTest, Ut_SendWrRspStructInit_Expect_ZeroInitialized)
{
    struct SendWrRsp opRsp = {};
    EXPECT_EQ(opRsp.wqeTmp.sqIndex, 0U);
    EXPECT_EQ(opRsp.wqeTmp.wqeIndex, 0U);
    EXPECT_EQ(opRsp.db.dbIndex, 0U);
}

TEST_F(OrionAdapterHccpStructInitTest, Ut_WrSgeListStructInit_Expect_ZeroInitialized)
{
    struct WrSgeList sge = {};
    EXPECT_EQ(sge.addr, 0U);
    EXPECT_EQ(sge.len, 0U);
}

TEST_F(OrionAdapterHccpStructInitTest, Ut_CqAttrStructInit_Expect_ZeroInitialized)
{
    struct CqAttr attr = {};
    EXPECT_EQ(attr.qpContext, nullptr);
    EXPECT_EQ(attr.ibSendCq, nullptr);
    EXPECT_EQ(attr.ibRecvCq, nullptr);
}

TEST_F(OrionAdapterHccpStructInitTest, Ut_IbvQpInitAttrStructInit_Expect_ZeroInitialized)
{
    struct ibv_qp_init_attr ibQpAttr = {};
    EXPECT_EQ(ibQpAttr.qp_context, nullptr);
    EXPECT_EQ(ibQpAttr.send_cq, nullptr);
    EXPECT_EQ(ibQpAttr.recv_cq, nullptr);
    EXPECT_EQ(ibQpAttr.srq, nullptr);
    EXPECT_EQ(ibQpAttr.cap.max_send_wr, 0);
    EXPECT_EQ(ibQpAttr.cap.max_recv_wr, 0);
}

TEST_F(OrionAdapterHccpStructInitTest, Ut_RaInfoNetworkModeStructInit_Expect_ZeroInitialized)
{
    struct RaInfo raInfo = {};
    raInfo.mode = HrtNetworkMode::HDC;
    raInfo.phyId = 1;
    EXPECT_EQ(raInfo.mode, HrtNetworkMode::HDC);
    EXPECT_EQ(raInfo.phyId, 1U);
}
