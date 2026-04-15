/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>

#include "gtest/gtest.h"

#include "aicpu_channel_res_handler.h"
#include "hcomm_res_defs.h"

namespace {
constexpr uint32_t kKindTsRoce = 2U;
}

TEST(AicpuChannelResRegistryTest, GetHandlers_ContainsTsRoce)
{
    const auto &m = GetAicpuChannelResHandlers();
    ASSERT_FALSE(m.empty());
    EXPECT_NE(m.find(kKindTsRoce), m.end());
}

TEST(AicpuChannelResRegistryTest, RegisterUnregister_RoundTrip)
{
    const ChannelHandle h = static_cast<ChannelHandle>(0xABCULL);
    AicpuChannelResRegisterHandleKind(h, kKindTsRoce);
    AicpuChannelResUnregisterHandleKind(h);
    EXPECT_FALSE(AicpuChannelResDestroyForHandle(h));
}

TEST(AicpuChannelResRegistryTest, DestroyForHandle_WhenUnknown_ReturnsFalse)
{
    EXPECT_FALSE(AicpuChannelResDestroyForHandle(static_cast<ChannelHandle>(0xDEADBEEFULL)));
}

TEST(AicpuChannelResRegistryTest, DestroyForHandle_WhenKindNotInHandlers_ClearsAndReturnsFalse)
{
    const ChannelHandle h = static_cast<ChannelHandle>(0x1001ULL);
    AicpuChannelResRegisterHandleKind(h, 999U);
    EXPECT_FALSE(AicpuChannelResDestroyForHandle(h));
}

TEST(AicpuChannelResRegistryTest, DestroyForHandle_WhenKnownKind_DelegatesToHandler)
{
    const ChannelHandle h = static_cast<ChannelHandle>(0x2002ULL);
    AicpuChannelResRegisterHandleKind(h, kKindTsRoce);
    EXPECT_FALSE(AicpuChannelResDestroyForHandle(h));
    AicpuChannelResUnregisterHandleKind(h);
}
