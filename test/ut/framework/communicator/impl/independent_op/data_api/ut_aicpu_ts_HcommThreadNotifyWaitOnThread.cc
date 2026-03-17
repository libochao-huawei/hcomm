/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_aicpu_ts_base.h"

#include "resource_entities.h"
#include "hccl_api_data.h"

using namespace hccl;

struct WaitThreadEntityBuffer {
    ThreadEntity entity;
    NotifyEntity notifiesData[2];
};

static const uint32_t kWaitQueueCapacity = 4;
static const ThreadServiceHandle kValidWaitService = 0x200;

class UtAicpuTsHcommThreadNotifyWaitOnThread : public UtAicpuTsBase
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "UtAicpuTsHcommThreadNotifyWaitOnThread tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UtAicpuTsHcommThreadNotifyWaitOnThread tests tear down." << std::endl;
    }

    virtual void SetUp() override
    {
        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(false))
            .will(returnValue(HCCL_SUCCESS));

        HcclResult ret = HCCL_E_RESERVED;
        ret = tsThreadHost_.Init();
        ASSERT_EQ(ret, HCCL_SUCCESS);
        std::string packedData = tsThreadHost_.GetUniqueId();

        GlobalMockObject::verify(); // Clear previous MOCKER

        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(true))
            .will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(DevType::DEV_TYPE_950))
            .will(returnValue(HCCL_SUCCESS));

        // ---- TS thread (for AicpuTs wait tests) ----
        tsThreadDevPtr_ = std::make_unique<AicpuTsThread>(packedData);
        ret = tsThreadDevPtr_->Init();
        ASSERT_EQ(ret, HCCL_SUCCESS);
        ret = tsThreadDevPtr_->SetAddTaskInfoCallback(callback_);
        ASSERT_EQ(ret, HCCL_SUCCESS);
        ASSERT_NE(tsThreadDevPtr_->GetStreamLitePtr(), nullptr);

        memset(&tsBuf_, 0, sizeof(tsBuf_));
        tsBuf_.entity.type = THREAD_TYPE_TS;
        tsBuf_.entity.engine = COMM_ENGINE_AICPU;
        tsBuf_.entity.threadObjAddr = reinterpret_cast<uint64_t>(tsThreadDevPtr_.get());
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
        UtAicpuTsBase::TearDown();
        for (uint64_t i = headIdx_; i != tailIdx_; i = (i + 1) % kWaitQueueCapacity) {
            if (msgQueue_[i].args != nullptr) {
                free(msgQueue_[i].args);
                msgQueue_[i].args = nullptr;
            }
        }
        std::cout << "A Test case in UtAicpuTsHcommThreadNotifyWaitOnThread TearDown" << std::endl;
    }

    uint32_t notifyIdx = 0;
    uint32_t timeOut = 1800;
    int32_t res{HCCL_E_RESERVED};

    // TS thread
    AicpuTsThread tsThreadHost_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    std::unique_ptr<AicpuTsThread> tsThreadDevPtr_;
    using FuncCb = std::function<HcclResult (u32, u32, const Hccl::TaskParam &, u64)>;
    FuncCb callback_ = [](u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle) {return HCCL_SUCCESS;};
    WaitThreadEntityBuffer tsBuf_;

    // CPU thread
    WaitThreadEntityBuffer cpuBuf_;
    uint64_t headIdx_ = 0;
    uint64_t tailIdx_ = 0;
    ThreadMsgEntity msgQueue_[kWaitQueueCapacity];

    ThreadHandle tsHandle_ = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    ThreadHandle cpuHandle_ = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);

    int32_t res_{HCCL_E_RESERVED};
};

// AicpuTs wait normal -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_AicpuTsWaitNormal_Expect_ReturnIsHCCL_SUCCESS)
{

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
