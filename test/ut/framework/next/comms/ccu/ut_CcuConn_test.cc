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
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#define protected public

#include "ccu_conn.h"

#include "hccp_async.h"

#include "port.h"
#include "socket.h"
#include "ccu_jetty_.h"
#include "orion_adapter_hccp.h"
#include "orion_adapter_rts.h"
#include "rdma_handle_manager.h"
#include "internal_exception.h"
#include "hccl_types.h"

#undef private
#undef protected

using namespace Hccl;

class CcuConnTest : public testing::Test {
protected:    
    static void SetUpTestCase()
    {
        std::cout << "CcuConnTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        GlobalMockObject::verify();
        std::cout << "CcuConnTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CcuConnTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CcuConnTest TearDown" << std::endl;
    }

};

pair<unique_ptr<hcomm::CcuConnection>, vector<unique_ptr<hcomm::CcuJetty>>> MockMakeCcuConnection(hcomm::TpProtocol tpProtocol)
{
    constexpr uint64_t fakeMemAddr = 0x12345678;

    const uint32_t fakeTaJettyId = 1025;
    const uint64_t fakeSqBufVa = fakeMemAddr;
    const uint32_t fakeSqBufSize = 1024;
    const uint32_t fakeSqDepth = 4;
    const IpAddress locAddrIp{"1.1.1.1"};
    CommAddr locAddr{};
    CommAddr rmtAddr{};
    locAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    rmtAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    locAddr.addr.s_addr = 0;
    rmtAddr.addr.s_addr = 0;

    hcomm::CcuChannelInfo channelInfo;
    channelInfo.channelId = 1;
    channelInfo.dieId = 1;
    
    vector<unique_ptr<hcomm::CcuJetty>> ccuJettys;
    vector<hcomm::CcuJetty *> ccuJettyPtrs;
    for (uint32_t i = 0; i < 2; i++) {
        hcomm::CcuJettyInfo jettyInfo;
        jettyInfo.jettyCtxId = 1 + i;
        jettyInfo.taJettyId = fakeTaJettyId + i;
        jettyInfo.sqDepth = fakeSqDepth;
        jettyInfo.wqeBBStartId = 16;
        jettyInfo.sqBufVa = fakeSqBufVa + i;
        jettyInfo.sqBufSize = fakeSqBufSize + i;
        channelInfo.jettyInfos.push_back(jettyInfo);
        auto ccuJetty = make_unique<hcomm::CcuJetty>(locAddrIp, jettyInfo);
        ccuJettyPtrs.emplace_back(ccuJetty.get());
        ccuJettys.emplace_back(std::move(ccuJetty));
    }

    unique_ptr<hcomm::CcuConnection> connection;
    if (tpProtocol == hcomm::TpProtocol::CTP) {
        connection = make_unique<hcomm::CcuCtpConnection>(locAddr, rmtAddr, channelInfo, ccuJettyPtrs);
    } else {
        connection = make_unique<hcomm::CcuRtpConnection>(locAddr, rmtAddr, channelInfo, ccuJettyPtrs);
    }

    return {std::move(connection), std::move(ccuJettys)};
}

namespace hcomm {
uint32_t TaHwValueToMs(uint8_t hwValue);
uint8_t FindMinTaHwValueGreaterThan(uint32_t tpTotalTimeoutMs);
}

TEST_F(CcuConnTest, Ut_GetStatus_When_CreateAndImportJettySuccess_Expect_Return_Connected)
{
    auto resPair = MockMakeCcuConnection(hcomm::TpProtocol::RTP);
    auto connection = resPair.first.get();

    std::string testDfxMsg = "";
    HcclResult ret = connection->Describe(testDfxMsg);
    MOCKER(HrtRaGetTpAttrAsync).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
    connection->Describe(testDfxMsg);
    GlobalMockObject::verify();

    MOCKER(HrtRaGetTpAttrAsync).stubs().will(returnValue(HcclResult::HCCL_E_NOT_SUPPORT)).then(returnValue(HcclResult::HCCL_E_INTERNAL));
    ret = connection->Describe(testDfxMsg);
    EXPECT_NE(ret, HcclResult::HCCL_SUCCESS);
    ret = connection->Describe(testDfxMsg);
    EXPECT_NE(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuConnTest, Ut_TaHwValueToMs_When_InputGear0_Expect_Return512ms)
{
    uint32_t timeoutMs = hcomm::TaHwValueToMs(0);
    EXPECT_EQ(timeoutMs, 512u);
    timeoutMs = hcomm::TaHwValueToMs(7);
    EXPECT_EQ(timeoutMs, 512u);
}

TEST_F(CcuConnTest, Ut_TaHwValueToMs_When_InputGear1_Expect_Return1000ms)
{
    uint32_t timeoutMs = hcomm::TaHwValueToMs(8);
    EXPECT_EQ(timeoutMs, 1000u);
    timeoutMs = hcomm::TaHwValueToMs(15);
    EXPECT_EQ(timeoutMs, 1000u);
}

TEST_F(CcuConnTest, Ut_TaHwValueToMs_When_InputGear2_Expect_Return8000ms)
{
    uint32_t timeoutMs = hcomm::TaHwValueToMs(16);
    EXPECT_EQ(timeoutMs, 8000u);
    timeoutMs = hcomm::TaHwValueToMs(23);
    EXPECT_EQ(timeoutMs, 8000u);
}

TEST_F(CcuConnTest, Ut_TaHwValueToMs_When_InputGear3_Expect_Return32000ms)
{
    uint32_t timeoutMs = hcomm::TaHwValueToMs(24);
    EXPECT_EQ(timeoutMs, 32000u);
    timeoutMs = hcomm::TaHwValueToMs(31);
    EXPECT_EQ(timeoutMs, 32000u);
}

TEST_F(CcuConnTest, Ut_TaHwValueToMs_When_InputInvalid_Expect_ReturnDefault8000ms)
{
    uint32_t timeoutMs = hcomm::TaHwValueToMs(32);
    EXPECT_EQ(timeoutMs, 8000u);
    timeoutMs = hcomm::TaHwValueToMs(100);
    EXPECT_EQ(timeoutMs, 8000u);
}

TEST_F(CcuConnTest, Ut_FindMinTaHwValueGreaterThan_When_LessThan512ms_Expect_Return0)
{
    uint8_t hwValue = hcomm::FindMinTaHwValueGreaterThan(100);
    EXPECT_EQ(hwValue, 0u);
    hwValue = hcomm::FindMinTaHwValueGreaterThan(511);
    EXPECT_EQ(hwValue, 0u);
}

TEST_F(CcuConnTest, Ut_FindMinTaHwValueGreaterThan_When_LessThan1000ms_Expect_Return8)
{
    uint8_t hwValue = hcomm::FindMinTaHwValueGreaterThan(512);
    EXPECT_EQ(hwValue, 8u);
    hwValue = hcomm::FindMinTaHwValueGreaterThan(999);
    EXPECT_EQ(hwValue, 8u);
}

TEST_F(CcuConnTest, Ut_FindMinTaHwValueGreaterThan_When_LessThan8000ms_Expect_Return16)
{
    uint8_t hwValue = hcomm::FindMinTaHwValueGreaterThan(1000);
    EXPECT_EQ(hwValue, 16u);
    hwValue = hcomm::FindMinTaHwValueGreaterThan(7999);
    EXPECT_EQ(hwValue, 16u);
}

TEST_F(CcuConnTest, Ut_FindMinTaHwValueGreaterThan_When_GreaterOrEqual8000ms_Expect_Return24)
{
    uint8_t hwValue = hcomm::FindMinTaHwValueGreaterThan(8000);
    EXPECT_EQ(hwValue, 24u);
    hwValue = hcomm::FindMinTaHwValueGreaterThan(10000);
    EXPECT_EQ(hwValue, 24u);
}