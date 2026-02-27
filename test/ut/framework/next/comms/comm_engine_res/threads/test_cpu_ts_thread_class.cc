/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "../../../hccl_api_base_test.h"
#include "local_notify_impl.h"
#include "llt_hccl_stub_rank_graph.h"

class TestCpuTsThread : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestCpuTsThread, Ut_CpuTsThread_Init_When_Normal_Expect_Return_HCCL_SUCCESS)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
    .stubs()
    .with(outBound(isDeviceSide))
    .will(returnValue(HCCL_SUCCESS));
    CpuTsThread cpuThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream *stream = cpuThread.GetStream();
    EXPECT_NE(stream, nullptr);
    uint32_t notifyNum = cpuThread.GetNotifyNum();
    EXPECT_EQ(2, notifyNum);
    void* notify = cpuThread.GetNotify(1);
    EXPECT_NE(nullptr, notify);
}

TEST_F(TestCpuTsThread, Ut_CpuTsThread_Init_When_GetRunSideIsDeviceFailed_Expect_Return_HCCL_E_DRV)
{
    MOCKER(GetRunSideIsDevice)
    .stubs()
    .will(returnValue(HCCL_E_DRV));
    CpuTsThread cpuThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_DRV);

}

TEST_F(TestCpuTsThread, Ut_CpuTsThread_Init_When_AllocDeviceStream_Expect_Return_HCCL_E_NOT_SUPPORT)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
    .stubs()
    .with(outBound(isDeviceSide))
    .will(returnValue(HCCL_SUCCESS));

    CpuTsThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::HOST_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(TestCpuTsThread, Ut_CpuTsThread_Init_When_AllocNotifyFailed_Expect_Return_HCCL_E_RUNTIME)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
    .stubs()
    .with(outBound(isDeviceSide))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtNotifyGetOffset)
    .stubs()
    .will(returnValue(HCCL_E_RUNTIME));

    CpuTsThread cpuThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY );
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

// TEST_F(TestCpuTsThread, Ut_CpuTsThread_Init_When_Alloc310DeviceNotify_Expect_Return_HCCL_SUCCESS)
// {
//     bool isDeviceSide{false};
//     MOCKER(GetRunSideIsDevice)
//     .stubs()
//     .with(outBound(isDeviceSide))
//     .will(returnValue(HCCL_SUCCESS));

//     MOCKER(Is310PDevice)
//     .stubs()
//     .will(returnValue(true));

//     CpuTsThread cpuThread(StreamType::STREAM_TYPE_ONLINE, 2, NotifyLoadType::HOST_NOTIFY);
//     HcclResult ret = cpuThread.Init();
//     EXPECT_EQ(ret, HCCL_SUCCESS);
// }