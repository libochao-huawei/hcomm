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

class AivBroadcastCrossNode91093 : public AivCrossNode91093Base {
public:
    __aicore__ inline AivBroadcastCrossNode91093() {}

    template<typename T>
    __aicore__ inline void Process(GM_ADDR buffIn0, GM_ADDR buffOut0, GM_ADDR commInfoAddr,
        GM_ADDR input, GM_ADDR output, int32_t tag, uint64_t len, uint64_t maxCountPerLoop, uint32_t root);

    __aicore__ inline void InitSelf(GM_ADDR buffOut0, uint32_t rank, uint32_t rankSize,
    int32_t tag, bool useDoubleBuffer, uint32_t root);

    __aicore__ inline void InitDataCopyOffset(uint64_t len, uint64_t maxCountPerLoop);

    __aicore__ inline void WaitRecordSync(int32_t tag, uint32_t root, GM_ADDR rootAddr,
        GM_ADDR* buffersOut);

    __aicore__ inline void CalcNumTargetsAndTargetRanks(uint32_t root);
};

__aicore__ inline void AivBroadcastCrossNode91093::InitSelf(GM_ADDR buffOut0, uint32_t rank, uint32_t rankSize,
    int32_t tag, bool useDoubleBuffer, uint32_t root)
{
    flagAddrSelf_ = buffOut0;
    rank_ = rank;
    tag_ = tag;
    rankSize_ = rankSize;
    useDoubleBuffer_ = useDoubleBuffer;
    usedBlockNum_ = block_num;
    pingpongOffset = 0;

    InitSetCheckClearArgsTensor();
    if (rankSize-1 > block_num ) {
        blockNumPerGroup = 1;
    }else{
        blockNumPerGroup = block_num / (rankSize_-1);
    }
    CalcNumTargetsAndTargetRanks(root);
    InitOffset();
}

__aicore__ inline void AivBroadcastCrossNode91093::CalcNumTargetsAndTargetRanks(uint32_t root)
{
    numTargets = (rankSize_ - 1) / block_num;
    uint32_t tailRankSize = (rankSize_ - 1) % block_num;
    if (tailRankSize > 0 && block_idx < tailRankSize) {
        numTargets += 1;
    }

    for (uint32_t i = 0; i < numTargets; i++) {
        uint32_t targetRank =  (block_idx + i * block_num) % rankSize_;
        if (targetRank >= root){
            targetRank += 1;
        }
        targetRanks[i] = targetRank;
    }
}

__aicore__ inline void AivBroadcastCrossNode91093::InitDataCopyOffset(uint64_t len, uint64_t maxCountPerLoop)
{
    uint64_t countPerRank = maxCountPerLoop / (rankSize_ - 1);

    if (len < maxCountPerLoop){
        countMid = 0;
        countTail = len / (rankSize_ - 1);
        countTailLast_ = len - (rankSize_ - 2) * countTail;
    } else if (len % maxCountPerLoop == 0){
        countMid = countPerRank;
        countTail = countPerRank;
        countTailLast_ = countPerRank;
    } else {
        countMid = countPerRank;
        uint64_t remainLen = len % maxCountPerLoop;
        countTail = remainLen / (rankSize_ - 1);
        countTailLast_ = remainLen - (rankSize_ - 2) * countTail;
    }
}

__aicore__ inline void AivBroadcastCrossNode91093::WaitRecordSync(int32_t tag, uint32_t root, GM_ADDR rootAddr,
    GM_ADDR* buffersOut)
{
    for (uint32_t i = 0; i < numTargets; ++i) {
        int32_t targetRank = targetRanks[i];
        if ((rank_ < root && targetRank == rank_) || (rank_ > root && targetRank == rank_)) {
            Record(tag, rootAddr, AivNotifyType::DataSignal);
            PipeBarrier<PIPE_ALL>();
        } else if(rank_ != root) {
            Record(tag, buffersOut[i], AivNotifyType::DataSignal);
            PipeBarrier<PIPE_ALL>();
        } else {
            Wait(tag, targetRank, AivNotifyType::DataSignal);
            PipeBarrier<PIPE_ALL>();
        }
    }

    for (uint32_t i = 0; i < numTargets; ++i) {
        int32_t targetRank = targetRanks[i];
        if ((rank_ < root && targetRank == rank_) || (rank_ > root && targetRank == rank_)) {
            for(uint32_t remoteRank = 0; remoteRank < rankSize_; remoteRank += 1){
                if (targetRank != root && targetRank != rank_) {
                    Wait(tag, targetRank, AivNotifyType::DataSignal);
                }
            }
        }
    }
}

template<typename T>
__aicore__ inline void AivBroadcastCrossNode91093::Process(GM_ADDR buffIn0, GM_ADDR buffOut0, GM_ADDR commInfoAddr,
    GM_ADDR input, GM_ADDR output, int32_t tag, uint64_t len, uint64_t maxCountPerLoop, uint32_t root)
{
    // 内存准备
    __gm__ T *inputGM = (__gm__ T *)input;
    __gm__ T *outputGM = (__gm__ T *)output;
    __gm__ T *cclGMSelf = (__gm__ T *)buffIn0;

    GlobalTensor<uint64_t> bufferArgsGT;
    __gm__ uint64_t *buffersGmAddr = (__gm__ uint64_t *)(commInfoAddr);
    bufferArgsGT.SetGlobalBuffer(buffersGmAddr, FLAG_SIZE * rankSize_ / sizeof(uint64_t));

    // 准备参数，buffer地址和最大收发count
    GM_ADDR buffersIn[MAX_TARGET_NUM + 1] = {};
    GM_ADDR buffersOut[MAX_TARGET_NUM + 1] = {};

    for (uint32_t i = 0; i < numTargets; i++) {
        uint32_t targetRank = targetRanks[i];
        DataCopy(bufferArgsTensor[i * 4], bufferArgsGT[2 * targetRank], 4);
    }
    DataCopy(bufferArgsTensor[numTargets * 4], bufferArgsGT[2 * root], 4);
    SyncFunc<HardEvent::MTE2_S>();

    for (uint32_t i = 0; i <= numTargets; i++) {
        uint32_t curIdx = i * 4;
        buffersIn[i] = (GM_ADDR)(bufferArgsTensor.GetValue(curIdx));
        buffersOut[i] = (GM_ADDR)(bufferArgsTensor.GetValue(curIdx + 1));
    }
    __gm__ T *cclGMRoot = (__gm__ T *)(buffersIn[numTargets]);
    GM_ADDR ctrlFlagGMRoot =  buffersOut[numTargets];

    int32_t curTag = (tag << TAG_MOVE_LEFT_BITS);
    uint64_t curOffset = 0;
    uint64_t curCount = 0;

    uint64_t bufferLoopNum = CeilDiv(len, maxCountPerLoop);
    for (uint64_t loop = 0; loop < bufferLoopNum; loop++) {
        if (loop == bufferLoopNum - 1){
            curCount = countTail;
        }else{
            curCount = countMid;
        }

        for (uint32_t i=0; i<numTargets; ++i) {
            __gm__ T *cclGMOther = (__gm__ T*)(buffersIn[i]);
            GM_ADDR ctrlFlagGMOther = buffersOut[i];

            uint32_t targetRank = targetRanks[i];
            uint64_t dataCopyOffset = targetRank * curCount;
            if (targetRank > root){
                dataCopyOffset = (targetRank - 1) * curCount;
            }

            if ( loop == bufferLoopNum - 1 ) {
                if ((root == rankSize_ - 1 && targetRank == rankSize_ - 2) ||
                    (root < rankSize_ - 1 && targetRank == rankSize_ - 1 )) {
                    curCount = countTailLast_;
                }
            }

            // root卡cclBuffer -> 本端(cclbuffer && output)
            if ((rank_ < root && targetRank == rank_) || (rank_ > root && targetRank == rank_)) {
                // 等待root就绪
                CountWaitGE(ctrlFlagGMRoot, curTag, targetRank);
                PipeBarrier<PIPE_ALL>();

                CpGM2GM(cclGMSelf + dataCopyOffset, cclGMRoot + dataCopyOffset, curCount);
                PipeBarrier<PIPE_ALL>();

                CountRecord(curTag, targetRank);
                PipeBarrier<PIPE_ALL>();

                CpGM2GM(outputGM + curOffset + dataCopyOffset, cclGMSelf + dataCopyOffset, curCount);
                PipeBarrier<PIPE_ALL>();
            }// 对端cclbuffer -> 本端output
            else if(rank_ != root) {
                CountWaitGE(ctrlFlagGMOther, curTag, targetRank);
                PipeBarrier<PIPE_ALL>();

                CpGM2GM(outputGM + curOffset + dataCopyOffset, cclGMOther + dataCopyOffset, curCount);
                PipeBarrier<PIPE_ALL>();
            }// root卡userInput -> root卡cclBuffer
            else {
                CpGM2GM(cclGMRoot + dataCopyOffset, inputGM + curOffset + dataCopyOffset, curCount);
                PipeBarrier<PIPE_ALL>();

                // root已就绪
                CountRecord(curTag, targetRank);
                PipeBarrier<PIPE_ALL>();
            }
        }
        // 尾同步
        WaitRecordSync(curTag, root, ctrlFlagGMRoot, buffersOut);

        curOffset += maxCountPerLoop;
        curTag += 1;
    }
}

template<typename T>
__aicore__ inline void aiv_broadcast_crossnode_91093(KERNEL_ARGS_DEF_A3)
{
    AivBroadcastCrossNode91093 op;

    uint64_t basicLoopCount = (rankSize - 1) * UB_MAX_DATA_SIZE / sizeof(T);
    uint64_t bufferCount = (uint64_t)bufferSize / sizeof(T);
    uint64_t maxCountPerLoop = bufferCount / basicLoopCount  * basicLoopCount;
    if (bufferCount < basicLoopCount) {
        maxCountPerLoop = basicLoopCount;
    }

    op.InitSelf(buffOut0, rank, rankSize, tag, false, root);
    op.InitDataCopyOffset(len, maxCountPerLoop);
    op.InitOpCounter(headCountMem, tailCountMem, addOneMem, SIZE_OF_INT32, isEnableCounter);
    op.HeadCounter();
    op.Process<T>(buffIn0, buffOut0, buffOut1, input, output, tag, len, maxCountPerLoop, root);
    op.TailCounter();
}