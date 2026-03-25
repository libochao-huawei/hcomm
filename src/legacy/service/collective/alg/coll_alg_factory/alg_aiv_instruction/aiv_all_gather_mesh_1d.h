/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_communication_base_v2.h"

using namespace AscendC;

template<typename T>
class AivAllGatherMesh1D : public AivCommBase {
public:
    __aicore__ inline AivAllGatherMesh1D() {}

    __aicore__ inline void Process(uint64_t count, uint64_t tag, uint64_t stride)
    {
        RunCtrlCore(count, tag, stride);
    }

    __aicore__ inline void RunCtrlCore(uint64_t count, uint64_t tag, uint64_t stride)
    {
        if (numBlocks_ > rankSize_) {
            numBlocks_ = rankSize_;
        }
        if (block_idx >= numBlocks_) {
            SyncAll<true>();
            return;
        }
        // 分核把数据从input搬到gm
        auto input = reinterpret_cast<__gm__ T *>(input_);
        uint64_t dataTypeSize = sizeof(T);
        uint64_t countPerCore = count / numBlocks_;
        uint64_t curCountCore = block_idx == numBlocks_ - 1 ? count - countPerCore * (numBlocks_ - 1) : countPerCore;
        auto gmIn = reinterpret_cast<__gm__ T *>(reinterpret_cast<uint64_t>(GM_IN[rank_]) + block_idx * countPerCore * dataTypeSize);
        CpGM2GM(gmIn, input + block_idx * countPerCore, curCountCore);
        SyncAll<true>();

        uint32_t perCoreRankNum = rankSize_ / numBlocks_;
        uint32_t remainRankNum = rankSize_ % numBlocks_;
        uint32_t curCoreRankNum = block_idx < remainRankNum ? perCoreRankNum + 1 : perCoreRankNum;
        uint32_t startRank = block_idx < remainRankNum ? (perCoreRankNum + 1) * block_idx : perCoreRankNum * block_idx + remainRankNum;
        for (uint32_t rank = startRank; rank < startRank + curCoreRankNum; rank++) {
            Record(rank, rank_, tag);
        }
        for (uint32_t rank = startRank; rank < startRank + curCoreRankNum; rank++) {
            auto gmOthers = reinterpret_cast<__gm__ T *>(reinterpret_cast<uint64_t>(GM_IN[rank]));
            auto output = reinterpret_cast<__gm__ T *>(output_ + rank * stride);
            WaitFlag(rank_, rank, tag);
            CpGM2GM(output, gmOthers, count);
            PipeBarrier<PIPE_ALL>();
            Record(rank, rank_ + rankSize_, tag);
        }
        for (uint32_t rank = startRank; rank < startRank + curCoreRankNum; rank++) {
            WaitFlag(rank_, rank + rankSize_, tag);
        }
    }   
    uint64_t coreOffset;
    int32_t curTag;
    uint64_t curCount;
};

template<typename T>
__aicore__ inline void AivAllGatherV2Mesh1D(EXTERN_KERNEL_ARGS_DEF_V2)
{
    AivAllGatherMesh1D<T> op;
    op.Init(KERNEL_CLASS_INIT, true);
    SyncAll<true>();
    if (block_idx == 0 && tag >> AIV_TAG_MOVE_RIGHT_BITS == 1 && (tag & LOW_16_BITS) == 1) {
        op.BarrierForFirstOP();
    }
	SyncAll<true>();

    op.Process(len, tag, outputSliceStride);
    // 执行barrier全同步
    op.BarrierAll();
}

template<typename T>
__aicore__ inline void AivAllGatherV2Mesh1DSuperKernel(SUPERKERNEL_ARGS_DEF)
{
    AivAllGatherMesh1D<T> op;
    op.Init(SUPERKERNEL_CLASS_INIT);
    uint64_t maxCountPerLoop = op.cclBufferSize_ / UB_ALIGN_SIZE * UB_ALIGN_SIZE / op.rankSize_ / sizeof(T);
    uint64_t countLeft = op.len_;

    int32_t loopTag = (op.tag_ << 15);

    while (countLeft > 0) {
        uint64_t curCount = (countLeft > maxCountPerLoop) ? maxCountPerLoop : countLeft;
        uint64_t curSize = curCount * sizeof(T);

        op.Process(curCount, loopTag, op.outputSliceStride_);
        op.BarrierAll();

        countLeft -= curCount;
        op.input_ += curSize;
        op.output_ += curSize;
        loopTag += curSize / UB_DB_DATA_BATCH_SIZE + 1;
    }
}

__aicore__ inline void sk_ag_mesh_1d(SUPERKERNEL_ARGS_DEF)
{
    #ifdef HCCL_DTYPE_INT8
        AivAllGatherV2Mesh1DSuperKernel<int8_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_UINT8
        AivAllGatherV2Mesh1DSuperKernel<uint8_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_INT16
        AivAllGatherV2Mesh1DSuperKernel<int16_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_UINT16
        AivAllGatherV2Mesh1DSuperKernel<uint16_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_INT32
        AivAllGatherV2Mesh1DSuperKernel<int32_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_UINT32
        AivAllGatherV2Mesh1DSuperKernel<uint32_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_FP16
        AivAllGatherV2Mesh1DSuperKernel<half> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_FP32
        AivAllGatherV2Mesh1DSuperKernel<float> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_BFP16
        AivAllGatherV2Mesh1DSuperKernel<bfloat16_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_INT64
        AivAllGatherV2Mesh1DSuperKernel<int64_t> (SUPERKERNEL_ARGS_CALL);
    #elif defined HCCL_DTYPE_UINT64
        AivAllGatherV2Mesh1DSuperKernel<uint64_t> (SUPERKERNEL_ARGS_CALL);
    #else
    #endif
}
