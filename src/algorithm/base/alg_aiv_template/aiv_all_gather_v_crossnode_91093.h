/*
 * Copyright (c) 2024-2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_communication_base.h"
#include "aiv_crossnode_91093_base.h"

using namespace AscendC;

class AivAllGatherVCrossNode91093 : public AivCrossNode91093Base {
public:
    __aicore__ inline AivAllGatherVCrossNode91093() {}

    template<typename T>
    __aicore__ inline void Process(GM_ADDR buffIn0, GM_ADDR buffOut0, GM_ADDR commInfoAddr, GM_ADDR input,
        GM_ADDR output, int32_t tag, uint64_t bufferCount, const ExtraArgsV2* extraArgs);

    __aicore__ inline void InitSelf(GM_ADDR buffOut0, uint32_t rank, uint32_t rankSize, int32_t tag, bool useDoubleBuffer);

    template<typename T>
    __aicore__ inline void InitDataCopyOffset(const ExtraArgsV2* extraArgs, uint64_t maxCountPerLoop);

    __aicore__ inline void CalcNumTargetsAndTargetRanks();

private:
    uint32_t targetLoopNum_[MAX_TARGET_NUM] = {}; // 对端循环次数
    uint32_t maxTargetLoopNum_ = 0;
    uint32_t countMid_[MAX_TARGET_NUM] = {};
    uint32_t countTail_[MAX_TARGET_NUM] = {};
};

__aicore__ inline void AivAllGatherVCrossNode91093::InitSelf(GM_ADDR buffOut0, uint32_t rank, uint32_t rankSize,
    int32_t tag, bool useDoubleBuffer)
{
    flagAddrSelf_ = buffOut0;
    rank_ = rank;
    tag_ = tag;
    rankSize_ = rankSize;
    useDoubleBuffer_ = useDoubleBuffer;
    usedBlockNum_ = block_num;
    pingpongOffset = 0;

    InitSetCheckClearArgsTensor();
    CalcNumTargetsAndTargetRanks();
    InitOffset();
}

__aicore__ inline void AivAllGatherVCrossNode91093::CalcNumTargetsAndTargetRanks()
{
    // rankSize + 1 中额外的 1 个核用于input到output搬运
    numTargets = (rankSize_ + 1) / usedBlockNum_;
    uint32_t tailRankSize = (rankSize_ + 1) % usedBlockNum_;
    if (tailRankSize > 0 && block_idx < tailRankSize) {
        numTargets += 1;
    }

    for (uint32_t i = 0; i < numTargets; i++) {
        uint32_t targetRank =  (block_idx + i * usedBlockNum_) % (rankSize_ + 1);
        targetRanks[i] = targetRank;
    }
}

template<typename T>
__aicore__ inline void AivAllGatherVCrossNode91093::InitDataCopyOffset(const ExtraArgsV2* extraArgs, uint64_t maxCountPerLoop)
{
    uint64_t len = 0;
    // 核数不足, 每个aiv核服务多个对端
    if (rankSize_ + 1 > block_num) {
        blockNumPerGroup = 1;
        blockIdxInGroup = 0;
        for(uint32_t i=0; i<numTargets; ++i) {
            uint32_t targetRank = targetRanks[i];
            // 目标为rankSize的核实际负责本卡数据搬运
            if (targetRank == rankSize_){
                targetRank = rank_;
            }
            len = extraArgs->recvCounts[targetRank];
            targetLoopNum_[i] = CeilDiv(len, maxCountPerLoop);
            maxTargetLoopNum_ = maxTargetLoopNum_ > targetLoopNum_[i] ? maxTargetLoopNum_ : targetLoopNum_[i];

            if (len <= maxCountPerLoop) {
                countMid_[i] = 0;
                countTail_[i] = len;
            } else if (len % maxCountPerLoop == 0) {
                countMid_[i] = maxCountPerLoop;
                countTail_[i] = maxCountPerLoop;
            } else {
                countMid_[i] = maxCountPerLoop;
                countTail_[i] = len % maxCountPerLoop;
            }
        }
        blockOffsetMid = 0;
        blockOffsetTail = 0;
    }
    // 核数充足，为（rankSize+1）倍数
    else {
        blockNumPerGroup = block_num / (rankSize_ + 1);
        numTargets = 1;
        targetRanks[0] = block_idx / blockNumPerGroup;
        blockIdxInGroup = block_idx % blockNumPerGroup;

        uint32_t padCount = UB_ALIGN_SIZE / sizeof(T);

        uint32_t targetRank = targetRanks[0];
        // 目标为rankSize的核实际负责本卡数据搬运
        if (targetRank == rankSize_){
            targetRank = rank_;
        }
        len = extraArgs->recvCounts[targetRank];
        targetLoopNum_[0] = CeilDiv(len, maxCountPerLoop);
        maxTargetLoopNum_ = targetLoopNum_[0];

        if (len <= maxCountPerLoop) {
            countMid = 0;
            blockOffsetMid = 0;
            CalCountAndBlockOffset(len, blockNumPerGroup, blockIdxInGroup, padCount, countTail, blockOffsetTail);
        } else if (len % maxCountPerLoop == 0) {
            CalCountAndBlockOffset(maxCountPerLoop, blockNumPerGroup, blockIdxInGroup, padCount, countMid, blockOffsetMid);
            countTail = countMid;
            blockOffsetTail = blockOffsetMid;
        } else {
            CalCountAndBlockOffset(maxCountPerLoop, blockNumPerGroup, blockIdxInGroup, padCount, countMid, blockOffsetMid);
            uint64_t remainLen = len % maxCountPerLoop;
            CalCountAndBlockOffset(remainLen, blockNumPerGroup, blockIdxInGroup, padCount, countTail, blockOffsetTail);
        }
        countMid_[0] = countMid;
        countTail_[0] = countTail;
    }
}

template<typename T>
__aicore__ inline void AivAllGatherVCrossNode91093::Process(GM_ADDR buffIn0, GM_ADDR buffOut0, GM_ADDR commInfoAddr,
    GM_ADDR input, GM_ADDR output, int32_t tag, uint64_t bufferCount, const ExtraArgsV2* extraArgs)
{
    // 内存准备
    __gm__ T *inputGM = (__gm__ T *)input;
    __gm__ T *outputGM = (__gm__ T *)output;
    __gm__ T *cclGMSelf = (__gm__ T *)buffIn0;

    GlobalTensor<uint64_t> bufferArgsGT;
    __gm__ uint64_t *buffersGmAddr = (__gm__ uint64_t *)(commInfoAddr);
    bufferArgsGT.SetGlobalBuffer(buffersGmAddr, FLAG_SIZE * rankSize_ / sizeof(uint64_t));

    GM_ADDR buffersIn[MAX_TARGET_NUM] = {};
    GM_ADDR buffersOut[MAX_TARGET_NUM] = {};

    for (uint32_t i = 0; i < numTargets; i++) {
        uint32_t targetRank = targetRanks[i];
        if (targetRank == rankSize_){
            targetRank = rank_;
        }
        DataCopy(bufferArgsTensor[i * 4], bufferArgsGT[2 * targetRank], 4); // buffersIn buffersOut
    }

    SyncFunc<HardEvent::MTE2_S>();

    for (uint32_t i = 0; i < numTargets; i++) {
        uint32_t curIdx = i * 4;
        buffersIn[i] = (GM_ADDR)(bufferArgsTensor.GetValue(curIdx));
        buffersOut[i] = (GM_ADDR)(bufferArgsTensor.GetValue(curIdx + 1));
    }

    int32_t curTag = (tag << TAG_MOVE_LEFT_BITS);
    uint64_t curOffset = 0;
    uint64_t curCount = 0;
    uint64_t curBlockOffset = 0;

    for (uint32_t loop = 0; loop < maxTargetLoopNum_; loop++) {
        //Step1. 本卡userIn -> 本卡ccl&output
        for (uint32_t i = 0; i < numTargets; ++i) {
            if (loop >= targetLoopNum_[i]){
                continue;
            }
            if (loop == targetLoopNum_[i] - 1) {
                curCount = countTail_[i];
                curBlockOffset = blockOffsetTail;
            } else {
                curCount = countMid_[i];
                curBlockOffset = blockOffsetMid;
            }

            uint32_t targetRank = targetRanks[i];
            if (targetRank == rank_){
                CpGM2GM(cclGMSelf + curBlockOffset, inputGM + curOffset + curBlockOffset, curCount);
                PipeBarrier<PIPE_ALL>();

                Record1vN(curTag, CommPattern::interRank, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();
            }else if (targetRank == rankSize_){
                CpGM2GM(outputGM + extraArgs->recvDispls[rank_] + curOffset + curBlockOffset, inputGM + curOffset + curBlockOffset, curCount);
                PipeBarrier<PIPE_ALL>();
            }
        }

        //Step2. 对端ccl -> 本卡output
        for (uint32_t i = 0; i < numTargets; ++i) {
            if (loop >= targetLoopNum_[i]){
                continue;
            }
            if (loop == targetLoopNum_[i] - 1) {
                curCount = countTail_[i];
                curBlockOffset = blockOffsetTail;
            } else {
                curCount = countMid_[i];
                curBlockOffset = blockOffsetMid;
            }
            uint32_t targetRank = targetRanks[i];
            __gm__ T* cclGMOther = (__gm__ T *)(buffersIn[i]);
            GM_ADDR ctrlFlagGMOther = buffersOut[i];

            if (targetRank != rank_ && targetRank != rankSize_){
                WaitNv1(curTag, ctrlFlagGMOther, false, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();

                CpGM2GM(outputGM + extraArgs->recvDispls[targetRank] + curOffset + curBlockOffset, cclGMOther + curBlockOffset, curCount);
                PipeBarrier<PIPE_ALL>();

                RecordNv1(curTag, ctrlFlagGMOther, false, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();
            }
        }

        for (uint32_t i = 0; i < numTargets; ++i) {
            if (loop >= targetLoopNum_[i]){
                continue;
            }
            if (targetRanks[i] == rank_){
                Wait1vN(curTag * (rankSize_ - 1), CommPattern::interRank, true, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();
            }
        }

        curOffset += bufferCount;
        curTag += 1;
    }
}

template<typename T>
__aicore__ inline void aiv_all_gather_v_crossnode_91093(KERNEL_ARGS_DEF_A3, const ExtraArgsV2* extraArgs)
{
    AivAllGatherVCrossNode91093 op;

    uint64_t maxCountPerLoop = (uint64_t) bufferSize / UB_ALIGN_SIZE * UB_ALIGN_SIZE / sizeof(T);

    op.InitSelf(buffOut0, rank, rankSize, tag, false);
    op.InitDataCopyOffset<T>(extraArgs, maxCountPerLoop);
    op.InitOpCounter(headCountMem, tailCountMem, addOneMem, SIZE_OF_INT32, isEnableCounter);
    op.HeadCounter();
    op.Process<T>(buffIn0, buffOut0, buffOut1, input, output, tag, maxCountPerLoop, extraArgs);
    op.TailCounter();
}