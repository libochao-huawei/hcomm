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

__aicore__ inline void AivSync910B::Process(int32_t tag)
{
    if (tag == 1) {
        SyncBarrier(1);
    } else if (tag == IDX_2) {
        ClearGM();
    } else if (tag == IDX_3) {
        SyncBarrier(IDX_2);
    }
}

__aicore__ inline void aiv_sync_910b_inner(KERNEL_ARGS_DEF)
{
    AivSync910B op;
    op.Init(KERNEL_CLASS_INIT, false);
    op.Process(tag);
}
