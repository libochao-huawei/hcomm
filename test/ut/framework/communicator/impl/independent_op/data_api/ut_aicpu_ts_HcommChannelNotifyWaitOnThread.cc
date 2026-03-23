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
#include "ub_transport_lite_impl.h"
#undef private

using namespace hccl;

class UtAicpuTsHcommChannelNotifyWaitOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        threadOnDevice.devType_ = DevType::DEV_TYPE_950;
        threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        threadOnDevice.pImpl_->streamLiteVoidPtr_ = reinterpret_cast<void *>(0x123456);
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    std::vector<char> uniqueId;
    Hccl::UbTransportLiteImpl transportOnDevice{uniqueId};
    ChannelHandle channel = reinterpret_cast<ChannelHandle>(&transportOnDevice);
    uint32_t notifyIdx = 0;
    uint32_t timeout = 1800;
    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommChannelNotifyWaitOnThread, Ut_HcommChannelNotifyWaitOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    res = HcommChannelNotifyWaitOnThread(thread, channel, notifyIdx, timeout);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommChannelNotifyWaitOnThread, Ut_HcommChannelNotifyWaitOnThread_When_Thread_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommChannelNotifyWaitOnThread(0, channel, notifyIdx, timeout);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommChannelNotifyWaitOnThread, Ut_HcommChannelNotifyWaitOnThread_When_Channel_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommChannelNotifyWaitOnThread(thread, 0, notifyIdx, timeout);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommChannelNotifyWaitOnThread, Ut_HcommChannelNotifyWaitOnThread_When_StreamLite_NotFound_Expect_ReturnIsHCCL_E_PTR)
{
    threadOnDevice.pImpl_->streamLiteVoidPtr_ = nullptr;
    res = HcommChannelNotifyWaitOnThread(thread, channel, notifyIdx, timeout);
    EXPECT_EQ(res, HCCL_E_PTR);
}