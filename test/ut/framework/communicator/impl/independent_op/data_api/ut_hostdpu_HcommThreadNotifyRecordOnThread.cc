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
struct ThreadEntityBuffer {
    ThreadEntity entity;
    NotifyEntity notifiesData[2];
};

// Queue backing memory for CPU thread sendQueue tests
static const uint32_t kQueueCapacity = 4;
static const ThreadServiceHandle kValidRecordService = 0x100;
static const uint64_t kDeviceMemAddr = 0x8000;

class UtHostDpuHcommThreadNotifyRecordOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        // TS thread: AicpuTsThread object + ThreadEntity pointing to it
        tsThread_.devType_ = DevType::DEV_TYPE_910_95;
        tsThread_.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();

        memset(&tsBuf_, 0, sizeof(tsBuf_));
        tsBuf_.entity.type = THREAD_TYPE_TS;
        tsBuf_.entity.engine = COMM_ENGINE_AICPU;
        tsBuf_.entity.threadObjAddr = reinterpret_cast<uint64_t>(&tsThread_);
        tsBuf_.entity.notifyNum = 1;

        // CPU thread: ThreadEntity with sendQueue and cpuRes
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
        GlobalMockObject::verify();
        // Free any malloc'd args that HcommRequestServiceOnThread wrote to the queue
        for (uint64_t i = headIdx_; i != tailIdx_; i = (i + 1) % kQueueCapacity) {
            if (msgQueue_[i].args != nullptr) {
                free(msgQueue_[i].args);
                msgQueue_[i].args = nullptr;
            }
        }
    }

    AicpuTsThread tsThread_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    ThreadEntityBuffer tsBuf_;
    ThreadEntityBuffer cpuBuf_;

    uint64_t headIdx_ = 0;
    uint64_t tailIdx_ = 0;
    ThreadMsgEntity msgQueue_[kQueueCapacity];

    int32_t res_{HCCL_E_RESERVED};
};

// test_01: AicpuTsThread -> CpuThread, NotifyRecord succeeds -> HCCL_SUCCESS
TEST_F(UtHostDpuHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_AicpuTsNotifyCpuThread_Expect_ReturnIsHCCL_SUCCESS)
{
    MOCKER_CPP(&Hccl::IAicpuTsThread::WriteValue).stubs().will(returnValue(HCCL_SUCCESS));

    ThreadHandle tsHandle = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommThreadNotifyRecordOnThread(tsHandle, cpuHandle, 0);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_02: CpuThread -> AicpuTsThread, requests RecordService -> HCCL_SUCCESS
TEST_F(UtHostDpuHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_CpuThreadNotifyAicpuTs_Expect_ReturnIsHCCL_SUCCESS)
{
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    ThreadHandle tsHandle = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    res_ = HcommThreadNotifyRecordOnThread(cpuHandle, tsHandle, 0);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_03: thread = 0 (null) -> HCCL_E_PTR
TEST_F(UtHostDpuHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_ThreadIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommThreadNotifyRecordOnThread(0, cpuHandle, 0);
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_04: dstThread = 0 (null) -> HCCL_E_PTR
TEST_F(UtHostDpuHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_DstThreadIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    ThreadHandle tsHandle = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    res_ = HcommThreadNotifyRecordOnThread(tsHandle, 0, 0);
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_05: CpuThread -> CpuThread (unsupported) -> HCCL_E_NOT_SUPPORT
TEST_F(UtHostDpuHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_TypeCombinationNotSupported_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    ThreadEntityBuffer cpuBuf2;
    memset(&cpuBuf2, 0, sizeof(cpuBuf2));
    cpuBuf2.entity.type = THREAD_TYPE_CPU;
    cpuBuf2.entity.engine = COMM_ENGINE_AICPU;
    cpuBuf2.entity.notifyNum = 1;

    ThreadHandle cpuHandle1 = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    ThreadHandle cpuHandle2 = reinterpret_cast<ThreadHandle>(&cpuBuf2.entity);
    res_ = HcommThreadNotifyRecordOnThread(cpuHandle1, cpuHandle2, 0);
    EXPECT_EQ(res_, HCCL_E_NOT_SUPPORT);
}

// test_06: dstNotifyIdx = 1 exceeds notifyNum=1 -> HCCL_E_PARA
TEST_F(UtHostDpuHcommThreadNotifyRecordOnThread,
    Ut_HcommThreadNotifyRecordOnThread_When_DstNotifyIdxOutOfRange_Expect_ReturnIsHCCL_E_PARA)
{
    ThreadHandle tsHandle = reinterpret_cast<ThreadHandle>(&tsBuf_.entity);
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    // cpuBuf_.entity.notifyNum == 1, so index 1 is out of range
    res_ = HcommThreadNotifyRecordOnThread(tsHandle, cpuHandle, 1);
    EXPECT_EQ(res_, HCCL_E_PARA);
}
