/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../../ut_hcomm_base.h"
#include "channel_process.h"

class TestChannelProcessPureFunc : public TestHcommCAdptBase {
public:
    void SetUp() override
    {
        TestHcommCAdptBase::SetUp();
        ClearGlobalMaps();
    }

    void TearDown() override
    {
        ClearGlobalMaps();
        TestHcommCAdptBase::TearDown();
    }

    static void ClearGlobalMaps()
    {
        std::lock_guard<std::mutex> lock(hcomm::ChannelProcess::g_ChannelMapMtx);
        hcomm::ChannelProcess::g_ChannelMap.clear();
        hcomm::ChannelProcess::g_ChannelD2HMap.clear();
    }
};

TEST_F(TestChannelProcessPureFunc, Ut_FillChannelD2HMap_Normal_Success)
{
    constexpr uint32_t listNum = 3;
    ChannelHandle deviceHandles[listNum] = {0x1000, 0x2000, 0x3000};
    ChannelHandle hostHandles[listNum] = {0xA000, 0xB000, 0xC000};

    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(deviceHandles, hostHandles, listNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    {
        std::lock_guard<std::mutex> lock(hcomm::ChannelProcess::g_ChannelMapMtx);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap.size(), listNum);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x1000], 0xA000);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x2000], 0xB000);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x3000], 0xC000);
    }
}

TEST_F(TestChannelProcessPureFunc, Ut_FillChannelD2HMap_DuplicateDeviceHandle_HandlesConflict)
{
    constexpr uint32_t listNum = 2;
    ChannelHandle deviceHandles[listNum] = {0x1000, 0x1000};
    ChannelHandle hostHandles[listNum] = {0xA000, 0xB000};

    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(deviceHandles, hostHandles, listNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    {
        std::lock_guard<std::mutex> lock(hcomm::ChannelProcess::g_ChannelMapMtx);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap.size(), 1);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x1000], 0xA000);
    }
}

TEST_F(TestChannelProcessPureFunc, Ut_FillChannelD2HMap_SingleElement_Success)
{
    constexpr uint32_t listNum = 1;
    ChannelHandle deviceHandle = 0x12345678;
    ChannelHandle hostHandle = 0x87654321;

    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(&deviceHandle, &hostHandle, listNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    {
        std::lock_guard<std::mutex> lock(hcomm::ChannelProcess::g_ChannelMapMtx);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap.size(), 1);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[deviceHandle], hostHandle);
    }
}

TEST_F(TestChannelProcessPureFunc, Ut_FillChannelD2HMap_LargeList_Success)
{
    constexpr uint32_t listNum = 100;
    std::vector<ChannelHandle> deviceHandles(listNum);
    std::vector<ChannelHandle> hostHandles(listNum);

    for (uint32_t i = 0; i < listNum; ++i) {
        deviceHandles[i] = static_cast<ChannelHandle>(0x1000 + i);
        hostHandles[i] = static_cast<ChannelHandle>(0xA000 + i);
    }

    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(deviceHandles.data(), hostHandles.data(), listNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    {
        std::lock_guard<std::mutex> lock(hcomm::ChannelProcess::g_ChannelMapMtx);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap.size(), listNum);
        for (uint32_t i = 0; i < listNum; ++i) {
            EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[deviceHandles[i]], hostHandles[i]);
        }
    }
}

TEST_F(TestChannelProcessPureFunc, Ut_FillChannelD2HMap_MixedOrder_Success)
{
    constexpr uint32_t listNum = 4;
    ChannelHandle deviceHandles[listNum] = {0x4000, 0x1000, 0x3000, 0x2000};
    ChannelHandle hostHandles[listNum] = {0xD000, 0xA000, 0xC000, 0xB000};

    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(deviceHandles, hostHandles, listNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    {
        std::lock_guard<std::mutex> lock(hcomm::ChannelProcess::g_ChannelMapMtx);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap.size(), listNum);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x4000], 0xD000);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x1000], 0xA000);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x3000], 0xC000);
        EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap[0x2000], 0xB000);
    }
}
