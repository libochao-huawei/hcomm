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
#undef private

using namespace hccl;

class UtAicpuTsHcommLocalCopyOnThread : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp() override
    {
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    threadOnDevice.devType_ = DevType::DEV_TYPE_910_95;
    threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(&tempDst);
    void *src = reinterpret_cast<void *>(&tempSrc);
    uint64_t len = sizeof(tempDst);

    int32_t res = HCCL_E_RESERVED;

    MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().with(any(), any(), any()).will(returnValue(HCCL_SUCCESS));

    res = HcommLocalCopyOnThread(thread, dst, src, len);

    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Thread_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(&tempDst);
    void *src = reinterpret_cast<void *>(&tempSrc);
    uint64_t len = sizeof(tempDst);

    int32_t res = HCCL_E_RESERVED;

    MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().with(any(), any(), any()).will(returnValue(HCCL_SUCCESS));

    res = HcommLocalCopyOnThread(nullptr, dst, src, len);

    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Dst_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    threadOnDevice.devType_ = DevType::DEV_TYPE_910_95;
    threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *src = reinterpret_cast<void *>(&tempSrc);
    uint64_t len = sizeof(tempDst);

    int32_t res = HCCL_E_RESERVED;

    MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().with(any(), any(), any()).will(returnValue(HCCL_SUCCESS));

    res = HcommLocalCopyOnThread(thread, nullptr, src, len);

    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Dst_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    threadOnDevice.devType_ = DevType::DEV_TYPE_910_95;
    threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    uint64_t tempDst[6] = {0};
    void *dst = reinterpret_cast<void *>(&tempDst);
    uint64_t len = sizeof(tempDst);

    int32_t res = HCCL_E_RESERVED;

    MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().with(any(), any(), any()).will(returnValue(HCCL_SUCCESS));

    res = HcommLocalCopyOnThread(thread, dst, nullptr, len);

    EXPECT_EQ(res, HCCL_E_PTR);
}