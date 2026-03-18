/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to License for details. You may not use this file except in compliance with License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
  
#include "../../../hccl_api_base_test.h"
#include "local_notify_impl.h"
#include "llt_hccl_stub_rank_graph.h"

class TestAicpuTsThreadMethods : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_DeInit_When_Normal_Expect_ClearAllMembers)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aicpuThread.DeInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream *stream = aicpuThread.GetStream();
    EXPECT_EQ(stream, nullptr);

    uint32_t notifyNum = aicpuThread.GetNotifyNum();
    EXPECT_EQ(notifyNum, 0);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetNotify_WhenIndexOutOfRange_ReturnNullptr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    LocalNotify *notify = aicpuThread.GetNotify(10);
    EXPECT_EQ(notify, nullptr);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetNotify_WhenIndexValid_ReturnValidNotify)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    LocalNotify *notify = aicpuThread.GetNotify(0);
    EXPECT_NE(notify, nullptr);

    notify = aicpuThread.GetNotify(1);
    EXPECT_NE(notify, nullptr);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_IsDeviceA5_WhenDevTypeIs950_ReturnTrue)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_950))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isA5 = aicpuThread.IsDeviceA5();
    EXPECT_EQ(isA5, true);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_IsDeviceA5_WhenDevTypeIsNot950_ReturnFalse)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_910_93))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isA5 = aicpuThread.IsDeviceA5();
    EXPECT_EQ(isA5, false);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetStream_WhenStreamValid_ReturnValidPtr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Stream *stream = aicpuThread.GetStream();
    EXPECT_NE(stream, nullptr);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetStreamLitePtr_WhenImplNull_ReturnNullptr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *streamLitePtr = aicpuThread.GetStreamLitePtr();
    EXPECT_EQ(streamLitePtr, nullptr);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_LaunchTask_WhenImplNull_LogErrorAndReturn)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    aicpuThread.LaunchTask();
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_LocalNotifyWait_WhenImplNull_ReturnEPtr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aicpuThread.LocalNotifyWait(0);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_LocalNotifyRecord_WhenImplNull_ReturnEPtr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aicpuThread.LocalNotifyRecord(0);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_LocalNotifyRecord_WithThreadHandle_ReturnENotSupport)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadHandle dstThread = 0;
    ret = aicpuThread.LocalNotifyRecord(dstThread, 0);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_LocalNotifyWait_WithTimeout_ReturnENotSupport)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aicpuThread.LocalNotifyWait(0, 1000);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_LocalCopy_WhenImplNull_ReturnEPtr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *dst = reinterpret_cast<void *>(0x1000);
    void *src = reinterpret_cast<void *>(0x2000);
    ret = aicpuThread.LocalCopy(dst, src, 1024);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_LocalReduce_WhenImplNull_ReturnEPtr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void *dst = reinterpret_cast<void *>(0x1000);
    void *src = reinterpret_cast<void *>(0x2000);
    ret = aicpuThread.LocalReduce(dst, src, 1024, HcommDataType::HCOMM_DATA_TYPE_INT32, HcommReduceOp::HCOMM_REDUCE_SUM);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetMaster_WhenSetTrue_ReturnTrue)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    aicpuThread.SetIsMaster(true);
    bool isMaster = aicpuThread.GetMaster();
    EXPECT_EQ(isMaster, true);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_SetIsMaster_WhenSetFalse_ReturnFalse)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    aicpuThread.SetIsMaster(false);
    bool isMaster = aicpuThread.GetMaster();
    EXPECT_EQ(isMaster, false);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_SupplementNotify_WhenNotifyNumLessThanOrEqual_ReturnSuccess)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aicpuThread.SupplementNotify(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aicpuThread.SupplementNotify(2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetStreamIdAndNotifyByUniqueId_WhenUniqueIdEmpty_ReturnEInternal)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    AicpuTsThread newThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    s32 streamId = 0;
    u32 notifyNum = 0;
    std::string notifyDesc;
    ret = newThread.GetStreamIdAndNotifyByUniqueId(streamId, notifyNum, notifyDesc);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetStreamIdAndNotifyByUniqueId_Normal_ReturnSuccess)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string uniqueId = aicpuThread.GetUniqueId();
    EXPECT_FALSE(uniqueId.empty());

    AicpuTsThread newThread(uniqueId);
    s32 streamId = 0;
    u32 notifyNum = 0;
    std::string notifyDesc;
    ret = newThread.GetStreamIdAndNotifyByUniqueId(streamId, notifyNum, notifyDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(streamId, 0);
    EXPECT_EQ(notifyNum, 2);
    EXPECT_FALSE(notifyDesc.empty());
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_SupplementNotify_WithNotifyDesc_ReturnSuccess)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string uniqueId = aicpuThread.GetUniqueId();
    EXPECT_FALSE(uniqueId.empty());

    AicpuTsThread newThread(uniqueId);
    ret = newThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 streamId = 0;
    u32 notifyNum = 0;
    std::string notifyDesc;
    ret = newThread.GetStreamIdAndNotifyByUniqueId(streamId, notifyNum, notifyDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = newThread.SupplementNotify(4, notifyDesc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetSqHeadAndTail_WhenImplNull_ReturnEPtr)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    uint32_t sqHead = 0;
    uint32_t sqTail = 0;
    ret = aicpuThread.GetSqHeadAndTail(sqHead, sqTail);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestAicpuTsThreadMethods, Ut_AicpuTsThread_GetUniqueId_WhenUniqueIdEmpty_CallUpdateUniqueId)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string uniqueId = aicpuThread.GetUniqueId();
    EXPECT_FALSE(uniqueId.empty());

    std::string uniqueId2 = aicpuThread.GetUniqueId();
    EXPECT_EQ(uniqueId, uniqueId2);
}
