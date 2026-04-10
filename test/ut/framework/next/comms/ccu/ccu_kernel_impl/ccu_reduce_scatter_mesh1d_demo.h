/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_REDUCE_SCATTER_MESH1D_DEMO_H
#define CCU_REDUCE_SCATTER_MESH1D_DEMO_H

#include "ccu_api.hpp"
#include "ccu_log.h"

#include <cstdint>
#include <climits>
#include <vector>
#include <string>
#include <set>

// ============================================================================
// 常量定义
// ============================================================================

constexpr uint32_t RS_MAX_RANK_SIZE = 16;

constexpr int RS_INPUT_XN_ID   = 0;
constexpr int RS_SCRATCH_XN_ID = 1;
constexpr int RS_TOKEN_XN_ID   = 2;
constexpr int RS_POST_SYNC_ID  = 3;

constexpr int RS_CKE_IDX_0 = 0;

constexpr uint64_t RS_CCU_MS_SIZE               = 4096;
constexpr uint64_t RS_CCU_MS_INTERLEAVE         = 8;
constexpr uint64_t RS_CCU_MS_DEFAULT_LOOP_COUNT = 64;

// ============================================================================
// 参数结构体
// ============================================================================

struct ReduceScatterKernelArg {
    uint64_t      rankSize;
    uint32_t      rankId;
    ChannelHandle channels[RS_MAX_RANK_SIZE];
    uint32_t      channelCount;
    HcclDataType  dataType;
    HcclDataType  outputDataType;
    HcclReduceOp  reduceOp;
};

struct GroupOpSizeVars {
    CcuVariable addrOffset;
    CcuVariable loopParam;
    CcuVariable parallelParam;
    CcuVariable residual;
};

// ============================================================================
// 运行时配置（对应 CcuKernelAlgBase::moConfig / moRes）
// ============================================================================

struct LoopGroupConfig {
    uint64_t msInterleave;
    uint64_t loopCount;
    uint64_t memSlice;
};

struct LoopGroupResource {
    CcuEvent  completedEvent[RS_CCU_MS_DEFAULT_LOOP_COUNT];
    CcuBuffer ccuBuf[RS_CCU_MS_DEFAULT_LOOP_COUNT * RS_CCU_MS_INTERLEAVE];
    uint32_t  eventCount;
    uint32_t  bufCount;
};

// ============================================================================
// Bit-packing 辅助函数（对应 ccu_kernel_utils.cc）
// ============================================================================

static inline constexpr uint64_t SetBits(uint16_t end)
{
    return ((uint64_t(1) << (end + 1)) - uint64_t(1));
}

static inline uint64_t GetMaxLoopIterNum()
{
    constexpr uint16_t loopNumBitNum = 12;
    return SetBits(loopNumBitNum);
}

static inline uint64_t GetLoopParam(uint64_t loopCtxId, uint64_t gsaOffset, uint64_t loopIterNum)
{
    constexpr uint16_t ctxIdBitNum     = 8;
    constexpr uint16_t ctxIdShiftBit   = 45;
    constexpr uint16_t gsaBitNum       = 32;
    constexpr uint16_t gsaShiftBit     = 13;
    constexpr uint16_t loopNumBitNum   = 13;
    constexpr uint16_t loopNumShiftBit = 0;
    return ((loopCtxId & SetBits(ctxIdBitNum)) << ctxIdShiftBit) |
           ((gsaOffset & SetBits(gsaBitNum)) << gsaShiftBit) |
           ((loopIterNum & SetBits(loopNumBitNum)) << loopNumShiftBit);
}

static inline uint64_t GetParallelParam(uint64_t repeatNum, uint64_t repeatLoopIndex, uint64_t totalLoopNum)
{
    constexpr uint16_t repeatBitNum       = 7;
    constexpr uint16_t repeatNumShiftBit  = 55;
    constexpr uint16_t repeatLoopBitNum   = 7;
    constexpr uint16_t repeatLoopShiftBit = 48;
    constexpr uint16_t totalLoopBitNum    = 7;
    constexpr uint16_t totalLoopShiftBit  = 41;
    return ((repeatNum & SetBits(repeatBitNum)) << repeatNumShiftBit) |
           ((repeatLoopIndex & SetBits(repeatLoopBitNum)) << repeatLoopShiftBit) |
           ((totalLoopNum & SetBits(totalLoopBitNum)) << totalLoopShiftBit);
}

static inline uint64_t GetOffsetParam(uint64_t gsaOffset, uint64_t msOffset, uint64_t ckeOffset)
{
    constexpr uint16_t gsaBitNum   = 32;
    constexpr uint16_t gsaShiftBit = 21;
    constexpr uint16_t msBitNum    = 11;
    constexpr uint16_t msShiftBit  = 10;
    constexpr uint16_t ckeBitNum   = 10;
    constexpr uint16_t ckeShiftBit = 0;
    return ((gsaOffset & SetBits(gsaBitNum)) << gsaShiftBit) |
           ((msOffset & SetBits(msBitNum)) << msShiftBit) |
           ((ckeOffset & SetBits(ckeBitNum)) << ckeShiftBit);
}

static inline uint64_t GetExpansionParam(uint64_t expansionNum)
{
    constexpr uint64_t expansionNum2        = 2;
    constexpr uint64_t expansionNumShiftBit = 53;
    return (expansionNum == expansionNum2 ? uint64_t(1) : uint64_t(2)) << expansionNumShiftBit;
}

static inline uint32_t GetReduceExpansionNum(HcclReduceOp reduceOp, HcclDataType dataType, HcclDataType outputDataType)
{
    (void)reduceOp;
    (void)dataType;
    (void)outputDataType;
    // 简化实现：大多数场景 expansionNum = 1
    // 完整实现需要 DataTypeSizeGet(outputDataType) / DataTypeSizeGet(dataType)
    return 1;
}

// ============================================================================
// CalGoSize（对应 CcuKernelAlgBase::CalGoSize，host 侧调用）
// ============================================================================

static inline std::vector<uint64_t> CalGoSize(uint64_t size, const LoopGroupConfig &config)
{
    uint64_t loopSize = config.loopCount * config.memSlice;
    uint64_t maxSize  = loopSize * (GetMaxLoopIterNum() + 1);

    uint64_t m = size / loopSize;
    uint64_t n = (size - m * loopSize) / config.memSlice;
    uint64_t p = size - m * loopSize - n * config.memSlice;

    if (size == maxSize) {
        m = GetMaxLoopIterNum();
        n = config.loopCount - 1;
        p = config.memSlice;
    }

    uint64_t offset      = config.memSlice * config.loopCount * m;
    uint64_t loopIterNum = m;

    uint64_t loopExtendNum = 0;
    uint64_t tailSize      = 0;

    if (n == 0 && p == 0) {
        loopExtendNum = 0;
        tailSize      = 0;
    } else if (n != 0 && p == 0) {
        loopExtendNum = GetParallelParam(n - 1, 0, 1);
        tailSize      = config.memSlice;
    } else if (n == 0 && p != 0) {
        loopExtendNum = GetParallelParam(0, 0, 1);
        tailSize      = p;
    } else {
        loopExtendNum = GetParallelParam(n - 1, 1, 2);
        tailSize      = p;
    }

    return {offset, loopIterNum, loopExtendNum, tailSize};
}

// ============================================================================
// AllocGoResource（对应 CcuKernelAlgBase::AllocGoResource）
// ============================================================================

static inline CcuResult AllocGoResource(LoopGroupConfig &config, LoopGroupResource &res,
    bool &allocated, uint32_t parallelDim = RS_CCU_MS_DEFAULT_LOOP_COUNT, uint32_t msPerLoop = 1)
{
    if (allocated) {
        return CCU_SUCCESS;
    }

    config.msInterleave = RS_CCU_MS_INTERLEAVE;
    config.loopCount    = parallelDim;
    config.memSlice     = msPerLoop * RS_CCU_MS_SIZE;

    res.eventCount = config.loopCount;
    CCU_CHK_RET(ccu::BlockCreate(res.completedEvent, res.eventCount));

    res.bufCount = config.loopCount * config.msInterleave;
    CCU_CHK_RET(ccu::BlockCreate(res.ccuBuf, res.bufCount));

    allocated = true;
    return CCU_SUCCESS;
}

// ============================================================================
// InitResource
// ============================================================================

struct ReduceScatterContext {
    const ReduceScatterKernelArg *arg;

    CcuVariable input[RS_MAX_RANK_SIZE];
    CcuVariable scratch[RS_MAX_RANK_SIZE];
    CcuVariable token[RS_MAX_RANK_SIZE];
    CcuVariable output;
    CcuVariable currentRankSliceInputOffset;
    CcuVariable currentRankSliceOutputOffset;
    CcuVariable normalSliceSize;
    CcuVariable lastSliceSize;
    CcuVariable inputRepeatStride;
    CcuVariable outputRepeatStride;
    CcuVariable repeatNum;
    CcuVariable flag;
    GroupOpSizeVars goSize;

    uint16_t selfBit;
    uint16_t allBit;

    CcuLocalAddr  myInput;
    CcuRemoteAddr remoteInput[RS_MAX_RANK_SIZE];
    CcuLocalAddr  scratchMem[RS_MAX_RANK_SIZE];
    CcuEvent      event;

    LoopGroupConfig  moConfig;
    LoopGroupResource moRes;
    bool resourceAllocated;

    CcuLoopHandle reduceLoops[2];
    bool loopRegistered;

    // Loop body 中的外部 LocalAddr（每个 loop index 各两组）
    CcuLocalAddr loopDst[2];
    CcuLocalAddr loopSrc[2];
    CcuLocalAddr loopScratch[2][RS_MAX_RANK_SIZE];
    CcuVariable  loopLen[2];
    CcuVariable  loopLenExp[2];
};

static CcuResult InitResource(ReduceScatterContext &ctx)
{
    const auto *arg = ctx.arg;
    uint32_t channelIdx = 0;

    if (arg->channelCount == 0) {
        return CCU_E_PARA;
    }

    for (uint64_t peerId = 0; peerId < arg->rankSize; peerId++) {
        if (peerId == arg->rankId) {
            CCU_CHK_RET(ccu::Create(&ctx.input[peerId]));
            CCU_CHK_RET(ccu::Create(&ctx.scratch[peerId]));
            CCU_CHK_RET(ccu::Create(&ctx.token[peerId]));
        } else {
            CCU_CHK_RET(ccu::CreateFromChannel(
                arg->channels[channelIdx], RS_INPUT_XN_ID, &ctx.input[peerId]));
            CCU_CHK_RET(ccu::CreateFromChannel(
                arg->channels[channelIdx], RS_SCRATCH_XN_ID, &ctx.scratch[peerId]));
            CCU_CHK_RET(ccu::CreateFromChannel(
                arg->channels[channelIdx], RS_TOKEN_XN_ID, &ctx.token[peerId]));
            channelIdx++;
        }
    }

    CCU_CHK_RET(ccu::Create(&ctx.output));
    CCU_CHK_RET(ccu::Create(&ctx.currentRankSliceInputOffset));
    CCU_CHK_RET(ccu::Create(&ctx.currentRankSliceOutputOffset));
    CCU_CHK_RET(ccu::Create(&ctx.normalSliceSize));
    CCU_CHK_RET(ccu::Create(&ctx.lastSliceSize));
    CCU_CHK_RET(ccu::Create(&ctx.inputRepeatStride));
    CCU_CHK_RET(ccu::Create(&ctx.outputRepeatStride));
    CCU_CHK_RET(ccu::Create(&ctx.repeatNum));
    CCU_CHK_RET(ccu::Create(&ctx.flag));

    CCU_CHK_RET(ccu::Create(&ctx.goSize.addrOffset));
    CCU_CHK_RET(ccu::Create(&ctx.goSize.loopParam));
    CCU_CHK_RET(ccu::Create(&ctx.goSize.parallelParam));
    CCU_CHK_RET(ccu::Create(&ctx.goSize.residual));

    ctx.selfBit = 1 << arg->rankId;
    ctx.allBit  = ((1 << arg->rankSize) - 1) & (~(1 << arg->rankId));

    for (uint64_t rankIdx = 0; rankIdx < arg->rankSize; rankIdx++) {
        CCU_CHK_RET(ccu::Create(&ctx.scratchMem[rankIdx]));
        if (rankIdx == arg->rankId) {
            CCU_CHK_RET(ccu::Create(&ctx.myInput));
        } else {
            CCU_CHK_RET(ccu::Create(&ctx.remoteInput[rankIdx]));
        }
    }

    CCU_CHK_RET(ccu::Create(&ctx.event));

    ctx.resourceAllocated = false;
    ctx.loopRegistered    = false;

    return CCU_SUCCESS;
}

// ============================================================================
// LoadArgs
// ============================================================================

static CcuResult LoadArgs(ReduceScatterContext &ctx)
{
    const auto *arg = ctx.arg;

    CCU_CHK_RET(ccu::LoadArg(ctx.input[arg->rankId]));
    CCU_CHK_RET(ccu::LoadArg(ctx.output));
    CCU_CHK_RET(ccu::LoadArg(ctx.token[arg->rankId]));
    CCU_CHK_RET(ccu::LoadArg(ctx.scratch[arg->rankId]));
    CCU_CHK_RET(ccu::LoadArg(ctx.currentRankSliceInputOffset));
    CCU_CHK_RET(ccu::LoadArg(ctx.currentRankSliceOutputOffset));
    CCU_CHK_RET(ccu::LoadArg(ctx.inputRepeatStride));
    CCU_CHK_RET(ccu::LoadArg(ctx.outputRepeatStride));
    CCU_CHK_RET(ccu::LoadArg(ctx.normalSliceSize));
    CCU_CHK_RET(ccu::LoadArg(ctx.lastSliceSize));
    CCU_CHK_RET(ccu::LoadArg(ctx.repeatNum));

    CCU_CHK_RET(ccu::LoadArg(ctx.goSize.addrOffset));
    CCU_CHK_RET(ccu::LoadArg(ctx.goSize.loopParam));
    CCU_CHK_RET(ccu::LoadArg(ctx.goSize.parallelParam));
    CCU_CHK_RET(ccu::LoadArg(ctx.goSize.residual));

    return CCU_SUCCESS;
}

// ============================================================================
// PreSync / PostSync
// ============================================================================

static void PreSync(ReduceScatterContext &ctx)
{
    const auto *arg = ctx.arg;

    for (uint32_t i = 0; i < arg->channelCount; i++) {
        ccu::WriteVariableWithNotify(arg->channels[i], ctx.input[arg->rankId],
            RS_INPUT_XN_ID, RS_CKE_IDX_0, 1 << RS_INPUT_XN_ID);
        ccu::WriteVariableWithNotify(arg->channels[i], ctx.scratch[arg->rankId],
            RS_SCRATCH_XN_ID, RS_CKE_IDX_0, 1 << RS_SCRATCH_XN_ID);
        ccu::WriteVariableWithNotify(arg->channels[i], ctx.token[arg->rankId],
            RS_TOKEN_XN_ID, RS_CKE_IDX_0, 1 << RS_TOKEN_XN_ID);
    }

    uint32_t allBit = (1 << RS_INPUT_XN_ID) | (1 << RS_SCRATCH_XN_ID) | (1 << RS_TOKEN_XN_ID);
    for (uint32_t i = 0; i < arg->channelCount; i++) {
        ccu::NotifyWait(arg->channels[i], RS_CKE_IDX_0, allBit);
    }
}

static void PostSync(ReduceScatterContext &ctx)
{
    const auto *arg = ctx.arg;

    for (uint32_t i = 0; i < arg->channelCount; i++) {
        ccu::WriteNotify(arg->channels[i], RS_CKE_IDX_0, 1 << RS_POST_SYNC_ID);
    }
    for (uint32_t i = 0; i < arg->channelCount; i++) {
        ccu::NotifyWait(arg->channels[i], RS_CKE_IDX_0, 1 << RS_POST_SYNC_ID);
    }
}

// ============================================================================
// CreateReduceLoop
// ============================================================================

static CcuResult CreateReduceLoop(ReduceScatterContext &ctx)
{
    const auto *arg = ctx.arg;
    const uint32_t size = arg->rankSize;

    constexpr uint32_t LOOP_NUM = 16;
    CCU_CHK_RET(AllocGoResource(ctx.moConfig, ctx.moRes, ctx.resourceAllocated, LOOP_NUM));

    if (ctx.loopRegistered) {
        return CCU_SUCCESS;
    }

    uint32_t expansionNum = GetReduceExpansionNum(arg->reduceOp, arg->dataType, arg->outputDataType);
    uint32_t usedBufNum   = size > expansionNum ? size : expansionNum;

    for (int32_t index = 0; index < 2; index++) {
        CCU_CHK_RET(ccu::Create(&ctx.loopDst[index]));
        CCU_CHK_RET(ccu::Create(&ctx.loopSrc[index]));
        for (uint32_t i = 0; i < size; i++) {
            CCU_CHK_RET(ccu::Create(&ctx.loopScratch[index][i]));
        }
        CCU_CHK_RET(ccu::Create(&ctx.loopLen[index]));
        CCU_CHK_RET(ccu::Create(&ctx.loopLenExp[index]));

        uint32_t bufBase = index * ctx.moConfig.msInterleave;

        CcuEvent loopEvt = ctx.moRes.completedEvent[index];

        CCU_LOOPBODY(ctx.reduceLoops[index]) {
            for (uint32_t i = 0; i < size; i++) {
                loopEvt.setMask(1 << i);
                if (i == arg->rankId) {
                    ccu::Copy(
                        ctx.moRes.ccuBuf[bufBase + i], ctx.loopSrc[index],
                        ctx.loopLen[index], loopEvt);
                } else {
                    ccu::Copy(
                        ctx.moRes.ccuBuf[bufBase + i], ctx.loopScratch[index][i],
                        ctx.loopLen[index], loopEvt);
                }
            }
            loopEvt.setMask((1 << size) - 1);
            ccu::Wait(loopEvt);

            if (size > 1) {
                loopEvt.setMask(1);
                ccu::Reduce(
                    &ctx.moRes.ccuBuf[bufBase], size,
                    arg->dataType, arg->outputDataType, arg->reduceOp,
                    ctx.loopLen[index], loopEvt);
                ccu::Wait(loopEvt);
            }

            loopEvt.setMask(1);
            ccu::Copy(
                ctx.loopDst[index], ctx.moRes.ccuBuf[bufBase],
                ctx.loopLenExp[index], loopEvt);
            ccu::Wait(loopEvt);
        }
    }

    ctx.loopRegistered = true;
    return CCU_SUCCESS;
}

// ============================================================================
// ReduceLoopGroup
// ============================================================================

static CcuResult ReduceLoopGroup(ReduceScatterContext &ctx,
    CcuLocalAddr outDstOrg, CcuLocalAddr srcOrg,
    CcuLocalAddr *scratchOrg, uint32_t scratchCount,
    GroupOpSizeVars &goSize)
{
    const auto *arg = ctx.arg;
    const uint32_t size = scratchCount;

    CcuLocalAddr dst;
    CCU_CHK_RET(ccu::Create(&dst));
    dst.addr  = outDstOrg.addr;
    dst.token = outDstOrg.token;

    CcuLocalAddr src;
    CCU_CHK_RET(ccu::Create(&src));
    src.addr  = srcOrg.addr;
    src.token = srcOrg.token;

    CcuLocalAddr scratch[RS_MAX_RANK_SIZE];
    for (uint32_t idx = 0; idx < size; idx++) {
        CCU_CHK_RET(ccu::Create(&scratch[idx]));
        scratch[idx].addr  = scratchOrg[idx].addr;
        scratch[idx].token = scratchOrg[idx].token;
    }

    CCU_CHK_RET(CreateReduceLoop(ctx));

    uint32_t expansionNum = GetReduceExpansionNum(arg->reduceOp, arg->dataType, arg->outputDataType);
    CcuVariable sliceSizeExpansion;
    CCU_CHK_RET(ccu::Create(&sliceSizeExpansion));

    if (expansionNum != 1) {
        CcuVariable tmp;
        CCU_CHK_RET(ccu::Create(&tmp));
        tmp = GetExpansionParam(expansionNum);
        dst.token = dst.token + tmp;
    }

    // m 部分
    CCU_IF_ONLY(goSize.loopParam != 0) {
        CcuVariable loopParam;
        CCU_CHK_RET(ccu::Create(&loopParam));
        loopParam = GetLoopParam(0, ctx.moConfig.memSlice * ctx.moConfig.loopCount, 0);
        loopParam = loopParam + goSize.loopParam;

        CcuVariable sliceSize;
        CCU_CHK_RET(ccu::Create(&sliceSize));
        sliceSize          = ctx.moConfig.memSlice;
        sliceSizeExpansion = ctx.moConfig.memSlice * expansionNum;

        // 绑定 loop0 的外部 LocalAddr 和 Variable
        ctx.loopDst[0].addr  = dst.addr;
        ctx.loopDst[0].token = dst.token;
        ctx.loopSrc[0].addr  = src.addr;
        ctx.loopSrc[0].token = src.token;
        for (uint32_t i = 0; i < size; i++) {
            ctx.loopScratch[0][i].addr  = scratch[i].addr;
            ctx.loopScratch[0][i].token = scratch[i].token;
        }
        ctx.loopLen[0]    = sliceSize;
        ctx.loopLenExp[0] = sliceSizeExpansion;

        CcuVariable paraCfg;
        CCU_CHK_RET(ccu::Create(&paraCfg));
        paraCfg = GetParallelParam(ctx.moConfig.loopCount - 1, 0, 1);

        CcuVariable offsetCfg;
        CCU_CHK_RET(ccu::Create(&offsetCfg));
        offsetCfg = GetOffsetParam(ctx.moConfig.memSlice, ctx.moConfig.msInterleave, 1);

        CcuLoopGroupHandle group;
        CCU_CHK_RET(CcuLoopGroupCreateFromVar(&group, &paraCfg, &offsetCfg));
        CCU_CHK_RET(CcuLoopGroupAddLoopFromVar(group, ctx.reduceLoops[0], &loopParam, false));
    }

    // n+p 部分
    CCU_IF_ONLY(goSize.parallelParam != 0) {
        for (uint32_t i = 0; i < size; i++) {
            scratch[i].addr += goSize.addrOffset;
        }
        src.addr += goSize.addrOffset;
        for (uint32_t i = 0; i < expansionNum; i++) {
            dst.addr += goSize.addrOffset;
        }

        sliceSizeExpansion = 0;
        for (uint32_t i = 0; i < expansionNum; i++) {
            sliceSizeExpansion = sliceSizeExpansion + goSize.residual;
        }

        // 绑定 loop0 参数 (p 部分)
        ctx.loopDst[0].addr  = dst.addr;
        ctx.loopDst[0].token = dst.token;
        ctx.loopSrc[0].addr  = src.addr;
        ctx.loopSrc[0].token = src.token;
        for (uint32_t i = 0; i < size; i++) {
            ctx.loopScratch[0][i].addr  = scratch[i].addr;
            ctx.loopScratch[0][i].token = scratch[i].token;
        }
        ctx.loopLen[0]    = goSize.residual;
        ctx.loopLenExp[0] = sliceSizeExpansion;

        // n 部分偏移
        for (uint32_t i = 0; i < size; i++) {
            scratch[i].addr += goSize.residual;
        }
        src.addr += goSize.residual;
        for (uint32_t i = 0; i < expansionNum; i++) {
            dst.addr += goSize.residual;
        }

        CcuVariable sliceSize;
        CCU_CHK_RET(ccu::Create(&sliceSize));
        sliceSize          = ctx.moConfig.memSlice;
        sliceSizeExpansion = ctx.moConfig.memSlice * expansionNum;

        // 绑定 loop1 参数 (n 部分)
        ctx.loopDst[1].addr  = dst.addr;
        ctx.loopDst[1].token = dst.token;
        ctx.loopSrc[1].addr  = src.addr;
        ctx.loopSrc[1].token = src.token;
        for (uint32_t i = 0; i < size; i++) {
            ctx.loopScratch[1][i].addr  = scratch[i].addr;
            ctx.loopScratch[1][i].token = scratch[i].token;
        }
        ctx.loopLen[1]    = sliceSize;
        ctx.loopLenExp[1] = sliceSizeExpansion;

        CcuVariable loopCfg0;
        CCU_CHK_RET(ccu::Create(&loopCfg0));
        loopCfg0 = GetLoopParam(0, 0, 1);

        CcuVariable loopCfg1;
        CCU_CHK_RET(ccu::Create(&loopCfg1));
        loopCfg1 = GetLoopParam(0, 0, 1);

        CcuVariable offsetCfg;
        CCU_CHK_RET(ccu::Create(&offsetCfg));
        offsetCfg = GetOffsetParam(ctx.moConfig.memSlice, ctx.moConfig.msInterleave, 1);

        CcuLoopGroupHandle group;
        CCU_CHK_RET(CcuLoopGroupCreateFromVar(&group, &goSize.parallelParam, &offsetCfg));
        CCU_CHK_RET(CcuLoopGroupAddLoopFromVar(group, ctx.reduceLoops[0], &loopCfg0, false));
        CCU_CHK_RET(CcuLoopGroupAddLoopFromVar(group, ctx.reduceLoops[1], &loopCfg1, false));
    }

    return CCU_SUCCESS;
}

// ============================================================================
// DoReduceScatter
// ============================================================================

static CcuResult DoReduceScatter(ReduceScatterContext &ctx)
{
    const auto *arg = ctx.arg;
    uint32_t channelId = 0;

    CcuLocalAddr myOutput;
    CCU_CHK_RET(ccu::Create(&myOutput));
    myOutput.addr  = ctx.output;
    myOutput.addr += ctx.currentRankSliceOutputOffset;
    myOutput.token = ctx.token[arg->rankId];

    CcuVariable sliceSize;
    CCU_CHK_RET(ccu::Create(&sliceSize));
    sliceSize = (arg->rankId == (arg->rankSize - 1)) ? ctx.lastSliceSize : ctx.normalSliceSize;

    CCU_IF_ONLY(sliceSize != 0) {
        for (uint32_t rankIdx = 0; rankIdx < arg->rankSize; rankIdx++) {
            ctx.event.setMask(1 << rankIdx);
            if (rankIdx == arg->rankId) {
                ccu::Record(ctx.event);
            } else {
                ccu::Read(
                    arg->channels[channelId],
                    ctx.scratchMem[rankIdx],
                    ctx.remoteInput[rankIdx],
                    sliceSize, ctx.event);
                channelId++;
            }
        }

        ctx.event.setMask((1 << arg->rankSize) - 1);
        ccu::Wait(ctx.event);

        ReduceLoopGroup(ctx, myOutput, ctx.myInput,
            ctx.scratchMem, arg->rankSize, ctx.goSize);
    }

    return CCU_SUCCESS;
}

// ============================================================================
// DoRepeatReduceScatter
// ============================================================================

static CcuResult DoRepeatReduceScatter(ReduceScatterContext &ctx)
{
    const auto *arg = ctx.arg;

    CcuVariable scratchOffset;
    CCU_CHK_RET(ccu::Create(&scratchOffset));
    scratchOffset = 0;

    for (uint32_t rankIdx = 0; rankIdx < arg->rankSize; rankIdx++) {
        if (rankIdx == arg->rankId) {
            ctx.myInput.addr  = ctx.input[rankIdx];
            ctx.myInput.addr += ctx.currentRankSliceInputOffset;
            ctx.myInput.token = ctx.token[rankIdx];
        } else {
            ctx.remoteInput[rankIdx].addr  = ctx.input[rankIdx];
            ctx.remoteInput[rankIdx].addr += ctx.currentRankSliceInputOffset;
            ctx.remoteInput[rankIdx].token = ctx.token[rankIdx];
        }

        ctx.scratchMem[rankIdx].addr  = ctx.scratch[arg->rankId];
        ctx.scratchMem[rankIdx].addr += scratchOffset;
        scratchOffset = scratchOffset + ctx.normalSliceSize;
        ctx.scratchMem[rankIdx].token = ctx.token[arg->rankId];
    }

    CcuVariable repeatNumAdd;
    CCU_CHK_RET(ccu::Create(&repeatNumAdd));
    repeatNumAdd = 1;
    ctx.flag     = 0;

    CCU_DO_WHILE(ctx.repeatNum != UINT64_MAX) {
        ctx.repeatNum = ctx.repeatNum + repeatNumAdd;

        CCU_IF_ONLY(ctx.flag == 1) {
            for (uint64_t rankIdx = 0; rankIdx < arg->rankSize; rankIdx++) {
                if (rankIdx == arg->rankId) {
                    ctx.myInput.addr += ctx.inputRepeatStride;
                } else {
                    ctx.remoteInput[rankIdx].addr += ctx.inputRepeatStride;
                }
            }
            ctx.output = ctx.output + ctx.outputRepeatStride;
        }

        DoReduceScatter(ctx);
        ctx.flag = 1;
    }

    return CCU_SUCCESS;
}

// ============================================================================
// 主入口 Kernel 函数
// ============================================================================

CcuResult CcuReduceScatterMesh1dKernel(CcuKernelArg arg)
{
    auto *kernelArg = static_cast<ReduceScatterKernelArg *>(arg);

    ReduceScatterContext ctx;
    ctx.arg = kernelArg;
    ctx.selfBit = 0;
    ctx.allBit = 0;
    ctx.resourceAllocated = false;
    ctx.loopRegistered = false;
    ctx.moConfig.msInterleave = 0;
    ctx.moConfig.loopCount = 0;
    ctx.moConfig.memSlice = 0;
    ctx.moRes.eventCount = 0;
    ctx.moRes.bufCount = 0;

    CCU_CHK_RET(InitResource(ctx));
    CCU_CHK_RET(LoadArgs(ctx));

    PreSync(ctx);

    CCU_CHK_RET(DoRepeatReduceScatter(ctx));

    PostSync(ctx);

    return CCU_SUCCESS;
}

#endif // CCU_REDUCE_SCATTER_MESH1D_DEMO_H
