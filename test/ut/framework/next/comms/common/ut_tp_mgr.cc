/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You can not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public

#include "tp_mgr.h"
#include "hccp_async_ctx.h"
#include "orion_adapter_hccp.h"

#undef private
#undef protected

using namespace hcomm;

class TpMgrTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TpMgrTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        GlobalMockObject::verify();
        std::cout << "TpMgrTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TpMgrTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TpMgrTest TearDown" << std::endl;
    }
};

TEST_F(TpMgrTest, GetTpTotalTimeout_ValidAtGear_ReturnsCorrectTimeout)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 0;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 16);

    tpAttrInfo.tpAttr.at = 1;
    tpAttrInfo.tpAttr.retryTimesInit = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 128);

    tpAttrInfo.tpAttr.at = 2;
    tpAttrInfo.tpAttr.retryTimesInit = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 1000);

    tpAttrInfo.tpAttr.at = 3;
    tpAttrInfo.tpAttr.retryTimesInit = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 4000);
}

TEST_F(TpMgrTest, GetTpTotalTimeout_InvalidAtGear_UsesDefault)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 5;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 1000);
}

TEST_F(TpMgrTest, GetTpTotalTimeout_WithRetryTimes_ReturnsCorrectTimeout)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 2;
    tpAttrInfo.tpAttr.retryTimesInit = 3;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 4000);
}

TEST_F(TpMgrTest, GetTpTotalTimeout_MaxRetryTimes_ReturnsCorrectTimeout)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 3;
    tpAttrInfo.tpAttr.retryTimesInit = 7;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 32000);
}

TEST_F(TpMgrTest, CalcTaTimeout_ZeroTpTimeout_ReturnsEnvValue)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 0;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    MOCKER_CPP(&Hccl::EnvConfig::GetInstance).stubs().will(returnValue((Hccl::EnvConfig*)0x12345));
    MOCKER_CPP(&Hccl::EnvConfig::GetRdmaConfig).stubs().will(returnValue((Hccl::EnvRdmaConfig*)0x12345));
    MOCKER_CPP(&Hccl::EnvRdmaConfig::GetUbTimeOut).stubs().will(returnValue(8));

    uint8_t timeout = TpMgr::CalcTaTimeout(tpAttrInfo);
    EXPECT_EQ(timeout, 8);

    GlobalMockObject::verify();
}

TEST_F(TpMgrTest, CalcTaTimeout_TpTimeoutGreaterThanEnv_AutoUpgrade)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 3;
    tpAttrInfo.tpAttr.retryTimesInit = 3;

    MOCKER_CPP(&Hccl::EnvConfig::GetInstance).stubs().will(returnValue((Hccl::EnvConfig*)0x12345));
    MOCKER_CPP(&Hccl::EnvConfig::GetRdmaConfig).stubs().will(returnValue((Hccl::EnvRdmaConfig*)0x12345));
    MOCKER_CPP(&Hccl::EnvRdmaConfig::GetUbTimeOut).stubs().will(returnValue(0));

    uint8_t timeout = TpMgr::CalcTaTimeout(tpAttrInfo);
    EXPECT_EQ(timeout, 24);

    GlobalMockObject::verify();
}

TEST_F(TpMgrTest, CalcTaTimeout_EnvEqualToTpTimeout_ReturnsEnvValue)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 2;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    MOCKER_CPP(&Hccl::EnvConfig::GetInstance).stubs().will(returnValue((Hccl::EnvConfig*)0x12345));
    MOCKER_CPP(&Hccl::EnvConfig::GetRdmaConfig).stubs().will(returnValue((Hccl::EnvRdmaConfig*)0x12345));
    MOCKER_CPP(&Hccl::EnvRdmaConfig::GetUbTimeOut).stubs().will(returnValue(16));

    uint8_t timeout = TpMgr::CalcTaTimeout(tpAttrInfo);
    EXPECT_EQ(timeout, 16);

    GlobalMockObject::verify();
}