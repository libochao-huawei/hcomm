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
#include "ub_transport_lite_impl.h"

#define private public
#include "aicpu_ts_thread.h"
#include "aicpu_ts_thread_interface.h"
#undef private

using namespace hccl;

class UtAicpuTsHcommReadReduceOnThreadTest : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        MOCKER_CPP(&Hccl::UbTransportLiteImpl::BuildLocRmaBufferLite)
            .stubs()
            .with(any(), any(), any())
            .will(returnValue(HCCL_SUCCESS));
        threadOnDevice.devType_ = DevType::DEV_TYPE_950;
        threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        threadOnDevice.pImpl_->streamLiteVoidPtr_ = reinterpret_cast<void *>(0x123);
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

private:
    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(tempDst);
    void *src = reinterpret_cast<void *>(tempSrc);
    std::vector<char> uniqueId;
    Hccl::UbTransportLiteImpl transportDev{uniqueId};
    ChannelHandle devHandle = reinterpret_cast<ChannelHandle>(&transportDev);
    uint64_t count = 1;
};

TEST_F(UtAicpuTsHcommReadReduceOnThreadTest, Ut_HcommReadReduceOnThread_When_buffer_not_find_Expect_HCCL_E_INTERNAL)
{
    // 前置条件
    GlobalMockObject::verify();
    MOCKER_CPP(&Hccl::UbTransportLiteImpl::BuildLocRmaBufferLite)
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(HCCL_E_INTERNAL));

    // 执行步骤
    auto res = HcommReadReduceOnThread(thread, devHandle, dst, src, count, HCOMM_DATA_TYPE_INT8, HCOMM_REDUCE_SUM);

    // 后置验证
    EXPECT_EQ(res, HCCL_E_INTERNAL);
}

TEST_F(UtAicpuTsHcommReadReduceOnThreadTest, Ut_HcommReadReduceOnThread_When_check_fail_Expect_HCCL_E_PARA)
{
    // 执行步骤
    auto res = HcommReadReduceOnThread(thread, devHandle, dst, src, count, HCOMM_DATA_TYPE_RESERVED, HCOMM_REDUCE_SUM);

    // 后置验证
    EXPECT_EQ(res, HCCL_E_PARA);
}

TEST_F(UtAicpuTsHcommReadReduceOnThreadTest, Ut_HcommReadReduceOnThread_When_thread_nullptr_Expect_HCCL_E_PTR)
{
    // 执行步骤
    auto res = HcommReadReduceOnThread(0, devHandle, dst, src, count, HCOMM_DATA_TYPE_INT8, HCOMM_REDUCE_SUM);

    // 后置验证
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommReadReduceOnThreadTest, Ut_HcommReadReduceOnThread_When_normal_Expect_HCCL_SUCCESS)
{
    // 执行步骤
    auto res = HcommReadReduceOnThread(thread, devHandle, dst, src, count, HCOMM_DATA_TYPE_INT8, HCOMM_REDUCE_SUM);

    // 后置验证
    EXPECT_EQ(res, HCCL_SUCCESS);
}