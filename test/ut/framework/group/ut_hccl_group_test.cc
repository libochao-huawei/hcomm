/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>
#include <deque>
#include <vector>

#include "hccl/hccl_types.h"
#include "hccl_group.h"

using namespace hccl;

// ==================== calcOpDataVolume ====================

class CalcOpDataVolumeTest : public testing::Test {
protected:
    void SetUp() override { memset(&info, 0, sizeof(info)); }
    hcclOpInfo info;
};

TEST_F(CalcOpDataVolumeTest, AllGather_FP32) {
    info.coll = HcclCMDType::HCCL_CMD_ALLGATHER; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AllGather_FP16) {
    info.coll = HcclCMDType::HCCL_CMD_ALLGATHER; info.sendCount = 2048; info.sendType = HCCL_DATA_TYPE_FP16;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AllReduce_FP32) {
    info.coll = HcclCMDType::HCCL_CMD_ALLREDUCE; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AllReduce_INT64) {
    info.coll = HcclCMDType::HCCL_CMD_ALLREDUCE; info.sendCount = 512; info.sendType = HCCL_DATA_TYPE_INT64;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AllReduce_INT32) {
    info.coll = HcclCMDType::HCCL_CMD_ALLREDUCE; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_INT32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AllReduce_INT8) {
    info.coll = HcclCMDType::HCCL_CMD_ALLREDUCE; info.sendCount = 4096; info.sendType = HCCL_DATA_TYPE_INT8;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AllReduce_BFP16) {
    info.coll = HcclCMDType::HCCL_CMD_ALLREDUCE; info.sendCount = 2048; info.sendType = HCCL_DATA_TYPE_BFP16;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AllReduce_INT128) {
    info.coll = HcclCMDType::HCCL_CMD_ALLREDUCE; info.sendCount = 100; info.sendType = HCCL_DATA_TYPE_INT128;
    EXPECT_EQ(calcOpDataVolume(info), 1600ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_FP32) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_FP64) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP64;
    EXPECT_EQ(calcOpDataVolume(info), 8192ULL);
}
TEST_F(CalcOpDataVolumeTest, AlltoAll_FP32) {
    info.coll = HcclCMDType::HCCL_CMD_ALLTOALL; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, Reduce_FP32) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_UINT8) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 100; info.sendType = HCCL_DATA_TYPE_UINT8;
    EXPECT_EQ(calcOpDataVolume(info), 100ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_UINT16) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 100; info.sendType = HCCL_DATA_TYPE_UINT16;
    EXPECT_EQ(calcOpDataVolume(info), 200ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_UINT32) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 100; info.sendType = HCCL_DATA_TYPE_UINT32;
    EXPECT_EQ(calcOpDataVolume(info), 400ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_UINT64) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 100; info.sendType = HCCL_DATA_TYPE_UINT64;
    EXPECT_EQ(calcOpDataVolume(info), 800ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_INT16) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 100; info.sendType = HCCL_DATA_TYPE_INT16;
    EXPECT_EQ(calcOpDataVolume(info), 200ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_HIF8) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 200; info.sendType = HCCL_DATA_TYPE_HIF8;
    EXPECT_EQ(calcOpDataVolume(info), 200ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_FP8E4M3) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 200; info.sendType = HCCL_DATA_TYPE_FP8E4M3;
    EXPECT_EQ(calcOpDataVolume(info), 200ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_FP8E5M2) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 200; info.sendType = HCCL_DATA_TYPE_FP8E5M2;
    EXPECT_EQ(calcOpDataVolume(info), 200ULL);
}
TEST_F(CalcOpDataVolumeTest, Broadcast_FP8E8M0) {
    info.coll = HcclCMDType::HCCL_CMD_BROADCAST; info.sendCount = 200; info.sendType = HCCL_DATA_TYPE_FP8E8M0;
    EXPECT_EQ(calcOpDataVolume(info), 200ULL);
}
TEST_F(CalcOpDataVolumeTest, ReduceScatter_FP32) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER; info.recvCount = 1024; info.recvType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, ReduceScatter_FP16) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER; info.recvCount = 2048; info.recvType = HCCL_DATA_TYPE_FP16;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, ReduceScatter_INT64) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER; info.recvCount = 512; info.recvType = HCCL_DATA_TYPE_INT64;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, ReduceScatter_UINT8) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER; info.recvCount = 4096; info.recvType = HCCL_DATA_TYPE_UINT8;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, ReduceScatter_BFP16) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER; info.recvCount = 2048; info.recvType = HCCL_DATA_TYPE_BFP16;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, ReduceScatter_INT128) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER; info.recvCount = 256; info.recvType = HCCL_DATA_TYPE_INT128;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, Scatter_FP32) {
    info.coll = HcclCMDType::HCCL_CMD_SCATTER; info.recvCount = 1024; info.recvType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, Scatter_FP64) {
    info.coll = HcclCMDType::HCCL_CMD_SCATTER; info.recvCount = 512; info.recvType = HCCL_DATA_TYPE_FP64;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, Scatter_INT32) {
    info.coll = HcclCMDType::HCCL_CMD_SCATTER; info.recvCount = 1024; info.recvType = HCCL_DATA_TYPE_INT32;
    EXPECT_EQ(calcOpDataVolume(info), 4096ULL);
}
TEST_F(CalcOpDataVolumeTest, AlltoAllV_ReturnsZero) {
    info.coll = HcclCMDType::HCCL_CMD_ALLTOALLV; EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, AlltoAllVC_ReturnsZero) {
    info.coll = HcclCMDType::HCCL_CMD_ALLTOALLVC; EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, AllGatherV_ReturnsZero) {
    info.coll = HcclCMDType::HCCL_CMD_ALLGATHER_V; EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, ReduceScatterV_ReturnsZero) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V; EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, Send_ReturnsZero) {
    info.coll = HcclCMDType::HCCL_CMD_SEND; info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, BatchSendRecv_ReturnsZero) {
    info.coll = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV; info.sendCount = 1024;
    EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, UnknownOpType_ReturnsZero) {
    info.coll = static_cast<HcclCMDType>(999); info.sendCount = 1024; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, ZeroSendCount) {
    info.coll = HcclCMDType::HCCL_CMD_ALLREDUCE; info.sendCount = 0; info.sendType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, ZeroRecvCount) {
    info.coll = HcclCMDType::HCCL_CMD_REDUCE_SCATTER; info.recvCount = 0; info.recvType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 0ULL);
}
TEST_F(CalcOpDataVolumeTest, AlltoAll_UsesSendTypeNotRecvType) {
    info.coll = HcclCMDType::HCCL_CMD_ALLTOALL; info.sendCount = 100;
    info.sendType = HCCL_DATA_TYPE_FP64; info.recvType = HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(calcOpDataVolume(info), 800ULL);
}

// ==================== sortGroupTasks ====================

class SortGroupTasksTest : public testing::Test {
protected:
    void SetUp() override { tasks.clear(); }
    std::deque<hcclOpInfo> tasks;

    void Push(HcclCMDType coll, u64 count, HcclDataType dt) {
        hcclOpInfo op;
        memset(&op, 0, sizeof(op));
        op.coll = coll;
        op.sendCount = count;
        op.sendType = dt;
        op.recvCount = count;
        op.recvType = dt;
        tasks.push_back(op);
    }

    // Verify op at index has expected coll type
    void ExpectColl(const std::vector<hcclOpInfo>& result, size_t idx, HcclCMDType coll) {
        ASSERT_LT(idx, result.size());
        EXPECT_EQ(result[idx].coll, coll);
    }
};

TEST_F(SortGroupTasksTest, EmptyQueue) {
    auto result = sortGroupTasks(tasks);
    EXPECT_TRUE(result.empty());
}

TEST_F(SortGroupTasksTest, SingleOp) {
    Push(HcclCMDType::HCCL_CMD_ALLREDUCE, 1024, HCCL_DATA_TYPE_FP32);
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 1ULL);
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_ALLREDUCE);
}

TEST_F(SortGroupTasksTest, NonV_SortedByVolumeDesc) {
    // Insert in wrong order: small, medium, large
    Push(HcclCMDType::HCCL_CMD_BROADCAST, 256, HCCL_DATA_TYPE_FP32);   // 1024
    Push(HcclCMDType::HCCL_CMD_ALLGATHER, 1024 * 1024, HCCL_DATA_TYPE_FP32); // 4MB
    Push(HcclCMDType::HCCL_CMD_ALLREDUCE, 1024, HCCL_DATA_TYPE_FP32);   // 4096
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 3ULL);
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_ALLGATHER);  // largest
    ExpectColl(result, 1, HcclCMDType::HCCL_CMD_ALLREDUCE);
    ExpectColl(result, 2, HcclCMDType::HCCL_CMD_BROADCAST); // smallest
}

TEST_F(SortGroupTasksTest, NonV_StableForEqualVolume) {
    Push(HcclCMDType::HCCL_CMD_ALLREDUCE, 1024, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLGATHER, 1024, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_REDUCE, 1024, HCCL_DATA_TYPE_FP32);
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 3ULL);
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_ALLREDUCE);
    ExpectColl(result, 1, HcclCMDType::HCCL_CMD_ALLGATHER);
    ExpectColl(result, 2, HcclCMDType::HCCL_CMD_REDUCE);
}

TEST_F(SortGroupTasksTest, OnlyVariants_PreserveOrder) {
    Push(HcclCMDType::HCCL_CMD_ALLTOALLV, 0, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLGATHER_V, 0, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLTOALLVC, 0, HCCL_DATA_TYPE_FP32);
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 3ULL);
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_ALLTOALLV);
    ExpectColl(result, 1, HcclCMDType::HCCL_CMD_ALLGATHER_V);
    ExpectColl(result, 2, HcclCMDType::HCCL_CMD_ALLTOALLVC);
}

TEST_F(SortGroupTasksTest, Mixed_VariantsToEndPreserveOrder) {
    // V1, NonV_large, V2, NonV_small, V3, NonV_medium
    Push(HcclCMDType::HCCL_CMD_ALLTOALLV, 0, HCCL_DATA_TYPE_FP32);      // V #1
    Push(HcclCMDType::HCCL_CMD_ALLGATHER, 1024, HCCL_DATA_TYPE_FP32);   // 4096
    Push(HcclCMDType::HCCL_CMD_ALLTOALLV, 0, HCCL_DATA_TYPE_FP32);      // V #2
    Push(HcclCMDType::HCCL_CMD_SCATTER, 256, HCCL_DATA_TYPE_FP32);      // 1024 (recv)
    Push(HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, 0, HCCL_DATA_TYPE_FP32); // V #3
    Push(HcclCMDType::HCCL_CMD_ALLREDUCE, 512, HCCL_DATA_TYPE_FP32);    // 2048
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 6ULL);
    // Non-V sorted desc: AllGather(4096) > AllReduce(2048) > Scatter(1024)
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_ALLGATHER);
    ExpectColl(result, 1, HcclCMDType::HCCL_CMD_ALLREDUCE);
    ExpectColl(result, 2, HcclCMDType::HCCL_CMD_SCATTER);
    // V-variants preserve order
    ExpectColl(result, 3, HcclCMDType::HCCL_CMD_ALLTOALLV);
    ExpectColl(result, 4, HcclCMDType::HCCL_CMD_ALLTOALLV);
    ExpectColl(result, 5, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V);
}

TEST_F(SortGroupTasksTest, Mixed_AllFourVariantTypes) {
    Push(HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, 0, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_BROADCAST, 100, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLTOALLV, 0, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLTOALLVC, 0, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLGATHER_V, 0, HCCL_DATA_TYPE_FP32);
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 5ULL);
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_BROADCAST);
    // All 4 variant types at end, preserve order
    ExpectColl(result, 1, HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V);
    ExpectColl(result, 2, HcclCMDType::HCCL_CMD_ALLTOALLV);
    ExpectColl(result, 3, HcclCMDType::HCCL_CMD_ALLTOALLVC);
    ExpectColl(result, 4, HcclCMDType::HCCL_CMD_ALLGATHER_V);
}

TEST_F(SortGroupTasksTest, Mixed_VariantsAtBothEnds) {
    Push(HcclCMDType::HCCL_CMD_ALLTOALLVC, 0, HCCL_DATA_TYPE_FP32);     // V at front
    Push(HcclCMDType::HCCL_CMD_ALLREDUCE, 1000, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_BROADCAST, 100, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLGATHER_V, 0, HCCL_DATA_TYPE_FP32);    // V at back
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 4ULL);
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_ALLREDUCE);
    ExpectColl(result, 1, HcclCMDType::HCCL_CMD_BROADCAST);
    ExpectColl(result, 2, HcclCMDType::HCCL_CMD_ALLTOALLVC);
    ExpectColl(result, 3, HcclCMDType::HCCL_CMD_ALLGATHER_V);
}

TEST_F(SortGroupTasksTest, Mixed_ReduceScatterAndScatterSortByRecvType) {
    // Both use recvType; ReduceScatter(FP64) > Scatter(FP32)
    Push(HcclCMDType::HCCL_CMD_SCATTER, 512, HCCL_DATA_TYPE_FP32);          // 2048
    Push(HcclCMDType::HCCL_CMD_REDUCE_SCATTER, 512, HCCL_DATA_TYPE_FP64);  // 4096
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 2ULL);
    ExpectColl(result, 0, HcclCMDType::HCCL_CMD_REDUCE_SCATTER);
    ExpectColl(result, 1, HcclCMDType::HCCL_CMD_SCATTER);
}

TEST_F(SortGroupTasksTest, LargeInput_ManyOps) {
    for (int i = 0; i < 100; i++) {
        Push(HcclCMDType::HCCL_CMD_ALLREDUCE, (u64)(100 - i) * 1024, HCCL_DATA_TYPE_FP32);
    }
    Push(HcclCMDType::HCCL_CMD_ALLTOALLV, 0, HCCL_DATA_TYPE_FP32);
    Push(HcclCMDType::HCCL_CMD_ALLGATHER_V, 0, HCCL_DATA_TYPE_FP32);
    auto result = sortGroupTasks(tasks);
    ASSERT_EQ(result.size(), 102ULL);
    // First 100 sorted descending
    for (size_t i = 0; i < 99; i++) {
        u64 cur = calcOpDataVolume(result[i]);
        u64 next = calcOpDataVolume(result[i + 1]);
        EXPECT_GE(cur, next) << "i=" << i;
    }
    // Last 2 are variants
    ExpectColl(result, 100, HcclCMDType::HCCL_CMD_ALLTOALLV);
    ExpectColl(result, 101, HcclCMDType::HCCL_CMD_ALLGATHER_V);
}
