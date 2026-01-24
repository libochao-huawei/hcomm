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
class AivReduceScatterVCrossNode91093 : public AivCrossNode91093Base {
public:
    __aicore__ inline AivReduceScatterVCrossNode91093() {}

    template<typename T>
    __aicore__ inline void Process(GM_ADDR buffIn0, GM_ADDR buffOut0, GM_ADDR commInfoAddr, GM_ADDR input,
        GM_ADDR output, int32_t tag, uint64_t bufferCount, const ExtraArgsV2* extraArgs);

    __aicore__ inline void InitSelf(GM_ADDR buffOut0, uint32_t rank, uint32_t rankSize, int32_t tag, bool useDoubleBuffer);

    template<typename T>
    __aicore__ inline void InitDataCopyOffset(const ExtraArgsV2* extraArgs, uint64_t avgBufferCount);

    __aicore__ inline void CalcNumTargetsAndTargetRanks();

private:
    uint32_t targetLoopNum_[MAX_TARGET_NUM + 1] = {};
    uint32_t maxTargetLoopNum_ = 0;
    uint32_t countMid_[MAX_TARGET_NUM + 1] = {};
    uint32_t countTail_[MAX_TARGET_NUM + 1] = {};

    uint32_t selfBlockOffsetMid_ = 0;
    uint32_t selfBlockOffsetTail_ = 0;
};

__aicore__ inline void AivReduceScatterVCrossNode91093::InitSelf(GM_ADDR buffOut0, uint32_t rank, uint32_t rankSize,
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

__aicore__ inline void AivReduceScatterVCrossNode91093::CalcNumTargetsAndTargetRanks()
{
    numTargets = (rankSize_) / usedBlockNum_;
    uint32_t tailRankSize = (rankSize_) % usedBlockNum_;
    if (tailRankSize > 0 && block_idx < tailRankSize) {
        numTargets += 1;
    }

    for (uint32_t i = 0; i < numTargets; i++) {
        uint32_t targetRank =  (block_idx + i * usedBlockNum_) % (rankSize_);
        targetRanks[i] = targetRank;
    }
}

template<typename T>
__aicore__ inline void AivReduceScatterVCrossNode91093::InitDataCopyOffset(const ExtraArgsV2* extraArgs, uint64_t avgBufferCount)
{
    uint64_t len = 0;
    // 核数不足, 每个aiv核服务多个对端
    if (rankSize_ > block_num) {
        blockNumPerGroup = 1;
        blockIdxInGroup = 0;
        for(uint32_t i = 0; i <= numTargets; ++i) {
            uint32_t targetRank = targetRanks[i];
            if ( i == numTargets ){
                targetRank = rank_;
            }

            len = extraArgs->sendCounts[targetRank];
            targetLoopNum_[i] = CeilDiv(len, avgBufferCount);
            maxTargetLoopNum_ = maxTargetLoopNum_ > targetLoopNum_[i] ? maxTargetLoopNum_ : targetLoopNum_[i];

            if (len <= avgBufferCount) {
                countMid_[i] = 0;
                countTail_[i] = len;
            } else if (len % avgBufferCount == 0) {
                countMid_[i] = avgBufferCount;
                countTail_[i] = avgBufferCount;
            } else {
                countMid_[i] = avgBufferCount;
                countTail_[i] = len % avgBufferCount;
            }
        }

        blockOffsetMid = 0;
        blockOffsetTail = 0;
    }
    // 核数充足，为（rankSize+1）倍数
    else {
        blockNumPerGroup = block_num / (rankSize_);
        numTargets = 1;
        targetRanks[0] = block_idx / blockNumPerGroup;
        blockIdxInGroup = block_idx % blockNumPerGroup;
        uint32_t padCount = UB_ALIGN_SIZE / sizeof(T);

        len = extraArgs->sendCounts[rank_];
        targetLoopNum_[1] = CeilDiv(len, avgBufferCount);
        maxTargetLoopNum_ = maxTargetLoopNum_ > targetLoopNum_[1] ? maxTargetLoopNum_ : targetLoopNum_[1];

        if (len <= avgBufferCount) {
            countMid = 0;
            blockOffsetMid = 0;
            CalCountAndBlockOffset(len, blockNumPerGroup, blockIdxInGroup, padCount, countTail, blockOffsetTail);
        } else if (len % avgBufferCount == 0) {
            CalCountAndBlockOffset(avgBufferCount, blockNumPerGroup, blockIdxInGroup, padCount, countMid, blockOffsetMid);
            countTail = countMid;
            blockOffsetTail = blockOffsetMid;
        } else {
            CalCountAndBlockOffset(avgBufferCount, blockNumPerGroup, blockIdxInGroup, padCount, countMid, blockOffsetMid);
            uint64_t remainLen = len % avgBufferCount;
            CalCountAndBlockOffset(remainLen, blockNumPerGroup, blockIdxInGroup, padCount, countTail, blockOffsetTail);
        }
        countMid_[1] = countMid;
        countTail_[1] = countTail;
        selfBlockOffsetMid_ = blockOffsetMid;
        selfBlockOffsetTail_ = blockOffsetTail;

        uint32_t targetRank = targetRanks[0];
        len = extraArgs->sendCounts[targetRank];
        targetLoopNum_[0] = CeilDiv(len, avgBufferCount);
        maxTargetLoopNum_ = maxTargetLoopNum_ > targetLoopNum_[0] ? maxTargetLoopNum_ : targetLoopNum_[0];

        if (len <= avgBufferCount) {
            countMid = 0;
            blockOffsetMid = 0;
            CalCountAndBlockOffset(len, blockNumPerGroup, blockIdxInGroup, padCount, countTail, blockOffsetTail);
        } else if (len % avgBufferCount == 0) {
            CalCountAndBlockOffset(avgBufferCount, blockNumPerGroup, blockIdxInGroup, padCount, countMid, blockOffsetMid);
            countTail = countMid;
            blockOffsetTail = blockOffsetMid;
        } else {
            CalCountAndBlockOffset(avgBufferCount, blockNumPerGroup, blockIdxInGroup, padCount, countMid, blockOffsetMid);
            uint64_t remainLen = len % avgBufferCount;
            CalCountAndBlockOffset(remainLen, blockNumPerGroup, blockIdxInGroup, padCount, countTail, blockOffsetTail);
        }
        countMid_[0] = countMid;
        countTail_[0] = countTail;
    }
}

template<typename T>
__aicore__ inline void AivReduceScatterVCrossNode91093::Process(GM_ADDR buffIn0, GM_ADDR buffOut0, GM_ADDR commInfoAddr,
    GM_ADDR input, GM_ADDR output, int32_t tag, uint64_t avgBufferCount, const ExtraArgsV2* extraArgs)
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
        DataCopy(bufferArgsTensor[i * 4], bufferArgsGT[2 * targetRank], 4);
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
    uint32_t targetRank = 0;
    __gm__ T* cclGMOther = 0;
    GM_ADDR ctrlFlagGMOther = 0;

    for (uint32_t loop = 0; loop < maxTargetLoopNum_; loop++) {
        // Step1. 本卡userIn -> 本卡ccl&&output
        for (uint32_t i = 0; i < numTargets; ++i) {
            if (loop >= targetLoopNum_[i]){
                continue;
            }

            targetRank = targetRanks[i];
            cclGMOther = (__gm__ T *)(buffersIn[i]);
            ctrlFlagGMOther = buffersOut[i];
            if (loop == targetLoopNum_[i] - 1) {
                curCount = countTail_[i];
                curBlockOffset = blockOffsetTail;
            } else {
                curCount = countMid_[i];
                curBlockOffset = blockOffsetMid;
            }

            if (targetRank == rank_){
                CpGM2GM(outputGM + curOffset + curBlockOffset, inputGM + extraArgs->sendDispls[rank_] + curOffset + curBlockOffset, curCount);
                PipeBarrier<PIPE_ALL>();

                Record1vN(curTag, CommPattern::interRank, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();
            } else {
                CpGM2GM(cclGMSelf + targetRank * avgBufferCount + curBlockOffset, inputGM + extraArgs->sendDispls[targetRank] + curOffset + curBlockOffset, curCount);
                PipeBarrier<PIPE_ALL>();

                Record(curTag, ctrlFlagGMOther, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();
            }
        }

        // Step2. 对端ccl -> 本卡output
        for (uint32_t i = 0; i < numTargets; ++i) {
            if (loop >= targetLoopNum_[numTargets]){
                continue;
            }

            targetRank = targetRanks[i];
            cclGMOther = (__gm__ T *)(buffersIn[i]);
            ctrlFlagGMOther = buffersOut[i];
            if (loop == targetLoopNum_[numTargets] - 1) {
                curCount = countTail_[numTargets];
                curBlockOffset = selfBlockOffsetTail_;
            } else {
                curCount = countMid_[numTargets];
                curBlockOffset = selfBlockOffsetMid_;
            }

            if (targetRank != rank_){
                Wait(curTag, targetRank, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();
                WaitGENv1(curTag, flagAddrSelf_, false, AivNotifyType::ACK);
                PipeBarrier<PIPE_ALL>();

                CpGM2GM(outputGM + curOffset + curBlockOffset, cclGMOther + rank_ * avgBufferCount + curBlockOffset, curCount, true, reduceOp_);
                PipeBarrier<PIPE_ALL>();

                Record(curTag, ctrlFlagGMOther, AivNotifyType::DataSignal);
                PipeBarrier<PIPE_ALL>();
            }
        }

        for (uint32_t i = 0; i < numTargets; ++i) {
            if (targetRanks[i]!= rank_ && loop < targetLoopNum_[i]){
                Wait(curTag, targetRanks[i], AivNotifyType::DataSignal);
                PipeBarrier<PIPE_ALL>();
            }
        }

        curOffset += avgBufferCount;
        curTag += 1;
    }
}

template<typename T>
__aicore__ inline void aiv_reduce_scatter_v_crossnode_91093(KERNEL_ARGS_DEF_A3, const ExtraArgsV2* extraArgs)
{
    AivReduceScatterVCrossNode91093 op;

    uint64_t avgBufferCount = (uint64_t) bufferSize / rankSize / UB_ALIGN_SIZE * UB_ALIGN_SIZE / sizeof(T);

    op.InitSelf(buffOut0, rank, rankSize, tag, false);
    op.InitDataCopyOffset<T>(extraArgs, avgBufferCount);
    op.InitOpCounter(headCountMem, tailCountMem, addOneMem, SIZE_OF_INT32, isEnableCounter);
    op.HeadCounter();
    op.Process<T>(buffIn0, buffOut0, buffOut1, input, output, tag, avgBufferCount, extraArgs);
    op.TailCounter();
}