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
struct SvcThreadEntityBuffer {
    ThreadEntity entity;
    NotifyEntity notifiesData[1];
};

static const uint32_t kSvcQueueCapacity = 4;
static const ThreadServiceHandle kValidService = 0x300;

// Dummy args struct for testing
struct TestServiceArgs {
    uint32_t param1;
    uint64_t param2;
};

class UtAicpuTsHcommRequestServiceOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        headIdx_ = 0;
        tailIdx_ = 0;
        memset(msgQueue_, 0, sizeof(msgQueue_));

        memset(&cpuBuf_, 0, sizeof(cpuBuf_));
        cpuBuf_.entity.type = THREAD_TYPE_CPU;
        cpuBuf_.entity.engine = COMM_ENGINE_AICPU;
        cpuBuf_.entity.cpuRes.sendQueue.addr = reinterpret_cast<uint64_t>(msgQueue_);
        cpuBuf_.entity.cpuRes.sendQueue.headIdxAddr = reinterpret_cast<uint64_t>(&headIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.tailIdxAddr = reinterpret_cast<uint64_t>(&tailIdx_);
        cpuBuf_.entity.cpuRes.sendQueue.msgSize = sizeof(ThreadMsgEntity);
        cpuBuf_.entity.cpuRes.sendQueue.capacity = kSvcQueueCapacity;
        cpuBuf_.entity.notifyNum = 0;

        testArgs_.param1 = 42;
        testArgs_.param2 = 0xDEADBEEF;
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
        // Free malloc'd args written to the queue by HcommRequestServiceOnThread
        for (uint64_t i = headIdx_; i != tailIdx_; i = (i + 1) % kSvcQueueCapacity) {
            if (msgQueue_[i].args != nullptr) {
                free(msgQueue_[i].args);
                msgQueue_[i].args = nullptr;
            }
        }
    }

    SvcThreadEntityBuffer cpuBuf_;
    uint64_t headIdx_ = 0;
    uint64_t tailIdx_ = 0;
    ThreadMsgEntity msgQueue_[kSvcQueueCapacity];
    TestServiceArgs testArgs_;

    int32_t res_{HCCL_E_RESERVED};
};

// test_01: normal case - args copied to heap, enqueued, msgId incremented -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommRequestServiceOnThread,
    Ut_HcommRequestServiceOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommRequestServiceOnThread(cpuHandle, kValidService, &testArgs_, sizeof(testArgs_));
    EXPECT_EQ(res_, HCCL_SUCCESS);
    // Verify queue advanced
    EXPECT_EQ(tailIdx_, 1u);
    // Verify args were copied to heap
    EXPECT_NE(msgQueue_[0].args, nullptr);
    EXPECT_EQ(msgQueue_[0].serviceHandle, kValidService);
    TestServiceArgs *copiedArgs = reinterpret_cast<TestServiceArgs *>(msgQueue_[0].args);
    EXPECT_EQ(copiedArgs->param1, testArgs_.param1);
    EXPECT_EQ(copiedArgs->param2, testArgs_.param2);
}

// test_02: dstThread = 0 (null) -> HCCL_E_PTR
TEST_F(UtAicpuTsHcommRequestServiceOnThread,
    Ut_HcommRequestServiceOnThread_When_DstThreadIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res_ = HcommRequestServiceOnThread(0, kValidService, &testArgs_, sizeof(testArgs_));
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_03: serviceHandle = 0 (null) -> HCCL_E_PTR
TEST_F(UtAicpuTsHcommRequestServiceOnThread,
    Ut_HcommRequestServiceOnThread_When_ServiceHandleIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommRequestServiceOnThread(cpuHandle, 0, &testArgs_, sizeof(testArgs_));
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_04: args = nullptr -> HCCL_E_PTR
TEST_F(UtAicpuTsHcommRequestServiceOnThread,
    Ut_HcommRequestServiceOnThread_When_ArgsIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommRequestServiceOnThread(cpuHandle, kValidService, nullptr, 0);
    EXPECT_EQ(res_, HCCL_E_PTR);
}

// test_05: dstThread type = TS (unsupported) -> HCCL_E_NOT_SUPPORT
TEST_F(UtAicpuTsHcommRequestServiceOnThread,
    Ut_HcommRequestServiceOnThread_When_ThreadTypeIsTs_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    SvcThreadEntityBuffer tsBuf;
    memset(&tsBuf, 0, sizeof(tsBuf));
    tsBuf.entity.type = THREAD_TYPE_TS;
    tsBuf.entity.engine = COMM_ENGINE_AICPU;

    ThreadHandle tsHandle = reinterpret_cast<ThreadHandle>(&tsBuf.entity);
    res_ = HcommRequestServiceOnThread(tsHandle, kValidService, &testArgs_, sizeof(testArgs_));
    EXPECT_EQ(res_, HCCL_E_NOT_SUPPORT);
}

// test_06: sendQueue full ((tail+1)%capacity == head) -> HCCL_E_INTERNAL
TEST_F(UtAicpuTsHcommRequestServiceOnThread,
    Ut_HcommRequestServiceOnThread_When_SendQueueIsFull_Expect_ReturnIsHCCL_E_INTERNAL)
{
    // capacity=4, set head=0, tail=3: nextTail = (3+1)%4 = 0 = head -> full
    headIdx_ = 0;
    tailIdx_ = kSvcQueueCapacity - 1;

    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommRequestServiceOnThread(cpuHandle, kValidService, &testArgs_, sizeof(testArgs_));
    EXPECT_EQ(res_, HCCL_E_INTERNAL);
}

// test_07: unregistered but non-null serviceHandle - message written normally -> HCCL_SUCCESS
TEST_F(UtAicpuTsHcommRequestServiceOnThread,
    Ut_HcommRequestServiceOnThread_When_UnregisteredService_Expect_ReturnIsHCCL_SUCCESS)
{
    const ThreadServiceHandle unregisteredService = 0xABCD;
    ThreadHandle cpuHandle = reinterpret_cast<ThreadHandle>(&cpuBuf_.entity);
    res_ = HcommRequestServiceOnThread(cpuHandle, unregisteredService, &testArgs_, sizeof(testArgs_));
    EXPECT_EQ(res_, HCCL_SUCCESS);
    // Message was enqueued despite unregistered handle (detection happens in CpuThread::KernelRun)
    EXPECT_EQ(tailIdx_, 1u);
    EXPECT_EQ(msgQueue_[0].serviceHandle, unregisteredService);
}
