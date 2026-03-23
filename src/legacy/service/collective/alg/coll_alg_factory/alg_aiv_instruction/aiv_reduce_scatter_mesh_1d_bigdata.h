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
class AivReduceScatterMesh1DBigData : public AivCommBase {
    constexpr static uint64_t stageNum = 2;  // 生产者 消费者
    constexpr static uint64_t TAG_FLAG_SIZE = 8;

public:

    __aicore__ inline AivReduceScatterMesh1DBigData() {
    }

    __aicore__ inline void InitCoreInfo(uint64_t len, uint64_t inputStride)
    {
        // stage1用rankSize个核，stage2用剩余所有核
        if (numBlocks_ <= rankSize_) {
            return;
        }
        coreNumStage1 = rankSize_;
        coreNumPerRank = coreNumStage1 / rankSize_;
        coreNumStage2 = numBlocks_ - coreNumStage1;
        if(GetBlockIdx() < coreNumStage1) { // input->ipc,多个block负责一个rank input
            targetRank = GetBlockIdx() / coreNumPerRank;
            coreIndex = (GetBlockIdx() - (targetRank * coreNumPerRank)) % coreNumPerRank;
            uint64_t dataPerCore = len / coreNumPerRank; // 数据量很少的时候，dataPerCore为0
            uint64_t remainder = len % coreNumPerRank;
            uint64_t innerDispls = 0;
            if (coreIndex < remainder) { // 这部分核需要多处理一个数据
                innerDispls = coreIndex * dataPerCore + coreIndex;
                sendCurCount = dataPerCore + 1;
            } else {
                innerDispls = coreIndex * dataPerCore + remainder;
                sendCurCount = dataPerCore;
            }
            inputOffset = input_ + targetRank  * inputStride + innerDispls * sizeof(T);
            outputOffset = reinterpret_cast<uint64_t>(GM_IN[rank_]) + (targetRank * len + innerDispls) * sizeof(T);
        } else if (GetBlockIdx() < (coreNumStage1 + coreNumStage2)) { // ipc->output,多个block负责一个卡的数据
            coreIndex = GetBlockIdx() - coreNumStage1;
            uint64_t dataPerCore = len / coreNumStage2; // 负责读数据的核，逐卡读取数据,每个核读rankSize次
            uint64_t remainder = len % coreNumStage2;
            uint64_t innerDispls = 0;
            if (coreIndex < remainder) { // 这部分核需要多处理一个数据
                innerDispls = coreIndex * dataPerCore + coreIndex;
                recvCurCount = dataPerCore + 1;
            } else {
                innerDispls = coreIndex * dataPerCore + remainder;
                recvCurCount = dataPerCore;
            }

            outputOffset = output_ + innerDispls * sizeof(T);
            int64_t consumInOffset;
            for (int index = 0; index < rankSize_; index++) { // 轮询每个rank的数据，拉取过来，做顺序累加
                consumInOffset = reinterpret_cast<uint64_t>(GM_IN[(index + coreIndex) % rankSize_]) +
                                 (len * rank_ + innerDispls) * sizeof(T); // 本卡的数据都在ipc
                inputOffVec[index] = consumInOffset;
            }
        }
    }

    __aicore__ inline void Producer()
    {
        CpGM2GM((__gm__ T *)outputOffset, (__gm__ T *)inputOffset, sendCurCount); // 本卡数据拷贝到ipc上
        pipe_barrier(PIPE_ALL);
        uint64_t flag_offset;
        for (int i = 0; i < coreNumStage2; i++) {
            flag_offset = targetRank * coreNumStage2 + i;
            Record(rank_, flag_offset, curTag); // 如果只写一个flag，对端来读的时候，就会竞争卡住，所以这里要写coreNumStage2个flag
        }
    }

    __aicore__ inline void Consumer()
    {
        uint64_t flag_offset;
        for (int index = 0; index < rankSize_; index++) {
            uint32_t rankIdx = (index + coreIndex) % rankSize_;
            // Producer阶段，len数据有coreNumPerRank个flag，现在也用coreNumPerRank个核去读
            flag_offset = rank_ * coreNumStage2 + coreIndex;
            WaitFlag(rankIdx, flag_offset, curTag);
            if (index == 0) { // 通过直接覆盖output把数据清一下
                CpGM2GM((__gm__ T *)outputOffset, (__gm__ T *)inputOffVec[index], recvCurCount);
            } else { // 其他卡往output上做atomic add
                CpGM2GM((__gm__ T *)outputOffset, (__gm__ T *)inputOffVec[index], recvCurCount, reduceOp_);
            }
            pipe_barrier(PIPE_ALL);
        }
    }

    __aicore__ inline void Process(uint32_t curTag)
    {
        this->curTag = static_cast<int32_t>(curTag);
        if(GetBlockIdx() < coreNumStage1){ // 0-1
            Producer();
        } else if(GetBlockIdx() < (coreNumStage1 + coreNumStage2)) { // 2-3
            Consumer();
        }
    }

private:

    uint32_t targetRank;
    uint64_t inputOffset;
    uint64_t outputOffset;
    int32_t curTag;
    uint64_t consumProcessNum;
    int64_t inputOffVec[MAX_RANK_SIZE];
    uint32_t coreNumStage1;
    uint32_t coreNumStage2;
    uint64_t coreIndex;
    uint64_t sendCurCount;
    uint64_t coreNumPerRank;
    uint64_t recvCurCount;
};

template<typename T>
__aicore__ inline void AivReduceScatterV2Mesh1DBigData(EXTERN_KERNEL_ARGS_DEF_V2)
{
    AivReduceScatterMesh1DBigData<T> op;
    op.Init(KERNEL_CLASS_INIT, true);
    op.InitCoreInfo(len, inputSliceStride);
    SyncAll<true>();
    if (block_idx == 0 && tag >> AIV_TAG_MOVE_RIGHT_BITS == 1 && (tag & LOW_16_BITS) == 1) {
        op.BarrierForFirstOP();
    }
    SyncAll<true>();
    op.Process(tag);
    op.BarrierAll();
}