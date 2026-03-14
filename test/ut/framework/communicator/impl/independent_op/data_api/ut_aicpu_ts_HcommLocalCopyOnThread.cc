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
#include "dispatcher_aicpu.h"
#undef private

using namespace hccl;

class UtAicpuTsHcommLocalCopyOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().will(returnValue(HCCL_SUCCESS));
        threadOnDevice.devType_ = DevType::DEV_TYPE_950;
        threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(tempDst);
    void *src = reinterpret_cast<void *>(tempSrc);
    uint64_t len = sizeof(tempDst);
    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));
    
    AicpuTsThread aicpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = aicpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string mainStr = aicpuThread.GetUniqueId();
    isDeviceSide = true;
    GlobalMockObject::verify(); 
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

     MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(DevType::DEV_TYPE_950))
        .will(returnValue(HCCL_SUCCESS));

    AicpuTsThread mainDevThread(mainStr);
    ret = mainDevThread.Init();
    std::function<HcclResult (u32, u32, const Hccl::TaskParam &, u64)> callback = [](u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle) {return HCCL_SUCCESS;};
    mainDevThread.SetAddTaskInfoCallback(callback);
    EXPECT_EQ(ret, HCCL_SUCCESS);  

    void *expectPtr = reinterpret_cast<void *>(0x2345);
    void *streamPtr = mainDevThread.GetStreamLitePtr();
    EXPECT_NE(nullptr, streamPtr);

    ret = mainDevThread.LocalNotifyRecord(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
   
    ret = mainDevThread.LocalNotifyWait(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    void *src = reinterpret_cast<void *>(0x2345);
    void *dst = reinterpret_cast<void *>(0x2345);
    uint64_t sizeByte = 8;
    thread = reinterpret_cast<ThreadHandle>(&mainDevThread);
    res = HcommLocalCopyOnThread(thread, dst, src, len);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Thread_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommLocalCopyOnThread(0, dst, src, len);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Dst_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommLocalCopyOnThread(thread, nullptr, src, len);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Src_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommLocalCopyOnThread(thread, dst, nullptr, len);
    EXPECT_EQ(res, HCCL_E_PTR);
}
