/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "tp_manager.h"
#include "orion_adapter_rts.h"
#include "internal_exception.h"

using namespace Hccl;

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