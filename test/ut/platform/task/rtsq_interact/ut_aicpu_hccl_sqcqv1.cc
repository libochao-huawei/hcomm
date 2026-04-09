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
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "aicpu/aicpu_hccl_sqcqv1.h"
#include "hccl_common.h"

using namespace std;
using namespace hccl;

class RtsqInteract_UT : public testing::Test {
protected:
    static void SetUpTestCase() { }
    static void TearDownTestCase() { }
    virtual void SetUp() { }
    virtual void TearDown() { }
};

TEST_F(RtsqInteract_UT, Ut_AddOneRdmaDbSendSqeV1_When_DbAddrIsZero_Expect_SqeTypeIsInvalid) {
    uint8_t sqe[64] = {0};
    uint8_t sqeType = 0;
    AddOneRdmaDbSendSqeV1(1, 1, 0, 0, 100, 1, sqe, &sqeType);
    EXPECT_EQ(sqeType, RDMA_DB_SEND_SQE);
    rtStarsWriteValueSqe_t* sqeStruct = reinterpret_cast<rtStarsWriteValueSqe_t*>(sqe);
    EXPECT_EQ(sqeStruct->header.type, RT_STARS_SQE_TYPE_INVALID);
}

TEST_F(RtsqInteract_UT, Ut_AddOneRdmaDbSendSqeV1_When_NormalParams_Expect_SqeTypeIsRdmaDbSend) {
    uint8_t sqe[64] = {0};
    uint8_t sqeType = 0;
    uint64_t dbAddr = 0x1000000ULL;
    AddOneRdmaDbSendSqeV1(1, 1, 0x12345678, dbAddr, 100, 2, sqe, &sqeType);
    EXPECT_EQ(sqeType, RDMA_DB_SEND_SQE);
    rtStarsWriteValueSqe_t* sqeStruct = reinterpret_cast<rtStarsWriteValueSqe_t*>(sqe);
    EXPECT_EQ(sqeStruct->header.type, RT_STARS_SQE_TYPE_WRITE_VALUE);
}

TEST_F(RtsqInteract_UT, Ut_AddOneRdmaDbSendSqeV1_When_NormalParams_Expect_SqeFieldsAreCorrect) {
    uint8_t sqe[64] = {0};
    uint8_t sqeType = 0;
    uint64_t dbInfo = 0xABCD;
    uint64_t dbAddr = 0x1000000ULL;
    uint32_t length = 256;
    uint8_t rdmaType = 3;
    uint16_t streamId = 5;
    uint16_t taskId = 10;
    
    AddOneRdmaDbSendSqeV1(streamId, taskId, dbInfo, dbAddr, length, rdmaType, sqe, &sqeType);
    
    rtStarsWriteValueSqe_t* sqeStruct = reinterpret_cast<rtStarsWriteValueSqe_t*>(sqe);
    EXPECT_EQ(sqeStruct->header.type, RT_STARS_SQE_TYPE_WRITE_VALUE);
    EXPECT_EQ(sqeStruct->header.rtStreamId, streamId);
    EXPECT_EQ(sqeStruct->header.taskId, taskId);
    EXPECT_EQ(sqeStruct->sub_type, RT_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND);
    EXPECT_EQ(sqeStruct->write_addr_low, static_cast<uint32_t>(dbAddr & MASK_32_BIT));
    EXPECT_EQ(sqeStruct->write_addr_high, static_cast<uint32_t>((dbAddr >> UINT32_BIT_NUM) & MASK_17_BIT));
    EXPECT_EQ(sqeStruct->rdmaWrLenth, length);
    EXPECT_EQ(sqeStruct->rdmaType, rdmaType);
}