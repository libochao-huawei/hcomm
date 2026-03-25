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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#undef private

using namespace hccl;

class UtAicpuTsHcommWriteWithNotifyNbi : public testing::Test
{
protected:
    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::vector<char> uniqueId;
    Hccl::UbTransportLiteImpl transportOnDevice{uniqueId};
    ChannelHandle channel = reinterpret_cast<ChannelHandle>(&transportOnDevice);
    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(tempDst);
    void *src = reinterpret_cast<void *>(tempSrc);
    uint64_t len = sizeof(tempDst);
    uint32_t notifyIdx = 0;
    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommWriteWithNotifyNbi, Ut_HcommWriteWithNotifyNbi_When_Src_IsNull_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    res = HcommWriteWithNotifyNbi(channel, nullptr, src, len, notifyIdx);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

TEST_F(UtAicpuTsHcommWriteWithNotifyNbi, Ut_HcommWriteWithNotifyNbi_When_Dst_IsNull_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    res = HcommWriteWithNotifyNbi(channel, dst, nullptr, len, notifyIdx);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

TEST_F(UtAicpuTsHcommWriteWithNotifyNbi, Ut_HcommWriteWithNotifyNbi_When_AllValid_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    res = HcommWriteWithNotifyNbi(channel, dst, src, len, notifyIdx);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}
