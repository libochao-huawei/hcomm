/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "hccl/hccl_res.h"
#include "../../hccl_api_base_test.h"
#include "hccl_tbe_task.h"
#include "adapter_hal.h"
#include "dispatcher_ctx.h"
#include "hcomm_primitives.h"

using namespace hccl;
static const char* RANKTABLE_FILE_NAME = nullptr;

class HcclIndependentOpChannelTest : public BaseInit {
public:
    void SetUp() override {
        MOCKER(HcclTbeTaskInit)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
        BaseInit::SetUp();
        bool isDeviceSide = false;
        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(isDeviceSide))
            .will(returnValue(HCCL_SUCCESS));
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
        UT_COMM_CREATE_DEFAULT(comm);
        RANKTABLE_FILE_NAME = rankTableFileName;
        EXPECT_EQ(RANKTABLE_FILE_NAME != nullptr, true);
        EXPECT_EQ(comm != nullptr, true);
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
        Ut_Comm_Destroy(comm);
    }
};

TEST_F(HcclIndependentOpChannelTest, Ut_HcclChannelAcquire_When_NotifyNum_Is_Invalid_Expect_Para_Error)
{
    std::vector<HcclChannelDesc> channelDesc(1);
    HcclChannelDescInit(channelDesc.data(), 1);
    std::vector<ChannelHandle> channels(1);
    channelDesc[0].remoteRank = 2;
    channelDesc[0].channelProtocol = CommProtocol::COMM_PROTOCOL_HCCS;
    channelDesc[0].notifyNum = 65;
    HcclResult ret = HcclChannelAcquire(comm, CommEngine::COMM_ENGINE_AICPU_TS, channelDesc.data(), 1, channels.data());
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclIndependentOpChannelTest, Ut_ProcessRoceChannelDesc_When_IsCommunicatorV2_Is_False)
{
    std::vector<HcclChannelDesc> channelDescFinal(1);
    HcclChannelDescInit(channelDescFinal.data(), 1);

    std::vector<HcclChannelDesc> channelDesc(1);
    HcclChannelDescInit(channelDesc.data(), 1);

    channelDesc[0].roceAttr.queueNum = 3;
    channelDesc[0].roceAttr.retryInterval = 7;
    channelDesc[0].roceAttr.retryCount = 3;
    channelDesc[0].roceAttr.tc = 120;
    channelDesc[0].roceAttr.sl = 4;

    hccl::hcclComm *hcclCommTest = new hccl::hcclComm(1, 1, "123");
    MOCKER_CPP(&hcclComm::IsCommunicatorV2)
    .stubs()
    .will(returnValue(false));

    HcclResult ret = ProcessRoceChannelDesc(channelDesc[0], channelDescFinal[0], hcclCommTest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}