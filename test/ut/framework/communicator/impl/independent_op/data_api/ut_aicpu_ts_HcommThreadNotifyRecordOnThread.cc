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
        std::cout << "A Test case in UtAicpuTsHcommThreadNotifyRecordOnThread SetUp" << std::endl;
        UtAicpuTsBase::SetUp();

        GlobalMockObject::reset();

        dstThreadOnDevice = std::make_unique<hccl::AicpuTsThread>(StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY);
        bool isDeviceSide = false;
        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(isDeviceSide))
            .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(DevType::DEV_TYPE_950))
            .will(returnValue(HCCL_SUCCESS));

        HcclResult ret = dstThreadOnDevice->Init();
        EXPECT_EQ(ret, HCCL_SUCCESS);
        std::string mainStr = dstThreadOnDevice->GetUniqueId();

        GlobalMockObject::reset();

        isDeviceSide = true;
        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(isDeviceSide))
            .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(DevType::DEV_TYPE_950))
            .will(returnValue(HCCL_SUCCESS));

        slaveDevThread = std::make_unique<hccl::AicpuTsThread>(mainStr);
        ret = slaveDevThread->Init();
        dstThread = reinterpret_cast<ThreadHandle>(slaveDevThread.get());

        MOCKER_CPP(&Hccl::IAicpuTsThread::NotifyRecordLoc, HcclResult (Hccl::IAicpuTsThread::*)(uint32_t) const).stubs().will(returnValue(HCCL_SUCCESS));

        // ---- TS thread (src for TS->TS and TS->CPU tests) ----
        tsThread_.devType_ = DevType::DEV_TYPE_950;
        tsThread_.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        InitNotifies(tsThread_);

        memset(&tsBuf_, 0, sizeof(tsBuf_));
        tsBuf_.entity.type = THREAD_TYPE_TS;
        tsBuf_.entity.engine = COMM_ENGINE_AICPU;
        tsBuf_.entity.threadObjAddr = reinterpret_cast<uint64_t>(&tsThread_);
        tsBuf_.entity.notifyNum = 1;

        // ---- dst TS thread (for TS->TS tests) ----
        dstTsThread_.devType_ = DevType::DEV_TYPE_950;
        dstTsThread_.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        InitNotifies(dstTsThread_);

        memset(&dstTsBuf_, 0, sizeof(dstTsBuf_));
        dstTsBuf_.entity.type = THREAD_TYPE_TS;
        dstTsBuf_.entity.engine = COMM_ENGINE_AICPU;
        dstTsBuf_.entity.threadObjAddr = reinterpret_cast<uint64_t>(&dstTsThread_);
        dstTsBuf_.entity.notifyNum = 1;

        // ---- CPU thread (for TS->CPU and CPU->TS tests) ----
        headIdx_ = 0;
        tailIdx_ = 0;
        memset(msgQueue_, 0, sizeof(msgQueue_));

        memset(&cpuBuf_, 0, sizeof(cpuBuf_));
        cpuBuf_.entity.type = THREAD_TYPE_CPU;
        cpuBuf_.entity.engine = COMM_ENGINE_AICPU;
        cpuBuf_.entity.cpuRes.recordService = kValidRecordService;
        cpuBuf_.entity.cpuRes.sendQueue.addr = reinterpret_cast<uint64_t>(msgQueue_);
        cpuBuf_.entity.cpuRes.sendQueue.headIdxAddr = reinterpret_cast<uint64_t>(&headIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.tailIdxAddr = reinterpret_cast<uint64_t>(&tailIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.msgSize = sizeof(ThreadMsgEntity);
        cpuBuf_.entity.cpuRes.sendQueue.capacity = kQueueCapacity;
        cpuBuf_.entity.notifyNum = 1;
        cpuBuf_.notifiesData[0].type = NOTIFY_TYPE_DEVICE_MEM;
        cpuBuf_.notifiesData[0].identifier = kDeviceMemAddr;
    }

    virtual void TearDown() override
    {
        UtAicpuTsBase::TearDown();
        for (uint64_t i = headIdx_; i != tailIdx_; i = (i + 1) % kQueueCapacity) {
            if (msgQueue_[i].args != nullptr) {
                free(msgQueue_[i].args);
                msgQueue_[i].args = nullptr;
            }
        }
        std::cout << "A Test case in UtAicpuTsHcommThreadNotifyRecordOnThread TearDown" << std::endl;
    }

    std::unique_ptr<hccl::AicpuTsThread> dstThreadOnDevice;
    std::unique_ptr<hccl::AicpuTsThread> slaveDevThread;
    ThreadHandle dstThread;

    uint32_t notifyIdx = 0;
    int32_t res{HCCL_E_RESERVED};

    // TS threads
    AicpuTsThread tsThread_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    AicpuTsThread dstTsThread_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    RecordThreadEntityBuffer tsBuf_;
    RecordThreadEntityBuffer dstTsBuf_;

    // CPU thread
    RecordThreadEntityBuffer cpuBuf_;
    uint64_t headIdx_ = 0;
    uint64_t tailIdx_ = 0;
    ThreadMsgEntity msgQueue_[kQueueCapacity];

    ThreadHandle tsHandle_ = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    ThreadHandle dstTsHandle_ = reinterpret_cast<ThreadHandle>(&dstTsBuf_.entity);
    ThreadHandle cpuHandle_ = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);

    int32_t res_{HCCL_E_RESERVED};
};

// test_01: AicpuTs -> AicpuTs, normal -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsNotifyAicpuTsNormal_Expect_ReturnIsHCCL_SUCCESS)
{
    res = HcommThreadNotifyRecordOnThread(thread, dstThread, notifyIdx);
    EXPECT_EQ(res, HCCL_SUCCESS);
    // TODO:
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
