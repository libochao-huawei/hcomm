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
#include "cpu_thread.h"
#include "local_notify_impl.h"
#include "llt_hccl_stub_rank_graph.h"

class TestCpuThread : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestCpuThread, Ut_MsgQueue_Init_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestCpuThread, Ut_MsgQueue_Push_And_Pop_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(Invoke([](void **devPtr, size_t size, uint32_t flag) {
            *devPtr = malloc(size);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(Invoke([](void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind) {
            memcpy(dst, src, count);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtFree)
        .stubs()
        .will(Invoke([](void *devPtr) {
            free(devPtr);
            return ACL_SUCCESS;
        }));
    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;
    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity poppedEntity;
    ret = msgQueue.pop(poppedEntity);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(poppedEntity.msgId, entity.msgId);
    EXPECT_EQ(poppedEntity.serviceHandle, entity.serviceHandle);
    EXPECT_EQ(poppedEntity.args, entity.args);
    EXPECT_EQ(poppedEntity.argsSizeByte, entity.argsSizeByte);
}

TEST_F(TestCpuThread, Ut_MsgQueue_Pop_While_Empty_Expect_Return_ERROR)
{

}

TEST_F(TestCpuThread, Ut_MsgQueue_Push_While_Full_Expect_Return_ERROR)
{

}



// Helper that prepares mocks and returns a CpuThread already initialized successfully.
static CpuThread CreateInitCpuThread() {
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtSetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(cpuThread.Init(), HCCL_SUCCESS);
    return cpuThread;
}


TEST_F(TestCpuThread, Ut_CpuThread_Init_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtSetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    uint32_t notifyNum = cpuThread.GetNotifyNum();
    EXPECT_EQ(2, notifyNum);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_hrtGetDeviceFailed_Expect_Return_HCCL_E_DRV)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .will(returnValue(HCCL_E_DRV));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_DRV);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_aclrtGetCurrentContextFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_rtSetXpuDeviceFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_PrepareDpuKernelResourceFailed_Expect_Return_HCCL_E_OPEN_FILE_FAILURE)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_FILE));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_OPEN_FILE_FAILURE);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_LaunchDpuKernelFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_GetThreadEntity_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtSetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* threadEntity = nullptr;
    ret = cpuThread.GetThreadEntity(threadEntity);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
