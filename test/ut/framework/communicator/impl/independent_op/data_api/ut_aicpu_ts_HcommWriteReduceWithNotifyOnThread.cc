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
#include "ub_transport_lite_impl.h"
#undef private

using namespace hccl;

class UtAicpuTsHcommWriteReduceWithNotifyOnThread : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        threadOnDevice.devType_ = DevType::DEV_TYPE_950;
        threadOnDevice.pImpl_ = std::make_unique<Hccl::IAicpuTsThread>();
        threadOnDevice.pImpl_->streamLiteVoidPtr_ = reinterpret_cast<void *>(0x123456);
        MOCKER_CPP(&Hccl::UbTransportLiteImpl::BuildLocRmaBufferLite).stubs().will(returnValue(HCCL_SUCCESS));
    }

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }

    AicpuTsThread threadOnDevice{StreamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<ThreadHandle>(&threadOnDevice);
    std::vector<char> uniqueId;
    Hccl::UbTransportLiteImpl transportOnDevice{uniqueId};
    ChannelHandle channel = reinterpret_cast<ChannelHandle>(&transportOnDevice);
    uint64_t tempDst[6] = {1, 1, 1, 1, 1, 1};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(tempDst);
    void *src = reinterpret_cast<void *>(tempSrc);
    uint64_t len = sizeof(tempDst);
    uint64_t count = len / sizeof(tempDst[0]);
    HcommDataType dataType = HCOMM_DATA_TYPE_FP16;
    HcommReduceOp reduceOp = HCOMM_REDUCE_SUM;
    uint32_t notifyIdx = 0;
    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    res = HcommWriteReduceWithNotifyOnThread(thread, channel, dst, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_SUCCESS);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_Thread_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommWriteReduceWithNotifyOnThread(0, channel, dst, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_Channel_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommWriteReduceWithNotifyOnThread(thread, 0, dst, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_Dst_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommWriteReduceWithNotifyOnThread(thread, channel, 0, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_Src_IsNull_Expect_ReturnIsHCCL_E_PTR)
{
    res = HcommWriteReduceWithNotifyOnThread(thread, channel, dst, 0, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_StreamLite_NotFound_Expect_ReturnIsHCCL_E_PTR)
{
    threadOnDevice.pImpl_->streamLiteVoidPtr_ = nullptr;
    res = HcommWriteReduceWithNotifyOnThread(thread, channel, dst, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_BuildLocRmaBufferLite_Fail_Expect_ReturnIsHCCL_E_INTERNAL)
{
    GlobalMockObject::verify();
    MOCKER_CPP(&Hccl::UbTransportLiteImpl::BuildLocRmaBufferLite).stubs().will(returnValue(HCCL_E_INTERNAL));
    res = HcommWriteReduceWithNotifyOnThread(thread, channel, dst, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_INTERNAL);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_DataType_IsInvalid_Expect_ReturnIsHCCL_E_PARA)
{
    dataType = HCOMM_DATA_TYPE_RESERVED;
    res = HcommWriteReduceWithNotifyOnThread(thread, channel, dst, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PARA);
}

TEST_F(UtAicpuTsHcommWriteReduceWithNotifyOnThread, Ut_HcommWriteReduceWithNotifyOnThread_When_ReduceOp_IsInvalid_Expect_ReturnIsHCCL_E_PARA)
{
    reduceOp = HCOMM_REDUCE_RESERVED;
    res = HcommWriteReduceWithNotifyOnThread(thread, channel, dst, src, count, dataType, reduceOp, notifyIdx);
    EXPECT_EQ(res, HCCL_E_PARA);
}