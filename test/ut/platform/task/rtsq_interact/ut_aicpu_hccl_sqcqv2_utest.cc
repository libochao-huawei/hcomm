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

#include "aicpu_hccl_sqcqv2.h"
#include "hccl_common.h"
#include "aicpu_hccl_sqcq.h"
#include "llt_hccl_stub_mc2.h"

using namespace std;
using namespace hccl;

class AicpuHcclSqcqv2_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AicpuHcclSqcqv2_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AicpuHcclSqcqv2_UT TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "AicpuHcclSqcqv2_UT Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "AicpuHcclSqcqv2_UT Test TearDown" << std::endl;
    }
};

// AddOneNotifyWaitSqeV2 测试用例
TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneNotifyWaitSqeV2_sqeIn_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;
    dfx::DfxTimeOutConfig dfxTimeOutConfig = {0, 0, false};

    AddOneNotifyWaitSqeV2(streamId, taskId, notifyId, sqeIn, &sqeType, dfxTimeOutConfig);

    EXPECT_EQ(sqeType, 0);
}

TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneNotifyWaitSqeV2_sqeType_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t *sqeType = nullptr;
    dfx::DfxTimeOutConfig dfxTimeOutConfig = {0, 0, false};

    AddOneNotifyWaitSqeV2(streamId, taskId, notifyId, sqeIn, sqeType, dfxTimeOutConfig);
}

TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneNotifyWaitSqeV2_normal)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t sqeType = 0;
    dfx::DfxTimeOutConfig dfxTimeOutConfig = {0, 0, false};

    AddOneNotifyWaitSqeV2(streamId, taskId, notifyId, sqeIn, &sqeType, dfxTimeOutConfig);

    EXPECT_EQ(sqeType, SqeType::NOTIFY_SQE_V2);
    rtStarsNotifySqeV2_t *sqe = reinterpret_cast<rtStarsNotifySqeV2_t *>(const_cast<uint8_t *>(sqeIn));
    EXPECT_EQ(sqe->header.type, RT_HW_STARS_SQE_TYPE_NOTIFY_WAIT);
    EXPECT_EQ(sqe->header.rt_stream_id, streamId);
    EXPECT_EQ(sqe->header.task_id, taskId);
    EXPECT_EQ(sqe->notify_id, notifyId);
}

// AddOneRecordSqeV2 测试用例
TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneRecordSqeV2_sqeIn_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;

    AddOneRecordSqeV2(streamId, taskId, notifyId, sqeIn, &sqeType);

    EXPECT_EQ(sqeType, 0);
}

TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneRecordSqeV2_sqeType_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t *sqeType = nullptr;

    AddOneRecordSqeV2(streamId, taskId, notifyId, sqeIn, sqeType);
}

TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneRecordSqeV2_normal)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t sqeType = 0;

    AddOneRecordSqeV2(streamId, taskId, notifyId, sqeIn, &sqeType);

    EXPECT_EQ(sqeType, SqeType::NOTIFY_SQE_V2);
    rtStarsNotifySqeV2_t *sqe = reinterpret_cast<rtStarsNotifySqeV2_t *>(const_cast<uint8_t *>(sqeIn));
    EXPECT_EQ(sqe->header.type, RT_HW_STARS_SQE_TYPE_NOTIFY_RECORD);
    EXPECT_EQ(sqe->header.rt_stream_id, streamId);
    EXPECT_EQ(sqe->header.task_id, taskId);
    EXPECT_EQ(sqe->notify_id, notifyId);
}

// AddOneWriteValueRecordSqeV2 测试用例
TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneWriteValueRecordSqeV2_sqeIn_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyWRAddr = 0x1000;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;

    AddOneWriteValueRecordSqeV2(streamId, taskId, notifyWRAddr, sqeIn, &sqeType);

    EXPECT_EQ(sqeType, 0);
}

TEST_F(AicpuHcclariqv2_UT, ut_AddOneWriteValueRecordSqeV2_sqeType_nullptr)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyWRAddr = 0x1000;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t *sqeType = nullptr;

    AddOneWriteValueRecordSqeV2(streamId, taskId, notifyWRAddr, sqeIn, sqeType);
}

TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneWriteValueRecordSqeV2_normal)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyWRAddr = 0x1000;
    uint8_t sqeBuffer[HCCL_SQE_SIZE] = {0};
    const uint8_t *sqeIn = sqeBuffer;
    uint8_t sqeType = 0;

    AddOneWriteValueRecordSqeV2(streamId, taskId, notifyWRAddr, sqeIn, &sqeType);

    EXPECT_EQ(sqeType, SqeType::WRITE_VALUE_SQE_V2);
    rtStarsWriteValueSqeV2_t *sqe = reinterpret_cast<rtStarsWriteValueSqeV2_t *>(const_cast<uint8_t *>(sqeIn));
    EXPECT_EQ(sqe->header.type, RT_HW_STARS_SQE_TYPE_WRITE_VALUE);
    EXPECT_EQ(sqe->header.rt_stream_id, streamId);
    EXPECT_EQ(sqe->header.task_id, taskId);
    EXPECT_EQ(sqe->write_val[0], 1U);
}

// AddOneMemcpySqeV2 测试用例
TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneMemcpySqeV2_sqeIn_nullptr)
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

    AddOneMemcpySqeV2(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);

    EXPECT_EQ(sqeType, 0);
}

TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneMemcpySqeV2_sqeType_nullptr)
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

    AddOneMemcpySqeV2(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, sqeType, hcclQos);
}

TEST_F(AicpuHcclSqcqv2_UT, ut_AddOneMemcpySqeV2_normal)
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

    AddOneMemcpySqeV2(streamId, taskId, src, length, runtimeDataType, rtReduceOp,
        dst, partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);

    EXPECT_EQ(sqeType, SqeType::MEMCPY_ASYNC_SQE_V2);
    rtStarsMemcpyAsyncSqeV2_t *sqe = reinterpret_cast<rtStarsMemcpyAsyncSqeV2_t *>(const_cast<uint8_t *>(sqeIn));
    EXPECT_EQ(sqe->type, RT_HW_STARS_SQE_TYPE_SDMA);
    EXPECT_EQ(sqe->rt_stream_id, streamId);
    EXPECT_EQ(sqe->task_id, taskId);
    EXPECT_EQ(sqe->length, length);
    EXPECT_EQ(sqe->src_addr_low, 0x1000);
    EXPECT_EQ(sqe->src_addr_high, 0);
    EXPECT_EQ(sqe->dst_addr_low, 0x2000);
    EXPECT_EQ(sqe->dst_addr_high, 0);
    EXPECT_EQ(sqe->qos, hcclQos);
}