/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_communication_base.h"

using namespace AscendC;

class AivSync910B : public AivCommBase {
public:
    __aicore__ inline AivSync910B() {}
    __aicore__ inline void SyncBarrier(int32_t tag);
    __aicore__ inline void ClearGM();
    __aicore__ inline void LocalRecord(uint32_t tag, uint32_t waitBlock, AivNotifyType notifyType, bool ifSet = true);
    __aicore__ inline void LocalWait(uint32_t tag, AivNotifyType notifyType, bool ifClear = false);
    __aicore__ inline void localMultiRecord(uint32_t tag, int32_t blockGroup, AivNotifyType notifyType);
    __aicore__ inline void LocalMultiWaitRecord(uint32_t tag, AivNotifyType notifyType, int32_t blockGroup, bool ifClear);
    __aicore__ inline void SyncAllCycle(AivNotifyType notifyType);
    __aicore__ inline void Process(int32_t tag);
};

__aicore__ inline void AivSync910B::SyncBarrier(int32_t tag)
{
    // 从0开始，用4个flag
    uint32_t flagOffset = SYNC_BUFFER_OFFSET;
    flagOffset += ((tag % AIV_PING_PONG_FACTOR_TWO == 0) ? 0 : rankSize_ * FLAG_SIZE);
    if (blockIdx_ != rank_) {
        // 卡间同步
        SetSignalValue((__gm__ int32_t *)(GM_OUT[blockIdx_] + flagOffset + rank_ * FLAG_SIZE), localSetTensor, 1);
        WaitSignalValue((__gm__ int32_t *)(GM_OUT[rank_] + flagOffset + blockIdx_ * FLAG_SIZE), localCheckTensor, 1);
        PipeBarrier<PIPE_ALL>();
        SetSignalValue((__gm__ int32_t *)(GM_OUT[rank_] + flagOffset + blockIdx_ * FLAG_SIZE), localSetTensor, 0);
    }
}

__aicore__ inline void AivSync910B::ClearGM()
{
    uint32_t emptyOffset = CLEAR_BUFFER_OFFSET;
    uint32_t blockCount = BUFFER_AREA / rankSize_;
    uint32_t blockOffset = blockCount * blockIdx_;
    CpGM2GM(GM_OUT[rank_] + blockOffset, GM_OUT[rank_] + blockOffset + emptyOffset, blockCount);
}

__aicore__ inline void AivSync910B::LocalRecord(uint32_t tag, uint32_t waitBlock, AivNotifyType notifyType, bool ifSet)
{
    int32_t recordOffset = localOffset + waitBlock * FLAG_SIZE + (int32_t(notifyType) % 3)* MAX_NUM_BLOCKS * FLAG_SIZE +
        (int32_t(notifyType) / 3) * 2560 * 1024;
    __gm__ int32_t *ctrlFlagGM = (__gm__ int32_t *)(GM_OUT[rank_]  + recordOffset);
    SetSignalValue(ctrlFlagGM, localSetTensor, tag, ifSet);
}

__aicore__ inline void AivSync910B::LocalWait(uint32_t tag, AivNotifyType notifyType, bool ifClear)
{
    int32_t waitOffset = localOffset + blockIdx_ * FLAG_SIZE + (int32_t(notifyType) % 3)* MAX_NUM_BLOCKS * FLAG_SIZE +
        (int32_t(notifyType) / 3) * 2560 * 1024;
    __gm__ int32_t *ctrlFlagGM = (__gm__ int32_t *)(GM_OUT[rank_]  + waitOffset );
    WaitSignalValue(ctrlFlagGM, localCheckTensor, tag);
    PipeBarrier<PIPE_ALL>();
    if (ifClear) {
        SetSignalValue(ctrlFlagGM, localSetTensor, 0);
    }  
}

__aicore__ inline void AivSync910B::localMultiRecord(uint32_t tag, int32_t blockGroup, AivNotifyType notifyType)
{ 
    localSetTensor.SetValue(0, tag);
    SyncFunc<HardEvent::S_MTE3>();
    for (int32_t i = 0; i < blockGroup; i++) {
        LocalRecord(tag, blockIdx_ + i, notifyType, false);
    }
}

__aicore__ inline void AivSync910B::LocalMultiWaitRecord(uint32_t tag, AivNotifyType notifyType, int32_t blockGroup, bool ifClear)
{ 
    int32_t waitOffset = localOffset + (int32_t(notifyType) % 3)* MAX_NUM_BLOCKS * FLAG_SIZE +
        (int32_t(notifyType) / 3) * 2560 * 1024;
    __gm__ int32_t *ctrlFlagGM = (__gm__ int32_t *)(GM_OUT[rank_]  + waitOffset);
    GlobalTensor<int32_t> globalTensor;
    globalTensor.SetGlobalBuffer(ctrlFlagGM, UB_FLAG_PAD_COUNT * blockGroup);

    while (true) {
        DataCopy(localCheckTensor, globalTensor, UB_FLAG_PAD_COUNT * blockGroup);
        SyncFunc<HardEvent::MTE2_S>();
        int32_t sum = 0;
        for (int32_t i = 1; i < blockGroup; i++) {
            sum += localCheckTensor.GetValue(UB_FLAG_PAD_COUNT * i);
        } 
        if (sum == (blockGroup - 1) * tag) {
            break;
        }
    }
    PipeBarrier<PIPE_ALL>();
    if (!ifClear) {
        localMultiRecord(tag + 1, blockGroup, notifyType);
    } else {
        localMultiRecord(0, blockGroup, notifyType);
    }
    return;
}

__aicore__ inline void AivSync910B::SyncAllCycle(AivNotifyType notifyType)
{
    LocalRecord(1, blockIdx_, notifyType);
    if (blockIdx_ == 0) {
        LocalMultiWaitRecord(1, notifyType, rankSize_, false);
    } 
    LocalWait(IDX_2, notifyType, true);
}

__aicore__ inline void AivSync910B::Process(int32_t tag)
{
    SyncBarrier(1);
    PipeBarrier<PIPE_ALL>();
    SyncAllCycle(AivNotifyType::ClearACK);
    PipeBarrier<PIPE_ALL>();
    ClearGM();
    PipeBarrier<PIPE_ALL>();
    SyncAllCycle(AivNotifyType::ClearDataSingal);
    PipeBarrier<PIPE_ALL>();
    SyncBarrier(IDX_2);
}

__aicore__ inline void aiv_sync_910b_inner(KERNEL_ARGS_DEF)
{
    AivSync910B op;
    op.Init(KERNEL_CLASS_INIT, false);
    op.Process(tag);
}
