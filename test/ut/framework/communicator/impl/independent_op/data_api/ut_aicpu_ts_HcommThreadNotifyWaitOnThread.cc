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
#include <cstring>

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#undef private

#include "resource_entities.h"
#include "hccl_api_data.h"

using namespace hccl;

// Buffer type that allocates ThreadEntity + trailing NotifyEntity array
struct WaitThreadEntityBuffer {
    ThreadEntity entity;
    NotifyEntity notifiesData[2];
};

// Queue backing memory for CPU thread sendQueue tests
static const uint32_t kWaitQueueCapacity = 4;
static const ThreadServiceHandle kValidWaitService = 0x200;

namespace {
    void InitNotifies(AicpuTsThread &thread)
    {
        thread.notifys_.reserve(thread.notifyNum_);
        for (uint32_t idx = 0; idx < thread.notifyNum_; idx++) {
            thread.notifys_.emplace_back(nullptr);
            thread.notifys_[idx].reset(new (std::nothrow) LocalNotify());
        }
    }
}

class UtAicpuTsHcommThreadNotifyWaitOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        // ---- TS thread (for AicpuTs wait tests) ----
        tsThread_.devType_ = DevType::DEV_TYPE_910_95;
        tsThread_.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        InitNotifies(tsThread_);

        memset(&tsBuf_, 0, sizeof(tsBuf_));
        tsBuf_.entity.type = THREAD_TYPE_TS;
        tsBuf_.entity.engine = COMM_ENGINE_AICPU;
        tsBuf_.entity.threadObjAddr = reinterpret_cast<uint64_t>(&tsThread_);
        tsBuf_.entity.notifyNum = 1;

        // ---- CPU thread (for CpuThread wait tests) ----
        headIdx_ = 0;
        tailIdx_ = 0;
        memset(msgQueue_, 0, sizeof(msgQueue_));

        memset(&cpuBuf_, 0, sizeof(cpuBuf_));
        cpuBuf_.entity.type = THREAD_TYPE_CPU;
        cpuBuf_.entity.engine = COMM_ENGINE_AICPU;
        cpuBuf_.entity.cpuRes.waitService = kValidWaitService;
        cpuBuf_.entity.cpuRes.sendQueue.addr = reinterpret_cast<uint64_t>(msgQueue_);
        cpuBuf_.entity.cpuRes.sendQueue.headIdxAddr = reinterpret_cast<uint64_t>(&headIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.tailIdxAddr = reinterpret_cast<uint64_t>(&tailIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.msgSize = sizeof(ThreadMsgEntity);
        cpuBuf_.entity.cpuRes.sendQueue.capacity = kWaitQueueCapacity;
        cpuBuf_.entity.notifyNum = 1;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
        // Free any malloc'd args written to the queue by HcommRequestServiceOnThread
        for (uint64_t i = headIdx_; i != tailIdx_; i = (i + 1) % kWaitQueueCapacity) {
            if (msgQueue_[i].args != nullptr) {
                free(msgQueue_[i].args);
                msgQueue_[i].args = nullptr;
            }
        }
    }

    // TS thread
    AicpuTsThread tsThread_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    WaitThreadEntityBuffer tsBuf_;

    // CPU thread
    WaitThreadEntityBuffer cpuBuf_;
    uint64_t headIdx_ = 0;
    uint64_t tailIdx_ = 0;
    ThreadMsgEntity msgQueue_[kWaitQueueCapacity];

    // Handles
    ThreadHandle tsHandle_ = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    ThreadHandle cpuHandle_ = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);

    int32_t res_{HCCL_E_RESERVED};
};

// AicpuTs wait normal -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_AicpuTsWaitNormal_Expect_ReturnIsHCCL_SUCCESS)
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
    res = HcommThreadNotifyWaitOnThread(thread, notifyIdx, timeOut);
    EXPECT_EQ(res, HCCL_SUCCESS);
    // TODO
    MOCKER_CPP(&Hccl::IAicpuTsThread::NotifyWait).stubs().will(returnValue(HCCL_SUCCESS));
    res_ = HcommThreadNotifyWaitOnThread(tsHandle_, 0, 1800);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_01: CpuThread wait normal, WaitService enqueued -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_CpuThreadWaitsNormal_Expect_ReturnIsHCCL_SUCCESS)
{
    res_ = HcommThreadNotifyWaitOnThread(cpuHandle_, 0, 1800);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_02: thread = 0 (null) -> HCCL_E_PTR
TEST_F(UtAicpuTsHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_ThreadIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res_ = HcommThreadNotifyWaitOnThread(0, 0, 1800);
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_03: notifyIdx out of range -> HCCL_E_PARA
TEST_F(UtAicpuTsHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_NotifyIdxOutOfRange_Expect_ReturnIsHCCL_E_PARA)
{
    // cpuBuf_.entity.notifyNum == 1, so index 1 is out of range
    res_ = HcommThreadNotifyWaitOnThread(cpuHandle_, 1, 1800);
    EXPECT_EQ(res_, HCCL_E_PARA);
}

// test_04: unsupported thread type -> HCCL_E_NOT_SUPPORT
TEST_F(UtAicpuTsHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_ThreadTypeNotSupported_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    WaitThreadEntityBuffer unsupportedBuf;
    memset(&unsupportedBuf, 0, sizeof(unsupportedBuf));
    unsupportedBuf.entity.type = THREAD_TYPE_INVALID;
    unsupportedBuf.entity.engine = COMM_ENGINE_AICPU;
    unsupportedBuf.entity.notifyNum = 1;

    ThreadHandle handle = reinterpret_cast<ThreadHandle>(&unsupportedBuf.entity);
    res_ = HcommThreadNotifyWaitOnThread(handle, 0, 1800);
    EXPECT_EQ(res_, HCCL_E_NOT_SUPPORT);
}
