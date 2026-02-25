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

class UtAicpuTsHcommThreadNotifyRecordOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        MOCKER_CPP(&Hccl::IAicpuTsThread::NotifyRecordLoc).stubs().will(returnValue(HCCL_SUCCESS));
        threadOnDevice.devType_ = DevType::DEV_TYPE_910_95;
        threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        InitNotifies(threadOnDevice);
        dstThreadOnDevice.devType_ = DevType::DEV_TYPE_910_95;
        dstThreadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        InitNotifies(dstThreadOnDevice);
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    AicpuTsThread dstThreadOnDevice{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    ThreadHandle dstThread = reinterpret_cast<ThreadHandle>(&dstThreadOnDevice);
    uint32_t notifyIdx = 0;
    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread, Ut_HcommThreadNotifyRecordOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    res = HcommThreadNotifyRecordOnThread(thread, dstThread, notifyIdx);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread, Ut_HcommThreadNotifyRecordOnThread_When_Thread_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommThreadNotifyRecordOnThread(0, dstThread, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread, Ut_HcommThreadNotifyRecordOnThread_When_DstThread_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommThreadNotifyRecordOnThread(thread, 0, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommThreadNotifyRecordOnThread, Ut_HcommThreadNotifyRecordOnThread_When_Notify_NotFound_Expect_ReturnIsHCCL_E_PTR)
{
    notifyIdx = 1;
    res = HcommThreadNotifyRecordOnThread(thread, dstThread, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}