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

#include "resource_entities.h"
#include "hccl_api_data.h"

using namespace hccl;

// Buffer type that allocates ThreadEntity + trailing NotifyEntity array
struct WaitThreadEntityBuffer {
    ThreadEntity entity;
    NotifyEntity notifiesData[2];
};

static const uint32_t kWaitQueueCapacity = 4;
static const ThreadServiceHandle kValidWaitService = 0x200;

class UtHostDpuHcommThreadNotifyWaitOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        headIdx_ = 0;
        tailIdx_ = 0;
        memset(msgQueue_, 0, sizeof(msgQueue_));

        // CPU thread entity with valid sendQueue
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

    WaitThreadEntityBuffer cpuBuf_;
    uint64_t headIdx_ = 0;
    uint64_t tailIdx_ = 0;
    ThreadMsgEntity msgQueue_[kWaitQueueCapacity];

    int32_t res_{HCCL_E_RESERVED};
};

// test_01: CpuThread normal wait, WaitService enqueued -> HCCL_SUCCESS
TEST_F(UtHostDpuHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_CpuThreadWaitsNormal_Expect_ReturnIsHCCL_SUCCESS)
{
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommThreadNotifyWaitOnThread(cpuHandle, 0, 1800);
    EXPECT_EQ(res_, HCCL_SUCCESS);
}

// test_02: thread = 0 (null) -> HCCL_E_PTR
TEST_F(UtHostDpuHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_ThreadIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res_ = HcommThreadNotifyWaitOnThread(0, 0, 1800);
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_03: notifyIdx=1 exceeds notifyNum=1 -> HCCL_E_PARA
TEST_F(UtHostDpuHcommThreadNotifyWaitOnThread,
    Ut_HcommThreadNotifyWaitOnThread_When_NotifyIdxOutOfRange_Expect_ReturnIsHCCL_E_PARA)
{
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    // notifyNum == 1, so index 1 is out of range
    res_ = HcommThreadNotifyWaitOnThread(cpuHandle, 1, 1800);
    EXPECT_EQ(res_, HCCL_E_PARA);
}

// test_04: unsupported thread type -> HCCL_E_NOT_SUPPORT
TEST_F(UtHostDpuHcommThreadNotifyWaitOnThread,
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
