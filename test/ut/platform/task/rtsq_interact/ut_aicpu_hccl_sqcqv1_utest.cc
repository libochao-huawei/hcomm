/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "aicpu_hccl_sqcqv1.h"
#include "hccl_common.h"
#include "aicpu_hccl_sqcq.h"
#include "llt_hccl_stub_mc2.h"

using namespace std;
using namespace hccl;

class AddOneCacheMemcpyRecordPlaceholderSqeV1_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AddOneCacheMemcpyRecordPlaceholderSqeV1_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AddOneCacheMemcpyRecordPlaceholderSqeV1_UT TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "AddOneCacheMemcpyRecordPlaceholderSqeV1_UT Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "AddOneCacheMemcpyRecordPlaceholderSqeV1_UT Test TearDown" << std::endl;
    }
};

TEST_F(AddOneCacheMemcpyRecordPlaceholderSqeV1_UT, ut_sqeIn_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void *>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void *>(0x2000);
    uint32_t partId = 0;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 1;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;
    uint32_t hcclQos = 0;

    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);

    EXPECT_EQ(sqeType, 0);
}

TEST_F(AddOneCacheMemcpyRecordPlaceholderSqeV1_UT, ut_sqeType_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void *>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void *>(0x2000);
    uint32_t partId = 0;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 1;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t *sqeType = nullptr;
    uint32_t hcclQos = 0;

    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, sqeType, hcclQos);
}

TEST_F(AddOneCacheMemcpyRecordPlaceholderSqeV1_UT, ut_src_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = nullptr;
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void *>(0x2000);
    uint32_t partId = 0;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 1;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t sqeType = 0;
    uint32_t hcclQos = 0;

    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);

    EXPECT_EQ(sqeType, 0);
}

TEST_F(AddOneCacheMemcpyRecordPlaceholderSqeV1_UT, ut_dst_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void *>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = nullptr;
    uint32_t partId = 0;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 1;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t sqeType = 0;
    uint32_t hcclQos = 0;

    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);

    EXPECT_EQ(sqeType, 0);
}

TEST_F(AddOneCacheMemcpyRecordPlaceholderSqeV1_UT, ut_normal_case)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void *>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void *>(0x2000);
    uint32_t partId = 0;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 1;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t sqeType = 0;
    uint32_t hcclQos = 5;

    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);

    rtStarsPlaceHolderSqe_t *sqe = reinterpret_cast<rtStarsPlaceHolderSqe_t *>(const_cast<uint8_t *>(sqeIn));
    EXPECT_EQ(sqeType, SqeType::CACHE_MEMCPY_RECORD_PLACEHOLDER_SQE);
    EXPECT_EQ(sqe->header.type, RT_STARS_SQE_TYPE_PLACE_HOLDER);
    EXPECT_EQ(sqe->header.rtStreamId, streamId);
    EXPECT_EQ(sqe->header.taskId, taskId);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.length, length);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.src_addr_low, 0x1000);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.src_addr_high, 0);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.dst_addr_low, 0x2000);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.dst_addr_high, 0);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.partid, partId);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.linkType, linkType);
    EXPECT_EQ(sqe->u.cache_memcpy_record_task_info.qos, hcclQos);
}