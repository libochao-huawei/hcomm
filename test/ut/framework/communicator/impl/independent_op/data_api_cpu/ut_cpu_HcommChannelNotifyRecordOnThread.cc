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
#include "cpu_ts_thread.h"
#include "host_cpu_roce_channel.h"
#undef private

using namespace hccl;

class UtCpuHcommChannelNotifyRecordOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        threadOnHost.stream_.reset(new (std::nothrow) Stream());
        MOCKER(&HcclRemoteNotifyRecord).stubs().will(returnValue(HCCL_SUCCESS));
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    CpuTsThread threadOnHost{StreamType::STREAM_TYPE_HOST, 1, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnHost);
    ChannelHandle channel910C = 0x03;
    hcomm::HostCpuRoceChannel channelOnHost{0x01, 0x02};
    ChannelHandle channel = reinterpret_cast<ChannelHandle>(&channelOnHost);
    uint32_t notifyIdx = 0;
    int32_t res{HCCL_E_RESERVED};
};

// 950

TEST_F(UtCpuHcommChannelNotifyRecordOnThread, Ut_HcommChannelNotifyRecordOnThread_When_950_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER(&IsDevice950).stubs().will(returnValue(true));
    res = HcommChannelNotifyRecordOnThread(thread, channel, notifyIdx);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtCpuHcommChannelNotifyRecordOnThread, Ut_HcommChannelNotifyRecordOnThread_When_950_Thread_IsNull_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER(&IsDevice950).stubs().will(returnValue(true));
    // On 950, thread is not used, so it could be nullptr.
    res = HcommChannelNotifyRecordOnThread(0, channel, notifyIdx);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtCpuHcommChannelNotifyRecordOnThread, Ut_HcommChannelNotifyRecordOnThread_When_950_Channel_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    MOCKER(&IsDevice950).stubs().will(returnValue(true));
    res = HcommChannelNotifyRecordOnThread(thread, 0, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

// 910C

TEST_F(UtCpuHcommChannelNotifyRecordOnThread, Ut_HcommChannelNotifyRecordOnThread_When_910C_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER(&IsDevice950).stubs().will(returnValue(false));
    res = HcommChannelNotifyRecordOnThread(thread, channel910C, notifyIdx);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtCpuHcommChannelNotifyRecordOnThread, Ut_HcommChannelNotifyRecordOnThread_When_910C_Thread_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    MOCKER(&IsDevice950).stubs().will(returnValue(false));
    res = HcommChannelNotifyRecordOnThread(0, channel910C, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtCpuHcommChannelNotifyRecordOnThread, Ut_HcommChannelNotifyRecordOnThread_When_910C_GetStream_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    MOCKER(&IsDevice950).stubs().will(returnValue(false));
    threadOnHost.stream_.reset();
    res = HcommChannelNotifyRecordOnThread(thread, channel910C, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}
