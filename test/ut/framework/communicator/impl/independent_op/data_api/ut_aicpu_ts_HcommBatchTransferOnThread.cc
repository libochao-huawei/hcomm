/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_aicpu_ts_base.h"
#include "ub_transport_lite_impl.h"
#include "transport_pub.h"

using namespace hccl;

class UtAicpuTsHcommBatchTransferOnThread : public UtAicpuTsBase
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "UtAicpuTsHcommBatchTransferOnThread tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UtAicpuTsHcommBatchTransferOnThread tests tear down." << std::endl;
    }

    virtual void SetUp() override
    {
        std::cout << "A Test case in UtAicpuTsHcommBatchTransferOnThread SetUp" << std::endl;
        UtAicpuTsBase::SetUp();
    }

    virtual void TearDown() override
    {
        UtAicpuTsBase::TearDown();
        std::cout << "A Test case in UtAicpuTsHcommBatchTransferOnThread TearDown" << std::endl;
    }

    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(tempDst);
    void *src = reinterpret_cast<void *>(tempSrc);
    uint64_t len = sizeof(tempDst);
    std::vector<char> uniqueId;
    Hccl::UbTransportLiteImpl transportDev{uniqueId};
    ChannelHandle devHandle = reinterpret_cast<ChannelHandle>(&transportDev);
    int32_t res{HCCL_E_RESERVED};
    HcommBatchTransferDesc transferDescs[2];
};

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_A5Path_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_ThreadIsNull_Expect_ReturnHCCL_E_PTR)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(0, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_DescsIsNull_Expect_ReturnHCCL_E_PTR)
{
    res = HcommBatchTransferOnThread(thread, devHandle, nullptr, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_DescNumIsZero_Expect_ReturnHCCL_E_PARA)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 0);
    EXPECT_EQ(res, HCCL_E_PARA);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_ReduceOpSet_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_SUM;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_DataTypeSet_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_FP32;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_InvalidTransType_Expect_ReturnHCCL_E_PARA)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_INVALID;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_PARA);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_DstIsNull_Expect_ReturnHCCL_E_PTR)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = nullptr;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_SrcIsNull_Expect_ReturnHCCL_E_PTR)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = nullptr;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 1);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransferOnThread, Ut_HcommBatchTransferOnThread_When_MultipleDescs_Expect_ReturnHCCL_E_NOT_SUPPORT)
{
    transferDescs[0].len = len;
    transferDescs[0].dst = dst;
    transferDescs[0].src = src;
    transferDescs[0].transType = HCOMM_TRANSFER_TYPE_WRITE;
    transferDescs[0].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[0].dataType = HCOMM_DATA_TYPE_RESERVED;

    transferDescs[1].len = len;
    transferDescs[1].dst = dst;
    transferDescs[1].src = src;
    transferDescs[1].transType = HCOMM_TRANSFER_TYPE_READ;
    transferDescs[1].reduceOp = HCOMM_REDUCE_RESERVED;
    transferDescs[1].dataType = HCOMM_DATA_TYPE_RESERVED;

    res = HcommBatchTransferOnThread(thread, devHandle, transferDescs, 2);
    EXPECT_EQ(res, HCCL_E_NOT_SUPPORT);
}