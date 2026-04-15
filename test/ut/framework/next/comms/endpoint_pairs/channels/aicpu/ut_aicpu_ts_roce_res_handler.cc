/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "adapter_hal_pub.h"
#include "dlhns_function.h"
#include "externalinput_pub.h"
#include "hccl_dispatcher_ctx.h"
#include "workflow_pub.h"

#include "aicpu/device/aicpu_ts_roce_res_handler.h"
#include "channel_param.h"

namespace {
HcclResult StubHrtDrvGetLocalDevIDByHostDevID(u32 /*hostUdevid*/, u32 *localDevid)
{
    if (localDevid != nullptr) {
        *localDevid = 0U;
    }
    return HCCL_SUCCESS;
}

HcclResult StubHrtDrvGetPlatformInfo(uint32_t *info)
{
    if (info != nullptr) {
        *info = 1U;
    }
    return HCCL_SUCCESS;
}
} // namespace

class AicpuTsRoceResHandlerTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(AicpuTsRoceResHandlerTest, Ut_Parse_WhenBlobNull_Returns_PTR)
{
    HcommDeviceInfo dev{};
    ChannelHandle h = 0;
    EXPECT_EQ(AicpuTsRoceResHandler::Instance().Parse(nullptr, sizeof(HcommRoceChannelRes), dev, h), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceResHandlerTest, Ut_Parse_WhenBlobTooSmall_Returns_PARA)
{
    HcommDeviceInfo dev{};
    ChannelHandle h = 0;
    uint8_t oneByte = 0;
    EXPECT_EQ(AicpuTsRoceResHandler::Instance().Parse(&oneByte, 1ULL, dev, h), HCCL_E_PARA);
}

TEST_F(AicpuTsRoceResHandlerTest, Ut_Parse_WhenQpLayoutInvalid_Returns_PARA)
{
    HcommRoceChannelRes res{};
    res.qpsPerConnection = 33U;
    res.localMemCount = 0U;
    res.remoteMemCount = 0U;

    HcommDeviceInfo dev{};
    ChannelHandle h = 0;
    EXPECT_EQ(AicpuTsRoceResHandler::Instance().Parse(&res, sizeof(res), dev, h), HCCL_E_PARA);
}

TEST_F(AicpuTsRoceResHandlerTest, Ut_Destroy_WhenUnknownHandle_ReturnsFalse)
{
    EXPECT_FALSE(AicpuTsRoceResHandler::Instance().Destroy(static_cast<ChannelHandle>(0x12345678ULL)));
}

TEST_F(AicpuTsRoceResHandlerTest, Ut_Parse_ValidBlob_Returns_SUCCESS_AndDestroyCleansUp)
{
    MOCKER_CPP(&DlHnsFunction::DlHnsFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtDrvGetPlatformInfo).stubs().will(invoke(StubHrtDrvGetPlatformInfo));
    MOCKER(GetExternalInputHcclAicpuUnfold).stubs().will(returnValue(true));
    DevType devType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtDrvGetLocalDevIDByHostDevID).stubs().will(invoke(StubHrtDrvGetLocalDevIDByHostDevID));

    HcommRoceChannelRes res{};
    res.qpsPerConnection = 1U;
    res.localMemCount = 0U;
    res.remoteMemCount = 0U;
    res.localMem = nullptr;
    res.remoteMem = nullptr;
    res.QpInfo[0].qpPtr = 0x7000ULL;
    res.QpInfo[0].sqIndex = 1U;
    res.QpInfo[0].dbIndex = 2U;

    HcommDeviceInfo dev{};
    dev.deviceLogicId = 0;
    dev.devicePhyId = 0U;

    ChannelHandle h = nullptr;
    ASSERT_EQ(AicpuTsRoceResHandler::Instance().Parse(&res, sizeof(res), dev, h), HCCL_SUCCESS);
    ASSERT_NE(h, nullptr);
    EXPECT_TRUE(AicpuTsRoceResHandler::Instance().Destroy(h));
    EXPECT_FALSE(AicpuTsRoceResHandler::Instance().Destroy(h));
}
