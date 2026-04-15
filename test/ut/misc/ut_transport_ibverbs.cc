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

#define private public
#define protected public
#include "dispatcher_pub.h"
#include "transport_device_ibverbs_pub.h"
#include "dlhns_function.h"
#include "network/hccp_common.h"
#undef protected
#undef private

using namespace hccl;

class TransportDeviceIbverbsTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

static void FillMinimalMemDetailsQpData(TransportDeviceIbverbsData &d)
{
    d.useMemDetailsMgr = true;
    d.qpsPerConnection = 1U;
    d.qpInfo.resize(1U);
    d.qpInfo[0].qpPtr = 0x5000ULL;
    d.qpInfo[0].sqIndex = 1U;
    d.qpInfo[0].dbIndex = 2U;
    d.multiQpThreshold = HCCL_MULTI_QP_THRESHOLD_DEFAULT;
    d.useAtomicWrite = false;

    RoceMemDetails loc{};
    loc.addr = 0x20000ULL;
    loc.devAddr = 0xC20000ULL;
    loc.size = 4096ULL;
    loc.key = 11U;
    d.localRoceMemDetailsList = { loc };

    RoceMemDetails rem{};
    rem.addr = 0x10000ULL;
    rem.devAddr = 0xC10000ULL;
    rem.size = 4096ULL;
    rem.key = 22U;
    d.remoteRoceMemDetailsList = { rem };
}

TEST_F(TransportDeviceIbverbsTest, Ut_InitMemDetails_InvalidQpSize_Returns_INTERNAL)
{
    HcclDispatcher dispatcherPtr = nullptr;
    ASSERT_EQ(HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr), HCCL_SUCCESS);
    ASSERT_NE(dispatcherPtr, nullptr);
    auto *dispatcher = reinterpret_cast<DispatcherPub *>(dispatcherPtr);

    MachinePara machinePara{};
    machinePara.deviceLogicId = 0;
    TransportDeviceIbverbsData d{};
    d.useMemDetailsMgr = true;
    d.qpsPerConnection = 1U;
    d.qpInfo.resize(3U);
    std::chrono::milliseconds timeout{ 1 };

    TransportDeviceIbverbs link(dispatcher, nullptr, machinePara, timeout, d);
    EXPECT_EQ(link.Init(), HCCL_E_INTERNAL);

    ASSERT_EQ(HcclDispatcherDestroy(dispatcherPtr), HCCL_SUCCESS);
}

TEST_F(TransportDeviceIbverbsTest, Ut_InitMemDetails_Success_Then_ResolveRdmaAddrsAndKeys)
{
    MOCKER_CPP(&DlHnsFunction::DlHnsFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));

    HcclDispatcher dispatcherPtr = nullptr;
    ASSERT_EQ(HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr), HCCL_SUCCESS);
    ASSERT_NE(dispatcherPtr, nullptr);
    auto *dispatcher = reinterpret_cast<DispatcherPub *>(dispatcherPtr);

    MachinePara machinePara{};
    machinePara.deviceLogicId = 0;
    TransportDeviceIbverbsData d{};
    FillMinimalMemDetailsQpData(d);
    std::chrono::milliseconds timeout{ 1 };

    TransportDeviceIbverbs link(dispatcher, nullptr, machinePara, timeout, d);
    ASSERT_EQ(link.Init(), HCCL_SUCCESS);
    EXPECT_TRUE(link.useMemDetailsLookup_);

    TransportDeviceIbverbs::RdmaAddrKeyResolveParam param{};
    param.remoteAddr = reinterpret_cast<const void *>(static_cast<uintptr_t>(0x10000ULL));
    param.localAddr = reinterpret_cast<const void *>(static_cast<uintptr_t>(0x20000ULL));
    param.length = 4096ULL;
    ASSERT_EQ(link.ResolveRdmaAddrsAndKeys(param), HCCL_SUCCESS);
    EXPECT_EQ(param.dstKey, 22U);
    EXPECT_EQ(param.srcKey, 11U);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(param.transRemoteAddr), static_cast<uintptr_t>(0xC10000ULL));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(param.transLocalAddr), static_cast<uintptr_t>(0xC20000ULL));

    ASSERT_EQ(HcclDispatcherDestroy(dispatcherPtr), HCCL_SUCCESS);
}

TEST_F(TransportDeviceIbverbsTest, Ut_ResolveRdmaAddrsAndKeys_MemDetails_Miss_Returns_INTERNAL)
{
    MOCKER_CPP(&DlHnsFunction::DlHnsFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));

    HcclDispatcher dispatcherPtr = nullptr;
    ASSERT_EQ(HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr), HCCL_SUCCESS);
    auto *dispatcher = reinterpret_cast<DispatcherPub *>(dispatcherPtr);

    MachinePara machinePara{};
    machinePara.deviceLogicId = 0;
    TransportDeviceIbverbsData d{};
    FillMinimalMemDetailsQpData(d);
    std::chrono::milliseconds timeout{ 1 };

    TransportDeviceIbverbs link(dispatcher, nullptr, machinePara, timeout, d);
    ASSERT_EQ(link.Init(), HCCL_SUCCESS);

    TransportDeviceIbverbs::RdmaAddrKeyResolveParam param{};
    param.remoteAddr = reinterpret_cast<const void *>(static_cast<uintptr_t>(0xDEADBEEFULL));
    param.localAddr = reinterpret_cast<const void *>(static_cast<uintptr_t>(0x20000ULL));
    param.length = 4096ULL;
    EXPECT_EQ(link.ResolveRdmaAddrsAndKeys(param), HCCL_E_INTERNAL);

    ASSERT_EQ(HcclDispatcherDestroy(dispatcherPtr), HCCL_SUCCESS);
}

class TransportIbverbsTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(TransportIbverbsTest, Ut_Init_UserMemEnableTrue_WithoutInputMem_Returns_PTR)
{
    MachinePara mp{};
    mp.userMemEnable = true;
    mp.notifyNum = 0;
    std::chrono::milliseconds timeout{ 1 };
    TransportIbverbs ib(nullptr, nullptr, mp, timeout);
    EXPECT_EQ(ib.Init(), HCCL_E_PTR);
}

TEST_F(TransportIbverbsTest, Ut_FillExchangeDataTotalSize_UserMemEnable_AddsTwoMemMsg)
{
    MachinePara mpOff{};
    mpOff.userMemEnable = false;
    mpOff.notifyNum = 0;
    mpOff.isIndOp = false;
    std::chrono::milliseconds timeout{ 1 };
    TransportIbverbs ibOff(nullptr, nullptr, mpOff, timeout);
    ibOff.qpsPerConnection_ = 1U;
    ibOff.notifyNum_ = 0U;
    ASSERT_EQ(ibOff.FillExchangeDataTotalSize(), HCCL_SUCCESS);
    const u64 baseSize = ibOff.exchangeDataTotalSize_;

    MachinePara mpOn{};
    mpOn.userMemEnable = true;
    mpOn.notifyNum = 0;
    mpOn.isIndOp = false;
    TransportIbverbs ibOn(nullptr, nullptr, mpOn, timeout);
    ibOn.qpsPerConnection_ = 1U;
    ibOn.notifyNum_ = 0U;
    ASSERT_EQ(ibOn.FillExchangeDataTotalSize(), HCCL_SUCCESS);
    EXPECT_EQ(ibOn.exchangeDataTotalSize_, baseSize + 2ULL * static_cast<u64>(sizeof(MemMsg)));
}
