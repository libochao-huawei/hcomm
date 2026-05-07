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

using namespace hccl;

class UtAicpuTsHcommBatchTransfer : public UtAicpuTsBase
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "UtAicpuTsHcommBatchTransfer tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UtAicpuTsHcommBatchTransfer tests tear down." << std::endl;
    }

    virtual void SetUp() override
    {
        std::cout << "A Test case in UtAicpuTsHcommBatchTransfer SetUp" << std::endl;
        UtAicpuTsBase::SetUp();
    }

    virtual void TearDown() override
    {
        UtAicpuTsBase::TearDown();
        std::cout << "A Test case in UtAicpuTsHcommBatchTransfer TearDown" << std::endl;
    }

    int32_t res{HCCL_E_RESERVED};
};

TEST_F(UtAicpuTsHcommBatchTransfer, Ut_HcommBatchTransfer_When_DstList_IsNull_Expect_PtrError)
{
    void *dstList = nullptr;
    const void *srcList = nullptr;
    uint64_t lenList[2] = {64, 128};
    uint32_t rw[2] = {0, 1};
    HcommDataType dataType[2] = {HCOMM_DATA_TYPE_FP32, HCOMM_DATA_TYPE_FP16};
    HcommReduceOp reduceOp[2] = {HCOMM_REDUCE_SUM, HCOMM_REDUCE_SUM};
    uint32_t count = 2;

    res = HcommBatchTransfer(thread, 0, dstList, srcList, lenList, rw, dataType, reduceOp, count);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransfer, Ut_HcommBatchTransfer_When_SrcList_IsNull_Expect_PtrError)
{
    void *dstList = reinterpret_cast<void *>(0x1000);
    const void *srcList = nullptr;
    uint64_t lenList[2] = {64, 128};
    uint32_t rw[2] = {0, 1};
    HcommDataType dataType[2] = {HCOMM_DATA_TYPE_FP32, HCOMM_DATA_TYPE_FP16};
    HcommReduceOp reduceOp[2] = {HCOMM_REDUCE_SUM, HCOMM_REDUCE_SUM};
    uint32_t count = 2;

    res = HcommBatchTransfer(thread, 0, dstList, srcList, lenList, rw, dataType, reduceOp, count);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransfer, Ut_HcommBatchTransfer_When_LenList_IsNull_Expect_PtrError)
{
    void *dstList = reinterpret_cast<void *>(0x1000);
    const void *srcList = reinterpret_cast<const void *>(0x2000);
    uint64_t *lenList = nullptr;
    uint32_t rw[2] = {0, 1};
    HcommDataType dataType[2] = {HCOMM_DATA_TYPE_FP32, HCOMM_DATA_TYPE_FP16};
    HcommReduceOp reduceOp[2] = {HCOMM_REDUCE_SUM, HCOMM_REDUCE_SUM};
    uint32_t count = 2;

    res = HcommBatchTransfer(thread, 0, dstList, srcList, lenList, rw, dataType, reduceOp, count);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransfer, Ut_HcommBatchTransfer_When_Rw_IsNull_Expect_PtrError)
{
    void *dstList = reinterpret_cast<void *>(0x1000);
    const void *srcList = reinterpret_cast<const void *>(0x2000);
    uint64_t lenList[2] = {64, 128};
    uint32_t *rw = nullptr;
    HcommDataType dataType[2] = {HCOMM_DATA_TYPE_FP32, HCOMM_DATA_TYPE_FP16};
    HcommReduceOp reduceOp[2] = {HCOMM_REDUCE_SUM, HCOMM_REDUCE_SUM};
    uint32_t count = 2;

    res = HcommBatchTransfer(thread, 0, dstList, srcList, lenList, rw, dataType, reduceOp, count);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransfer, Ut_HcommBatchTransfer_When_DataType_IsNull_Expect_PtrError)
{
    void *dstList = reinterpret_cast<void *>(0x1000);
    const void *srcList = reinterpret_cast<const void *>(0x2000);
    uint64_t lenList[2] = {64, 128};
    uint32_t rw[2] = {0, 1};
    HcommDataType *dataType = nullptr;
    HcommReduceOp reduceOp[2] = {HCOMM_REDUCE_SUM, HCOMM_REDUCE_SUM};
    uint32_t count = 2;

    res = HcommBatchTransfer(thread, 0, dstList, srcList, lenList, rw, dataType, reduceOp, count);
    EXPECT_EQ(res, HCCL_E_PTR);
}

TEST_F(UtAicpuTsHcommBatchTransfer, Ut_HcommBatchTransfer_When_ReduceOp_IsNull_Expect_PtrError)
{
    void *dstList = reinterpret_cast<void *>(0x1000);
    const void *srcList = reinterpret_cast<const void *>(0x2000);
    uint64_t lenList[2] = {64, 128};
    uint32_t rw[2] = {0, 1};
    HcommDataType dataType[2] = {HCOMM_DATA_TYPE_FP32, HCOMM_DATA_TYPE_FP16};
    HcommReduceOp *reduceOp = nullptr;
    uint32_t count = 2;

    res = HcommBatchTransfer(thread, 0, dstList, srcList, lenList, rw, dataType, reduceOp, count);
    EXPECT_EQ(res, HCCL_E_PTR);
}


