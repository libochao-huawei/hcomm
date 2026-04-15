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

#include "channel.h"
#include "hcomm_res_defs.h"

using namespace hcomm;

TEST(UtChannelRoceFactory, CreateChannel_NullEndpoint_AicpuTs_Roce_Returns_E_PTR)
{
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    EXPECT_EQ(Channel::CreateChannel(nullptr, COMM_ENGINE_AICPU_TS, desc, ch), HCCL_E_PTR);
    EXPECT_EQ(ch.get(), nullptr);
}

TEST(UtChannelRoceFactory, CreateChannel_NullEndpoint_Aicpu_Roce_Returns_E_PTR)
{
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    EXPECT_EQ(Channel::CreateChannel(nullptr, COMM_ENGINE_AICPU, desc, ch), HCCL_E_PTR);
    EXPECT_EQ(ch.get(), nullptr);
}
