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
#include "dispatcher_aicpu.h"
#undef private

#include "resource_entities.h"

using namespace hccl;

class UtAicpuTsHcommLocalCopyOnThread : public testing::Test
{
protected:
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

        GlobalMockObject::verify();

        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(true))
            .will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(DevType::DEV_TYPE_950))
            .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Hccl::IAicpuTsThread::SdmaCopy).stubs().will(returnValue(HCCL_SUCCESS));

        tsThreadDevPtr_ = std::make_unique<AicpuTsThread>(packedData);
        ret = tsThreadDevPtr_->Init();
        ASSERT_EQ(ret, HCCL_SUCCESS);
        ret = tsThreadDevPtr_->SetAddTaskInfoCallback(callback_);
        ASSERT_EQ(ret, HCCL_SUCCESS);
        ASSERT_NE(tsThreadDevPtr_->GetStreamLitePtr(), nullptr);

        memset(&threadEntity_, 0, sizeof(threadEntity_));
        threadEntity_.type = THREAD_TYPE_TS;
        threadEntity_.engine = COMM_ENGINE_AICPU;
        threadEntity_.threadObjAddr = reinterpret_cast<uint64_t>(tsThreadDevPtr_.get());
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    AicpuTsThread tsThreadHost_{StreamType::STREAM_TYPE_DEVICE, 1, NotifyLoadType::DEVICE_NOTIFY};
    std::unique_ptr<AicpuTsThread> tsThreadDevPtr_;
    using FuncCb = std::function<HcclResult (u32, u32, const Hccl::TaskParam &, u64)>;
    FuncCb callback_ = [](u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle) {return HCCL_SUCCESS;};
    ThreadEntity threadEntity_{};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadEntity_);
    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(tempDst);
    void *src = reinterpret_cast<void *>(tempSrc);
    uint64_t len = sizeof(tempDst);
    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
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
