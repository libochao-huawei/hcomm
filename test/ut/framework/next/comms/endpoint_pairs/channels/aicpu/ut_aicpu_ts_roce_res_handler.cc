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

#include "aicpu/device/aicpu_ts_roce_res_handler.h"
#include "channel_param.h"

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
