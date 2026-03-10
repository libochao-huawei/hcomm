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

#define private public
#include "my_rank.h"
#undef private

#include "channel_logger.h"
#include "hcomm_c_adpt.h"

using namespace hccl;

namespace {
Hccl::TlsStatus g_expectedTlsStatus = Hccl::TlsStatus::UNKNOWN;
bool g_channelLoggerCalled = false;
Hccl::TlsStatus g_loggedTlsStatus = Hccl::TlsStatus::UNKNOWN;
std::vector<HcclResult> g_channelStatusSequence;
size_t g_channelStatusIndex = 0;

HcclResult StubHrtGetDeviceSuccess(s32 *deviceLogicId)
{
    *deviceLogicId = 1;
    return HCCL_SUCCESS;
}

HcclResult StubHrtGetDevicePhyIdByIndexSuccess(u32, u32 &devicePhyId, bool)
{
    devicePhyId = 8;
    return HCCL_SUCCESS;
}

HcclResult StubMyRankHrtRaGetTlsStatus(RaInfo *info, Hccl::TlsStatus &tlsStatus)
{
    EXPECT_NE(info, nullptr);
    EXPECT_EQ(info->phyId, 8U);
    tlsStatus = g_expectedTlsStatus;
    return HCCL_SUCCESS;
}

HcclResult StubMyRankGetLocalTlsStatusSuccess(const MyRank *, Hccl::TlsStatus &tlsStatus)
{
    tlsStatus = g_expectedTlsStatus;
    return HCCL_SUCCESS;
}

HcclResult StubHcommChannelGetStatus(const ChannelHandle *, uint32_t, int32_t *statusList)
{
    if (statusList != nullptr) {
        statusList[0] = 0;
    }
    if (g_channelStatusIndex >= g_channelStatusSequence.size()) {
        return HCCL_SUCCESS;
    }
    return g_channelStatusSequence[g_channelStatusIndex++];
}

void StubPrintChannelErrorDetails(
    uint32_t,
    uint32_t,
    const HcclChannelDesc *,
    ChannelHandle *,
    int32_t *,
    int64_t,
    Hccl::TlsStatus tlsStatus)
{
    g_channelLoggerCalled = true;
    g_loggedTlsStatus = tlsStatus;
}
}

class MyRankTlsTest : public testing::Test {
protected:
    void SetUp() override
    {
        callbacks_.getAicpuCommState = []() { return false; };
        callbacks_.setAicpuCommState = [](bool) {};
        callbacks_.kernelLaunchAicpuCommInit = []() { return HCCL_SUCCESS; };
        myRank_.reset(new MyRank(nullptr, 0, config_, callbacks_));
        g_expectedTlsStatus = Hccl::TlsStatus::UNKNOWN;
        g_channelLoggerCalled = false;
        g_loggedTlsStatus = Hccl::TlsStatus::UNKNOWN;
        g_channelStatusSequence.clear();
        g_channelStatusIndex = 0;
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        myRank_.reset();
    }

    CommConfig config_ {};
    ManagerCallbacks callbacks_ {};
    std::unique_ptr<MyRank> myRank_ {};
};

TEST_F(MyRankTlsTest, Ut_GetLocalTlsStatus_When_HrtGetDeviceFails_Expect_ReturnSameError)
{
    Hccl::TlsStatus tlsStatus = Hccl::TlsStatus::UNKNOWN;

    MOCKER(hrtGetDevice).stubs().will(returnValue(HCCL_E_RUNTIME));

    HcclResult ret = myRank_->GetLocalTlsStatus(tlsStatus);

    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(MyRankTlsTest, Ut_GetLocalTlsStatus_When_HrtGetDevicePhyIdByIndexFails_Expect_ReturnSameError)
{
    Hccl::TlsStatus tlsStatus = Hccl::TlsStatus::UNKNOWN;

    MOCKER(hrtGetDevice).stubs().will(invoke(StubHrtGetDeviceSuccess));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs().will(returnValue(HCCL_E_RUNTIME));

    HcclResult ret = myRank_->GetLocalTlsStatus(tlsStatus);

    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(MyRankTlsTest, Ut_GetLocalTlsStatus_When_HrtRaGetTlsStatusFails_Expect_ReturnSameError)
{
    Hccl::TlsStatus tlsStatus = Hccl::TlsStatus::UNKNOWN;

    MOCKER(hrtGetDevice).stubs().will(invoke(StubHrtGetDeviceSuccess));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs().will(invoke(StubHrtGetDevicePhyIdByIndexSuccess));
    MOCKER(Hccl::HrtRaGetTlsStatus).stubs().will(returnValue(HCCL_E_NETWORK));

    HcclResult ret = myRank_->GetLocalTlsStatus(tlsStatus);

    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

TEST_F(MyRankTlsTest, Ut_GetLocalTlsStatus_When_AllDependenciesSucceed_Expect_ReturnSuccessAndTlsStatusUpdated)
{
    Hccl::TlsStatus tlsStatus = Hccl::TlsStatus::UNKNOWN;
    g_expectedTlsStatus = Hccl::TlsStatus::ENABLE;

    MOCKER(hrtGetDevice).stubs().will(invoke(StubHrtGetDeviceSuccess));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs().will(invoke(StubHrtGetDevicePhyIdByIndexSuccess));
    MOCKER(Hccl::HrtRaGetTlsStatus).stubs().will(invoke(StubMyRankHrtRaGetTlsStatus));

    HcclResult ret = myRank_->GetLocalTlsStatus(tlsStatus);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(tlsStatus, Hccl::TlsStatus::ENABLE);
}

TEST_F(MyRankTlsTest, Ut_BatchConnectChannels_When_RetryThenSuccess_Expect_ReturnSuccess)
{
    HcclChannelDesc channelDesc {};
    ChannelHandle channelHandle = reinterpret_cast<ChannelHandle>(0x1);
    g_channelStatusSequence = {HCCL_E_AGAIN, HCCL_SUCCESS};

    MOCKER(HcommChannelGetStatus).stubs().will(invoke(StubHcommChannelGetStatus));

    HcclResult ret = myRank_->BatchConnectChannels(&channelDesc, &channelHandle, 1);

    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MyRankTlsTest, Ut_BatchConnectChannels_When_ConnectFails_Expect_ReturnOriginalError)
{
    HcclChannelDesc channelDesc {};
    ChannelHandle channelHandle = reinterpret_cast<ChannelHandle>(0x1);
    g_expectedTlsStatus = Hccl::TlsStatus::ENABLE;
    g_channelStatusSequence = {HCCL_E_NETWORK};

    MOCKER(HcommChannelGetStatus).stubs().will(invoke(StubHcommChannelGetStatus));
    MOCKER_CPP(&MyRank::GetLocalTlsStatus).stubs().will(invoke(StubMyRankGetLocalTlsStatusSuccess));
    MOCKER(logger::ChannelLogger::PrintChannelErrorDetails).stubs().will(invoke(StubPrintChannelErrorDetails));

    HcclResult ret = myRank_->BatchConnectChannels(&channelDesc, &channelHandle, 1);

    EXPECT_EQ(ret, HCCL_E_NETWORK);
    EXPECT_TRUE(g_channelLoggerCalled);
    EXPECT_EQ(g_loggedTlsStatus, Hccl::TlsStatus::ENABLE);
}

TEST_F(MyRankTlsTest, Ut_BatchConnectChannels_When_GetLocalTlsStatusFailsAfterConnectError_Expect_StillReturnOriginalError)
{
    HcclChannelDesc channelDesc {};
    ChannelHandle channelHandle = reinterpret_cast<ChannelHandle>(0x1);
    g_channelStatusSequence = {HCCL_E_NETWORK};

    MOCKER(HcommChannelGetStatus).stubs().will(invoke(StubHcommChannelGetStatus));
    MOCKER_CPP(&MyRank::GetLocalTlsStatus).stubs().will(returnValue(HCCL_E_PTR));
    MOCKER(logger::ChannelLogger::PrintChannelErrorDetails).stubs().will(invoke(StubPrintChannelErrorDetails));

    HcclResult ret = myRank_->BatchConnectChannels(&channelDesc, &channelHandle, 1);

    EXPECT_EQ(ret, HCCL_E_NETWORK);
    EXPECT_TRUE(g_channelLoggerCalled);
    EXPECT_EQ(g_loggedTlsStatus, Hccl::TlsStatus::UNKNOWN);
}
