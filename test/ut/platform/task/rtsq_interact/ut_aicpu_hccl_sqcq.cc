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
#include <cstring>

#include "aicpu/aicpu_hccl_sqcqv1.h"
#include "aicpu/aicpu_hccl_sqcqv2.h"
#include "hccl_common.h"

using namespace hccl;

class AicpuHcclSqceTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AicpuHcclSqceTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AicpuHcclSqceTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "AicpuHcclSqceTest Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "AicpuHcclSqceTest Test TearDown" << std::endl;
    }
};

// Test AddOneCacheMemcpyRecordPlaceholderSqeV1 with null sqeIn
TEST_F(AicpuHcclSqceTest, AddOneCacheMemcpyRecordPlaceholderSqeV1_NullSqeIn)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void*>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void*>(0x2000);
    uint32_t partId = 1;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 0;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;
    uint32_t hcclQos = 1;

    // This should not crash, just return
    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp, dst,
        partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);
}

// Test AddOneCacheMemcpyRecordPlaceholderSqeV1 with null sqeType
TEST_F(AicpuHcclSqceTest, AddOneCacheMemcpyRecordPlaceholderSqeV1_NullSqeType)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void*>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void*>(0x2000);
    uint32_t partId = 1;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 0;
    uint8_t sqeIn[256] = {0};
    uint8_t *sqeType = nullptr;
    uint32_t hcclQos = 1;

    // This should not crash, just return
    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp, dst,
        partId, ssid, devId, overflowAddr, linkType, sqeIn, sqeType, hcclQos);
}

// Test AddOneCacheMemcpyRecordPlaceholderSqeV1 with null src
TEST_F(AicpuHcclSqceTest, AddOneCacheMemcpyRecordPlaceholderSqeV1_NullSrc)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = nullptr;
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void*>(0x2000);
    uint32_t partId = 1;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 0;
    uint8_t sqeIn[256] = {0};
    uint8_t sqeType = 0;
    uint32_t hcclQos = 1;

    // This should not crash, just return
    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp, dst,
        partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);
}

// Test AddOneCacheMemcpyRecordPlaceholderSqeV1 with null dst
TEST_F(AicpuHcclSqceTest, AddOneCacheMemcpyRecordPlaceholderSqeV1_NullDst)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void*>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = nullptr;
    uint32_t partId = 1;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 0;
    uint8_t sqeIn[256] = {0};
    uint8_t sqeType = 0;
    uint32_t hcclQos = 1;

    // This should not crash, just return
    AddOneCacheMemcpyRecordPlaceholderSqeV1(streamId, taskId, src, length, runtimeDataType, rtReduceOp, dst,
        partId, ssid, devId, overflowAddr, linkType, sqeIn, &sqeType, hcclQos);
}

// Test AddOneNotifyWaitSqeV2 with null sqeIn
TEST_F(AicpuHcclSqceTest, AddOneNotifyWaitSqeV2_NullSqeIn)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;
    dfx::DfxTimeOutConfig dfxTimeOutConfig;
    dfxTimeOutConfig.useCredit = false;
    dfxTimeOutConfig.sqeTimeOutTimeOut = 0;

    // This should not crash, just return
    AddOneNotifyWaitSqeV2(streamId, taskId, notifyId, sqeIn, &sqeType, dfxTimeOutConfig);
}

// Test AddOneNotifyWaitSqeV2 with null sqeType
TEST_F(AicpuHcclSqceTest, AddOneNotifyWaitSqeV2_NullSqeType)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    uint8_t sqeIn[256] = {0};
    uint8_t *sqeType = nullptr;
    dfx::DfxTimeOutConfig dfxTimeOutConfig;
    dfxTimeOutConfig.useCredit = false;
    dfxTimeOutConfig.sqeTimeOutTimeOut = 0;

    // This should not crash, just return
    AddOneNotifyWaitSqeV2(streamId, taskId, notifyId, sqeIn, sqeType, dfxTimeOutConfig);
}

// Test AddOneRecordSqeV2 with null sqeIn
TEST_F(AicpuHcclSqceTest, AddOneRecordSqeV2_NullSqeIn)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;

    // This should not crash, just return
    AddOneRecordSqeV2(streamId, taskId, notifyId, sqeIn, &sqeType);
}

// Test AddOneRecordSqeV2 with null sqeType
TEST_F(AicpuHcclSqceTest, AddOneRecordSqeV2_NullSqeType)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyId = 100;
    uint8_t sqeIn[256] = {0};
    uint8_t *sqeType = nullptr;

    // This should not crash, just return
    AddOneRecordSqeV2(streamId, taskId, notifyId, sqeIn, sqeType);
}

// Test AddOneWriteValueRecordSqeV2 with null sqeIn
TEST_F(AicpuHcclSqceTest, AddOneWriteValueRecordSqeV2_NullSqeIn)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyWRAddr = 0x1000;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;

    // This should not crash, just return
    AddOneWriteValueRecordSqeV2(streamId, taskId, notifyWRAddr, sqeIn, &sqeType);
}

// Test AddOneWriteValueRecordSqeV2 with null sqeType
TEST_F(AicpuHcclSqceTest, AddOneWriteValueRecordSqeV2_NullSqeType)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    u64 notifyWRAddr = 0x1000;
    uint8_t sqeIn[256] = {0};
    uint8_t *sqeType = nullptr;

    // This should not crash, just return
    AddOneWriteValueRecordSqeV2(streamId, taskId, notifyWRAddr, sqeIn, sqeType);
}

// Test AddOneMemcpySqeV2 with null sqeIn
TEST_F(AicpuHcclSqceTest, AddOneMemcpySqeV2_NullSqeIn)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void*>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void*>(0x2000);
    uint32_t partId = 1;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 0;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;
    uint32_t hcclQos = 1;

    // This should not crash, just return
    AddOneMemcpySqeV2(streamId, taskId, src, length, runtimeDataType, rtReduceOp, dst, partId, ssid, devId,
        overflowAddr, linkType, sqeIn, &sqeType, hcclQos);
}

// Test AddOneMemcpySqeV2 with null sqeType
TEST_F(AicpuHcclSqceTest, AddOneMemcpySqeV2_NullSqeType)
{
    uint16_t streamId = 1;
    uint16_t taskId = 1;
    void *src = reinterpret_cast<void*>(0x1000);
    uint32_t length = 1024;
    aclDataType runtimeDataType = ACL_FLOAT;
    aclrtReduceKind rtReduceOp = ACL_RT_MEMCPY_SDMA_AUTOMATIC_SUM;
    void *dst = reinterpret_cast<void*>(0x2000);
    uint32_t partId = 1;
    uint32_t ssid = 1;
    uint32_t devId = 0;
    u64 overflowAddr = 0;
    uint8_t linkType = 0;
    uint8_t sqeIn[256] = {0};
    uint8_t *sqeType = nullptr;
    uint32_t hcclQos = 1;

    // This should not crash, just return
    AddOneMemcpySqeV2(streamId, taskId, src, length, runtimeDataType, rtReduceOp, dst, partId, ssid, devId,
        overflowAddr, linkType, sqeIn, sqeType, hcclQos);
}

// Test AddOneEventResetSqeV2 with null sqeIn
TEST_F(AicpuHcclSqceTest, AddOneEventResetSqeV2_NullSqeIn)
{
    uint16_t streamId = 1;
    int32_t eventId = 1;
    uint16_t taskId = 1;
    int64_t phyChipId = 0;
    int64_t phyDieId = 0;
    u64 addr = 0x1000;
    const uint8_t *sqeIn = nullptr;
    uint8_t sqeType = 0;

    // This should not crash, just return
    AddOneEventResetSqeV2(streamId, eventId, taskId, phyChipId, phyDieId, addr, sqeIn, &sqeType);
}

// Test AddOneEventResetSqeV2 with null sqeType
TEST_F(AicpuHcclSqceTest, AddOneEventResetSqeV2_NullSqeType)
{
    uint16_t streamId = 1;
    int32_t eventId = 1;
    uint16_t taskId = 1;
    int64_t phyChipId = 0;
    int64_t phyDieId = 0;
    u64 addr = 0x1000;
    uint8_t sqeIn[256] = {0};
    uint8_t *sqeType = nullptr;

    // This should not crash, just return
    AddOneEventResetSqeV2(streamId, eventId, taskId, phyChipId, phyDieId, addr, sqeIn, sqeType);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}