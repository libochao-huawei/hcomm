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
#include "tp_manager.h"
#include "hccp.h"
#include "orion_adapter_hccp.h"
#include "orion_adapter_rts.h"
#include "internal_exception.h"
#include "env_config/env_config.h"

using namespace Hccl;

namespace {

void MockDeviceTpAttrAsyncSupport()
{
    u32 tpAttrVersion = 2U;
    MOCKER(RaGetInterfaceVersion)
        .stubs()
        .with(any(), any(), outBoundP(&tpAttrVersion, sizeof(tpAttrVersion)))
        .will(returnValue(0));
    MOCKER(HrtRaSetTpAttrAsync).stubs().will(returnValue(HCCL_SUCCESS));
}

} // namespace

class TpManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TpManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TpManagerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        MOCKER(HrtGetDevicePhyIdByIndex).defaults().will(returnValue(static_cast<DevId>(0)));
        void *rdmaHandle = (void*)0x200;
        MOCKER(HrtRaUbCtxInit).stubs().with(any(), any()).will(returnValue(rdmaHandle));
        MOCKER(RaGetInterfaceVersion).defaults().will(returnValue(static_cast<s32>(-1)));
        TpManager::GetInstance(0).Init();
        std::cout << "A Test case in TpManagerTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TpManagerTest TearDown" << std::endl;
    }
};

TEST_F(TpManagerTest, tp_manager_get_infos_success)
{
    HcclResult result;
    int32_t devLogicId = 0;

    IpAddress locAddr("3.0.0.1");
    IpAddress rmtAddr("3.0.0.2");
    TpProtocol protocol = TpProtocol::TP;
    TpInfo tpInfo;

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(TpManagerTest, tp_manager_redo_get_infos_success)
{
    HcclResult result;
    int32_t devLogicId = 0;

    IpAddress locAddr("3.0.0.1");
    IpAddress rmtAddr("3.0.0.2");
    TpProtocol protocol = TpProtocol::TP;
    TpInfo tpInfo;

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_SUCCESS);

    result = TpManager::GetInstance(devLogicId).ReleaseTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(TpManagerTest, tp_manager_get_infos_throw)
{
    ReqHandleResult sockEAgain = ReqHandleResult::SOCK_E_AGAIN;
    MOCKER(HrtRaGetAsyncReqResult).stubs().will(returnValue(sockEAgain));

    HcclResult result;
    int32_t devLogicId = 0;

    IpAddress locAddr("4.0.0.1");
    IpAddress rmtAddr("4.0.0.2");
    TpProtocol protocol = TpProtocol::TP;
    TpInfo tpInfo;

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    EXPECT_THROW(TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo), InternalException);
}

TEST_F(TpManagerTest, tp_manager_get_infos_not_found)
{
    uint32_t errNum = 0;
    RequestHandle reqHandle = 0x12345678;
    MOCKER(RaUbGetTpInfoAsync).stubs()
        .with(any(), any(), any(), outBound(errNum))
        .will(returnValue(reqHandle));
    HcclResult result;
    int32_t devLogicId = 0;

    IpAddress locAddr("5.0.0.1");
    IpAddress rmtAddr("5.0.0.2");
    TpProtocol protocol = TpProtocol::TP;
    TpInfo tpInfo;

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
}

TEST_F(TpManagerTest, tp_manager_redo_get_infos_not_found)
{   // 新版本查询失败后，下一次调用还会尝试寻找tp资源，不会直接按记录报错
    uint32_t errNum = 0;
    RequestHandle reqHandle = 0x12345678;
    MOCKER(RaUbGetTpInfoAsync).stubs()
        .with(any(), any(), any(), outBound(errNum))
        .will(returnValue(reqHandle));
    HcclResult result;
    int32_t devLogicId = 0;

    IpAddress locAddr("5.0.0.1");
    IpAddress rmtAddr("5.0.0.2");
    TpProtocol protocol = TpProtocol::TP;
    TpInfo tpInfo;

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_NOT_FOUND);
}

TEST_F(TpManagerTest, Ut_ReleaseTpInfo_When_InputValue_Expect_Return_HCCL_SUCCESS)
{
    HcclResult result;
    int32_t devLogicId = 1;

    IpAddress locAddr("6.0.0.1");
    IpAddress rmtAddr("6.0.0.2");
    TpProtocol protocol = TpProtocol::TP;
    TpInfo tpInfo;

    result = TpManager::GetInstance(devLogicId).ReleaseTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_NOT_FOUND);

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    TpInfo fakeTpInfo;
    result = TpManager::GetInstance(devLogicId).ReleaseTpInfo({locAddr, rmtAddr, protocol}, fakeTpInfo);
    EXPECT_EQ(result, HCCL_E_NOT_FOUND);

    result = TpManager::GetInstance(devLogicId).GetTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_SUCCESS);

    result = TpManager::GetInstance(devLogicId).ReleaseTpInfo({locAddr, rmtAddr, protocol}, tpInfo);
    EXPECT_EQ(result, HCCL_SUCCESS);
}

TEST_F(TpManagerTest, Ut_TaHwValueToMs_AllGears_ReturnsCorrectTimeout)
{
    EXPECT_EQ(TpManager::TaHwValueToMs(0), 512);
    EXPECT_EQ(TpManager::TaHwValueToMs(8), 1000);
    EXPECT_EQ(TpManager::TaHwValueToMs(16), 8000);
    EXPECT_EQ(TpManager::TaHwValueToMs(24), 32000);

    EXPECT_EQ(TpManager::TaHwValueToMs(7), 512);
    EXPECT_EQ(TpManager::TaHwValueToMs(15), 1000);
    EXPECT_EQ(TpManager::TaHwValueToMs(23), 8000);
    EXPECT_EQ(TpManager::TaHwValueToMs(31), 32000);
}

TEST_F(TpManagerTest, Ut_TaHwValueToMs_InvalidGear_ReturnsDefault)
{
    EXPECT_EQ(TpManager::TaHwValueToMs(32), 8000);
    EXPECT_EQ(TpManager::TaHwValueToMs(255), 8000);
}

TEST_F(TpManagerTest, Ut_FindMinTaHwValue_AllTimeouts_ReturnsCorrectHwValue)
{
    EXPECT_EQ(TpManager::FindMinTaHwValue(100), 0);
    EXPECT_EQ(TpManager::FindMinTaHwValue(700), 8);
    EXPECT_EQ(TpManager::FindMinTaHwValue(5000), 16);
    EXPECT_EQ(TpManager::FindMinTaHwValue(20000), 24);
}

TEST_F(TpManagerTest, Ut_FindMinTaHwValue_BoundaryValues_ReturnsCorrectHwValue)
{
    EXPECT_EQ(TpManager::FindMinTaHwValue(512), 8);
    EXPECT_EQ(TpManager::FindMinTaHwValue(1000), 16);
    EXPECT_EQ(TpManager::FindMinTaHwValue(8000), 24);
}

TEST_F(TpManagerTest, Ut_GetTpTotalTimeout_ValidAtGear_ReturnsCorrectTimeout)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 0;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpManager::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 16);

    tpAttrInfo.tpAttr.at = 1;
    tpAttrInfo.tpAttr.retryTimesInit = 0;
    EXPECT_EQ(TpManager::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 128);

    tpAttrInfo.tpAttr.at = 2;
    tpAttrInfo.tpAttr.retryTimesInit = 0;
    EXPECT_EQ(TpManager::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 1000);

    tpAttrInfo.tpAttr.at = 3;
    tpAttrInfo.tpAttr.retryTimesInit = 0;
    EXPECT_EQ(TpManager::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 4000);
}

TEST_F(TpManagerTest, Ut_GetTpTotalTimeout_InvalidAtGear_UsesDefault)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 5;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpManager::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 1000);
}

TEST_F(TpManagerTest, Ut_GetTpTotalTimeout_WithRetryTimes_ReturnsCorrectTimeout)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 2;
    tpAttrInfo.tpAttr.retryTimesInit = 3;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpManager::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 4000);
}

TEST_F(TpManagerTest, Ut_GetTpTotalTimeout_MaxRetryTimes_ReturnsCorrectTimeout)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 3;
    tpAttrInfo.tpAttr.retryTimesInit = 7;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpManager::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 32000);
}

TEST_F(TpManagerTest, tp_manager_device_qos_sl_mapping_success)
{
    MockDeviceTpAttrAsyncSupport();

    HcclResult result;
    const int32_t devLogicId = 0;
    IpAddress locAddr("8.0.0.1");
    IpAddress rmtAddr("8.0.0.2");
    const TpProtocol protocol = TpProtocol::TP;
    RaUbGetTpInfoParam param{locAddr, rmtAddr, protocol};
    param.qos = 5U;
    TpInfo tpInfo{};

    result = TpManager::GetInstance(devLogicId).GetTpInfo(param, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    result = TpManager::GetInstance(devLogicId).GetTpInfo(param, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    result = TpManager::GetInstance(devLogicId).GetTpInfo(param, tpInfo);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_TRUE(tpInfo.hasMappedJettyPriority);
}

TEST_F(TpManagerTest, tp_manager_loop_first_tp_lowest_sl_success)
{
    MockDeviceTpAttrAsyncSupport();

    HcclResult result;
    const int32_t devLogicId = 0;
    IpAddress locAddr("8.0.0.3");
    IpAddress rmtAddr("8.0.0.4");
    const TpProtocol protocol = TpProtocol::TP;
    RaUbGetTpInfoParam param{locAddr, rmtAddr, protocol};
    param.loopFirstTpLowestSl = true;
    param.qos = 2U;
    TpInfo tpInfo{};

    result = TpManager::GetInstance(devLogicId).GetTpInfo(param, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    result = TpManager::GetInstance(devLogicId).GetTpInfo(param, tpInfo);
    EXPECT_EQ(result, HCCL_E_AGAIN);

    result = TpManager::GetInstance(devLogicId).GetTpInfo(param, tpInfo);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_TRUE(tpInfo.hasMappedJettyPriority);
}

TEST_F(TpManagerTest, tp_manager_qos_cache_by_key_success)
{
    HcclResult result;
    const int32_t devLogicId = 0;
    IpAddress locAddr("8.0.0.5");
    IpAddress rmtAddr("8.0.0.6");
    const TpProtocol protocol = TpProtocol::TP;

    RaUbGetTpInfoParam paramQos0{locAddr, rmtAddr, protocol};
    paramQos0.qos = 0U;
    TpInfo tpInfo0{};

    RaUbGetTpInfoParam paramQos7{locAddr, rmtAddr, protocol};
    paramQos7.qos = 7U;
    TpInfo tpInfo7{};

    result = TpManager::GetInstance(devLogicId).GetTpInfo(paramQos0, tpInfo0);
    EXPECT_EQ(result, HCCL_E_AGAIN);
    result = TpManager::GetInstance(devLogicId).GetTpInfo(paramQos0, tpInfo0);
    EXPECT_EQ(result, HCCL_SUCCESS);

    result = TpManager::GetInstance(devLogicId).GetTpInfo(paramQos7, tpInfo7);
    EXPECT_EQ(result, HCCL_E_AGAIN);
    result = TpManager::GetInstance(devLogicId).GetTpInfo(paramQos7, tpInfo7);
    EXPECT_EQ(result, HCCL_SUCCESS);
}