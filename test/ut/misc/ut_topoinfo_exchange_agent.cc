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
#include <mockcpp/mockcpp.hpp>
#define private public
#include "topoinfo_exchange_agent.h"
#include <iostream>
#include <sstream>
#include "externalinput_pub.h"
#include "adapter_error_manager_pub.h"
#include "config.h"
#include "sal_pub.h"
#include "device_capacity.h"
#include "preempt_port_manager.h"

#define protected public
#define TEST_RANK_SIZE 4

using namespace std;
using namespace hccl;

class TopoExchangeAgentTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TopoExchangeAgentTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TopoExchangeAgentTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

static void TestConstructRankTable(RankTable_t &rankTable)
{
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(TEST_RANK_SIZE);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    rankVec[0].serverId = "192.168.0.101";

    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 1;
    rankVec[1].serverId = "192.168.0.101";

    rankVec[2].rankId = 2;
    rankVec[2].deviceInfo.devicePhyId = 0;
    rankVec[2].serverId = "192.168.0.101";

    rankVec[3].rankId = 3;
    rankVec[3].deviceInfo.devicePhyId = 1;
    rankVec[3].serverId = "192.168.0.101";

    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = TEST_RANK_SIZE;
    rankTable.serverNum = TEST_RANK_SIZE / 2;
}

TEST_F(TopoExchangeAgentTest, St_VerifyClusterTlsConsistency_When_Consistent_True_SupportTls_True_Expect_ReturnIsHCCL_SUCCESS)
{
    HcclIpAddress localIp(1694542016);
    HcclNetDevCtx netDevCtx;
    HcclBasicRankInfo localRankInfo;
    localRankInfo.deviceType = DevType::DEV_TYPE_910_93;
    u32 serverPort = 60000;
    string identifier = "test";
    RankTable_t clusterInfo;
    TestConstructRankTable(clusterInfo);
    clusterInfo.rankList[0].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[1].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[2].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[3].tlsStatus = TlsStatus::ENABLE;
    TopoInfoExchangeAgent agent(localIp, serverPort, identifier, netDevCtx, localRankInfo);
    HcclResult ret = agent.VerifyClusterTlsConsistency(clusterInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TopoExchangeAgentTest, St_VerifyClusterTlsConsistency_When_Consistent_False_SupportTls_True_Expect_ReturnIsHCCL_E_PARA_01)
{
    HcclIpAddress localIp(1694542016);
    HcclNetDevCtx netDevCtx;
    HcclBasicRankInfo localRankInfo;
    localRankInfo.deviceType = DevType::DEV_TYPE_910_93;
    u32 serverPort = 60000;
    string identifier = "test";
    RankTable_t clusterInfo;
    TestConstructRankTable(clusterInfo);
    clusterInfo.rankList[0].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[1].tlsStatus = TlsStatus::DISABLE;
    clusterInfo.rankList[2].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[3].tlsStatus = TlsStatus::DISABLE;
    TopoInfoExchangeAgent agent(localIp, serverPort, identifier, netDevCtx, localRankInfo);
    HcclResult ret = agent.VerifyClusterTlsConsistency(clusterInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TopoExchangeAgentTest, St_VerifyClusterTlsConsistency_When_Consistent_False_SupportTls_True_Expect_ReturnIsHCCL_E_PARA_02)
{
    HcclIpAddress localIp(1694542016);
    HcclNetDevCtx netDevCtx;
    HcclBasicRankInfo localRankInfo;
    localRankInfo.deviceType = DevType::DEV_TYPE_910_93;
    u32 serverPort = 60000;
    string identifier = "test";
    RankTable_t clusterInfo;
    TestConstructRankTable(clusterInfo);
    clusterInfo.rankList[0].tlsStatus = TlsStatus::DISABLE;
    clusterInfo.rankList[1].tlsStatus = TlsStatus::DISABLE;
    clusterInfo.rankList[2].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[3].tlsStatus = TlsStatus::DISABLE;
    TopoInfoExchangeAgent agent(localIp, serverPort, identifier, netDevCtx, localRankInfo);
    HcclResult ret = agent.VerifyClusterTlsConsistency(clusterInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TopoExchangeAgentTest, St_VerifyClusterTlsConsistency_When_Consistent_True_SupportTls_False_Expect_ReturnIsHCCL_SUCCESS)
{
    HcclIpAddress localIp(1694542016);
    HcclNetDevCtx netDevCtx;
    HcclBasicRankInfo localRankInfo;
    localRankInfo.deviceType = DevType::DEV_TYPE_910_93;
    u32 serverPort = 60000;
    string identifier = "test";
    RankTable_t clusterInfo;
    TestConstructRankTable(clusterInfo);
    clusterInfo.rankList[0].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[1].tlsStatus = TlsStatus::UNKNOWN;
    clusterInfo.rankList[2].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[3].tlsStatus = TlsStatus::UNKNOWN;
    TopoInfoExchangeAgent agent(localIp, serverPort, identifier, netDevCtx, localRankInfo);
    HcclResult ret = agent.VerifyClusterTlsConsistency(clusterInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TopoExchangeAgentTest, St_VerifyClusterTlsConsistency_When_Consistent_False_SupportTls_False_Expect_ReturnIsHCCL_E_PARA)
{
    HcclIpAddress localIp(1694542016);
    HcclNetDevCtx netDevCtx;
    HcclBasicRankInfo localRankInfo;
    localRankInfo.deviceType = DevType::DEV_TYPE_910_93;
    u32 serverPort = 60000;
    string identifier = "test";
    RankTable_t clusterInfo;
    TestConstructRankTable(clusterInfo);
    clusterInfo.rankList[0].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[1].tlsStatus = TlsStatus::DISABLE;
    clusterInfo.rankList[2].tlsStatus = TlsStatus::ENABLE;
    clusterInfo.rankList[3].tlsStatus = TlsStatus::UNKNOWN;
    TopoInfoExchangeAgent agent(localIp, serverPort, identifier, netDevCtx, localRankInfo);
    HcclResult ret = agent.VerifyClusterTlsConsistency(clusterInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);
}