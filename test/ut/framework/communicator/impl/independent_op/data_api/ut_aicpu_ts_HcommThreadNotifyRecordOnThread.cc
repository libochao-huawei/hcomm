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

struct RecordThreadEntityBuffer {
    ThreadEntity entity;
    NotifyEntity notifiesData[2];
};

static const uint32_t kQueueCapacity = 4;
static const ThreadServiceHandle kValidRecordService = 0x100;
static const uint64_t kDeviceMemAddr = 0x8000;

class UtAicpuTsHcommThreadNotifyRecordOnThread : public UtAicpuTsBase
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "UtAicpuTsHcommThreadNotifyRecordOnThread tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UtAicpuTsHcommThreadNotifyRecordOnThread tests tear down." << std::endl;
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

        ret = dstTsThreadHost_.Init();
        ASSERT_EQ(ret, HCCL_SUCCESS);
        std::string dstPackedData = dstTsThreadHost_.GetUniqueId();

        GlobalMockObject::verify(); // Clear previous MOCKER

        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(true))
            .will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(DevType::DEV_TYPE_950))
            .will(returnValue(HCCL_SUCCESS));

        // ---- TS thread (src for TS->TS and TS->CPU tests) ----
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

        // ---- dst TS thread (for TS->TS tests) ----
        dstTsThreadDevPtr_ = std::make_unique<AicpuTsThread>(dstPackedData);
        ret = dstTsThreadDevPtr_->Init();
        ASSERT_EQ(ret, HCCL_SUCCESS);
        ret = dstTsThreadDevPtr_->SetAddTaskInfoCallback(callback_);
        ASSERT_EQ(ret, HCCL_SUCCESS);
        ASSERT_NE(dstTsThreadDevPtr_->GetStreamLitePtr(), nullptr);

        memset(&dstTsBuf_, 0, sizeof(dstTsBuf_));
        dstTsBuf_.entity.type = THREAD_TYPE_TS;
        dstTsBuf_.entity.engine = COMM_ENGINE_AICPU;
        dstTsBuf_.entity.threadObjAddr = reinterpret_cast<uint64_t>(dstTsThreadDevPtr_.get());
        dstTsBuf_.entity.notifyNum = 1;

        // ---- CPU thread (for TS->CPU and CPU->TS tests) ----
        headIdx_ = 0;
        tailIdx_ = 0;
        memset(msgQueue_, 0, sizeof(msgQueue_));

        // DataRing 模拟缓冲区
        dataRingHead_ = 0;
        dataRingTail_ = 0;
        memset(dataRingBuf_, 0, sizeof(dataRingBuf_));

        memset(&cpuBuf_, 0, sizeof(cpuBuf_));
        cpuBuf_.entity.type = THREAD_TYPE_CPU;
        cpuBuf_.entity.engine = COMM_ENGINE_AICPU;
        cpuBuf_.entity.cpuRes.recordService = kValidRecordService;
        cpuBuf_.entity.cpuRes.sendQueue.addr = reinterpret_cast<uint64_t>(msgQueue_);
        cpuBuf_.entity.cpuRes.sendQueue.headIdxAddr = reinterpret_cast<uint64_t>(&headIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.tailIdxAddr = reinterpret_cast<uint64_t>(&tailIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.msgSize = sizeof(ThreadMsgEntity);
        cpuBuf_.entity.cpuRes.sendQueue.capacity = kQueueCapacity;
        cpuBuf_.entity.cpuRes.dataRing.addr = reinterpret_cast<uint64_t>(dataRingBuf_);
        cpuBuf_.entity.cpuRes.dataRing.headIdxAddr = reinterpret_cast<uint64_t>(&dataRingHead_);
        cpuBuf_.entity.cpuRes.dataRing.tailIdxAddr = reinterpret_cast<uint64_t>(&dataRingTail_);
        cpuBuf_.entity.cpuRes.dataRing.capacity = sizeof(dataRingBuf_);
        cpuBuf_.entity.cpuRes.dataRing.reserved = 0;
        cpuBuf_.entity.notifyNum = 1;
        cpuBuf_.notifiesData[0].type = NOTIFY_TYPE_DEVICE_MEM;
        cpuBuf_.notifiesData[0].identifier = kDeviceMemAddr;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    uint32_t notifyIdx = 0;
    int32_t res{HCCL_E_RESERVED};

    // TS threads
    AicpuTsThread tsThreadHost_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    AicpuTsThread dstTsThreadHost_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    std::unique_ptr<AicpuTsThread> tsThreadDevPtr_;
    std::unique_ptr<AicpuTsThread> dstTsThreadDevPtr_;
    using FuncCb = std::function<HcclResult (u32, u32, const Hccl::TaskParam &, u64)>;
    FuncCb callback_ = [](u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle) {return HCCL_SUCCESS;};
    RecordThreadEntityBuffer tsBuf_;
    RecordThreadEntityBuffer dstTsBuf_;

    // CPU thread
    RecordThreadEntityBuffer cpuBuf_;
    uint64_t headIdx_ = 0;
    uint64_t tailIdx_ = 0;
    ThreadMsgEntity msgQueue_[kQueueCapacity];
    uint8_t dataRingBuf_[32768];
    uint64_t dataRingHead_ = 0;
    uint64_t dataRingTail_ = 0;

    ThreadHandle tsHandle_ = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    ThreadHandle dstTsHandle_ = reinterpret_cast<ThreadHandle>(&dstTsBuf_.entity);
    ThreadHandle cpuHandle_ = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);

    int32_t res_{HCCL_E_RESERVED};
};

// test_01: AicpuTs -> AicpuTs, normal -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsNotifyAicpuTsNormal_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER_CPP(&Hccl::IAicpuTsThread::NotifyRecordLoc).stubs().will(returnValue(HCCL_SUCCESS));
    res_ = HcommThreadNotifyRecordOnThread(tsHandle_, dstTsHandle_, 0);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_02: AicpuTs -> CpuThread, NotifyRecord succeeds -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsNotifyCpuThread_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER_CPP(&Hccl::IAicpuTsThread::WriteValue).stubs().will(returnValue(HCCL_SUCCESS));
    res_ = HcommThreadNotifyRecordOnThread(tsHandle_, cpuHandle_, 0);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_03: CpuThread -> AicpuTs, requests RecordService -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_CpuThreadNotifyAicpuTs_Expect_ReturnIsHCCL_SUCCESS)
{
    res_ = HcommThreadNotifyRecordOnThread(cpuHandle_, tsHandle_, 0);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_04: thread = 0 (null) -> HCCL_E_PTR
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_ThreadIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res_ = HcommThreadNotifyRecordOnThread(0, dstTsHandle_, 0);
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_05: dstThread = 0 (null) -> HCCL_E_PTR
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_DstThreadIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res_ = HcommThreadNotifyRecordOnThread(tsHandle_, 0, 0);
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_06: CpuThread -> CpuThread (unsupported) -> HCCL_E_NOT_SUPPORT
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_TypeCombinationNotSupported_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    RecordThreadEntityBuffer cpuBuf2;
    memset(&cpuBuf2, 0, sizeof(cpuBuf2));
    cpuBuf2.entity.type = THREAD_TYPE_CPU;
    cpuBuf2.entity.engine = COMM_ENGINE_AICPU;
    cpuBuf2.entity.notifyNum = 1;

    ThreadHandle cpuHandle2 = reinterpret_cast<ThreadHandle>(&cpuBuf2.entity);
    res_ = HcommThreadNotifyRecordOnThread(cpuHandle_, cpuHandle2, 0);
    EXPECT_EQ(res_, HCCL_E_NOT_SUPPORT);
}

// test_07: AicpuTs -> AicpuTs, dstNotifyIdx out of range -> HCCL_E_PARA
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsToAicpuTsDstNotifyIdxOutOfRange_Expect_ReturnIsHCCL_E_PARA)
{
    res_ = HcommThreadNotifyRecordOnThread(tsHandle_, dstTsHandle_, 1);
    EXPECT_EQ(res_, HCCL_E_PARA);
}

// test_08: AicpuTs -> Cpu, dstNotifyIdx out of range -> HCCL_E_PARA
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsToCpuDstNotifyIdxOutOfRange_Expect_ReturnIsHCCL_E_PARA)
{
    // cpuBuf_.entity.notifyNum == 1, so index 1 is out of range
    res_ = HcommThreadNotifyRecordOnThread(tsHandle_, cpuHandle_, 1);
    EXPECT_EQ(res_, HCCL_E_PARA);
}

// test_09: AicpuTs -> Cpu, non-A5 device -> HCCL_E_NOT_SUPPORT
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsToCpuOnNonA5Device_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    AicpuTsThread nonA5Thread{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    nonA5Thread.devType_ = DevType::DEV_TYPE_910_93;
    nonA5Thread.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();

    RecordThreadEntityBuffer nonA5Buf;
    memset(&nonA5Buf, 0, sizeof(nonA5Buf));
    nonA5Buf.entity.type = THREAD_TYPE_TS;
    nonA5Buf.entity.engine = COMM_ENGINE_AICPU;
    nonA5Buf.entity.threadObjAddr = reinterpret_cast<uint64_t>(&nonA5Thread);
    nonA5Buf.entity.notifyNum = 1;

    ThreadHandle nonA5Handle = reinterpret_cast<ThreadHandle>(&nonA5Buf.entity);
    res_ = HcommThreadNotifyRecordOnThread(nonA5Handle, cpuHandle_, 0);
    EXPECT_EQ(res_, HCCL_E_NOT_SUPPORT);
}

// test_10: CpuThread -> AicpuTs, non-A5 device -> HCCL_E_NOT_SUPPORT
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_CpuToAicpuTsOnNonA5Device_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    AicpuTsThread nonA5DstThread{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    nonA5DstThread.devType_ = DevType::DEV_TYPE_910_93;
    nonA5DstThread.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();

    RecordThreadEntityBuffer nonA5DstBuf;
    memset(&nonA5DstBuf, 0, sizeof(nonA5DstBuf));
    nonA5DstBuf.entity.type = THREAD_TYPE_TS;
    nonA5DstBuf.entity.engine = COMM_ENGINE_AICPU;
    nonA5DstBuf.entity.threadObjAddr = reinterpret_cast<uint64_t>(&nonA5DstThread);
    nonA5DstBuf.entity.notifyNum = 1;

    ThreadHandle nonA5DstHandle = reinterpret_cast<ThreadHandle>(&nonA5DstBuf.entity);
    res_ = HcommThreadNotifyRecordOnThread(cpuHandle_, nonA5DstHandle, 0);
    EXPECT_EQ(res_, HCCL_E_NOT_SUPPORT);
}

// test_11: AicpuTs -> AicpuTs, LocalNotifyRecord throws exception -> HCCL_E_INTERNAL
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsToAicpuTsLocalNotifyRecordThrowException_Expect_ReturnIsHCCL_E_INTERNAL)
{
    MOCKER_CPP(&Hccl::IAicpuTsThread::NotifyRecordLoc).stubs().will(throws(std::runtime_error("mock exception")));
    res_ = HcommThreadNotifyRecordOnThread(tsHandle_, dstTsHandle_, 0);
    EXPECT_EQ(res_, HCCL_E_INTERNAL);
}

// test_12: AicpuTs -> Cpu, WriteValue fails -> error propagated
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsToCpuWriteValueFails_Expect_ReturnIsHCCL_E_INTERNAL)
{
    MOCKER_CPP(&Hccl::IAicpuTsThread::WriteValue).stubs().will(returnValue(HCCL_E_INTERNAL));
    res_ = HcommThreadNotifyRecordOnThread(tsHandle_, cpuHandle_, 0);
    EXPECT_EQ(res_, HCCL_E_INTERNAL);
}
