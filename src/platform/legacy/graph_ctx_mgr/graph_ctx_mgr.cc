/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "graph_ctx_mgr.h"
#include "legacy_log.h"
#include "legacy_common.h"
#include "ffts_ctx_provider.h"
 
using namespace std;

HcclResult InitFftsDescVectorReduce(HcclFftsContextsInfo *&fftsCtxsPtr, uint32_t streamId, int count, void *addrListDevMemPtr,
    void *funcAddr, uint32_t blockDim, bool useGraphConstructorV2)
{
    EnsureFftsContextsSize(fftsCtxsPtr);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    CHK_SAFETY_FUNC_RET(memset_s(&comCtx, sizeof(rtFftsPlusComCtx_t), 0, sizeof(rtFftsPlusComCtx_t)));

    rtFftsPlusAicAivCtx_t *ctx = reinterpret_cast<rtFftsPlusAicAivCtx_t *>(&comCtx);

    ConstructFftsAicAivCtx(count, addrListDevMemPtr, funcAddr, blockDim, ctx, true);

    if (useGraphConstructorV2) {
        CHK_RET(InitTaskAndUpdateDependencies(streamId, HcclFftsTaskType::REDUCE_TBE, fftsCtxsPtr));
    } else {
        CHK_RET(UpdataFftsDescRelationship(streamId, fftsCtxsPtr));
    }

    fftsCtxsPtr->refreshIndex++;
    fftsCtxsPtr->lastThreadIndex[streamId] = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult RefreshFftsDescVectorReduce(HcclFftsContextsInfo *&fftsCtxsPtr, uint32_t streamId, int count, void *addrListDevMemPtr,
    void *funcAddr, uint32_t blockDim, bool useGraphConstructorV2)
{
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    rtFftsPlusAicAivCtx_t *ctx = reinterpret_cast<rtFftsPlusAicAivCtx_t *>(&comCtx);

    if (ctx->contextType != RT_CTX_TYPE_AIV && ctx->contextType != RT_CTX_TYPE_LABEL) {
        HCCL_ERROR("ffts context type is invaild, expected:0x%x, actual:0x%x", RT_CTX_TYPE_AIV, ctx->contextType);
        return HCCL_E_PARA;
    }

    ConstructFftsAicAivCtx(count, addrListDevMemPtr, funcAddr, blockDim, ctx, false);
    fftsCtxsPtr->refreshIndex++;
    UpdateLastThreadIndex(streamId, fftsCtxsPtr, useGraphConstructorV2);
    return HCCL_SUCCESS;
}

HcclResult ConstructFftsAicAivCtx(int count, void *addrListDevMemPtr, void *funcAddr, uint32_t blockDim,
    rtFftsPlusAicAivCtx_t *ctx, bool isInit)
{
    if (isInit) {
        ctx->successorNum = 0;
        ctx->aten = 0;
        ctx->prefetchConfig = 0;
        ctx->predCntInit = 0;
        ctx->predCnt = 0;
        ctx->prefetchOnceBitmap = 0;
        ctx->res6 = 0;
        ctx->prefetchEnableBitmap = 0;
        ctx->threadId = 0;
        ctx->threadDim = 1;
        ctx->res12 = 0;
        ctx->res13 = 0;
    }
    ctx->contextType = (count == 0) ? RT_CTX_TYPE_LABEL : RT_CTX_TYPE_AIV;
    const u64 u64HighMask = 0xffffffff00000000;
    const u64 u64LowMask = 0x00000000ffffffff;
    const u32 shift = 32;
    ctx->nonTailBlockdim = blockDim;
    ctx->tailBlockdim = blockDim;
    ctx->taskParamPtrBaseL = reinterpret_cast<u64>(addrListDevMemPtr) & u64LowMask;
    ctx->taskParamPtrBaseH = (reinterpret_cast<u64>(addrListDevMemPtr) & u64HighMask) >> shift;
    ctx->nonTailTaskStartPcL = reinterpret_cast<u64>(funcAddr) & u64LowMask;
    ctx->nonTailTaskStartPcH = (reinterpret_cast<u64>(funcAddr) & u64HighMask) >> shift;
    ctx->tailTaskStartPcL = reinterpret_cast<u64>(funcAddr) & u64LowMask;
    ctx->tailTaskStartPcH = (reinterpret_cast<u64>(funcAddr) & u64HighMask) >> shift;

    return HCCL_SUCCESS;
}

HcclResult InitFftsDescRdmaSend(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, u32 dbindex, u64 dbinfo,
    bool useGraphConstructorV2, bool isCapture)
{
    EnsureFftsContextsSize(fftsCtxsPtr);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    CHK_SAFETY_FUNC_RET(memset_s(&comCtx, sizeof(rtFftsPlusComCtx_t), 0, sizeof(rtFftsPlusComCtx_t)));

    rtFftsPlusWriteValueCtx_t* ctx = reinterpret_cast<rtFftsPlusWriteValueCtx_t*>(&comCtx);

    CHK_RET(ConstructFftsWriteValueCtx(dbindex, dbinfo, ctx, isCapture));

    if (useGraphConstructorV2) {
        CHK_RET(InitTaskAndUpdateDependencies(streamId, HcclFftsTaskType::RDMA_SEND, fftsCtxsPtr));
    } else {
        CHK_RET(UpdataFftsDescRelationship(streamId, fftsCtxsPtr));
    }

    fftsCtxsPtr->refreshIndex++;
    fftsCtxsPtr->lastThreadIndex[streamId] = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult RefreshFftsDescRdmaSend(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, u32 dbindex, u64 dbinfo,
    bool useGraphConstructorV2, bool isCapture)
{
    const u64 u64HighMask = 0xffffffff00000000;
    const u64 u64LowMask = 0x00000000ffffffff;
    const u32 shift = 32;
    u64 dbAddr = 0;

    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    rtFftsPlusWriteValueCtx_t *ctx = reinterpret_cast<rtFftsPlusWriteValueCtx_t *>(&comCtx);

    if (ctx->contextType != RT_CTX_TYPE_WRITE_VALUE && ctx->contextType != RT_CTX_TYPE_LABEL &&
            ctx->contextType != RT_CTX_TYPE_WRITE_VALUE_RDMA) {
        HCCL_ERROR("ffts context type is invaild, expected:0x%x, actual:0x%x or 0x%x or 0x%x", ctx->contextType,
            RT_CTX_TYPE_WRITE_VALUE, RT_CTX_TYPE_LABEL, RT_CTX_TYPE_WRITE_VALUE_RDMA);
        return HCCL_E_PARA;
    };

    if (IsInvalidRdmaParam(dbindex, dbinfo)) {
        ctx->contextType = RT_CTX_TYPE_LABEL;
        ctx->writeAddressBaseL = 0;
        ctx->writeAddressBaseH = 0;
        SkipFftsRefresh(streamId, fftsCtxsPtr, useGraphConstructorV2);
        return HCCL_SUCCESS;
    }

    CHK_RET(GetRdmaDoorbellAddr(dbindex, dbAddr));
    if (ctx->contextType == RT_CTX_TYPE_LABEL) {
        ctx->contextType = RT_CTX_TYPE_WRITE_VALUE;
        ctx->writeAddressBaseL = dbAddr & u64LowMask;
        ctx->writeAddressBaseH = (dbAddr & u64HighMask) >> shift;
    } else {
        u64 dbAddrOrg = (static_cast<u64>(ctx->writeAddressBaseH) << 32) + ctx->writeAddressBaseL;
        if (dbAddrOrg != dbAddr) {
            HCCL_ERROR("ffts context dbAddr is invaild, expected:0x%x, actual:0x%x", dbAddrOrg, dbAddr);
            return HCCL_E_PARA;
        }
    }

    ctx->writeValue[0] = dbinfo & u64LowMask;                           // index 0: byte0~3
    ctx->writeValue[1] = (dbinfo & u64HighMask) >> shift;               // index 1: byte4~8
    ctx->writeValue[2] = 0;                                             // index 2: byte8~11
    ctx->writeValue[3] = 0;                                             // index 3: byte12~15

    if(isCapture) {
        u32 sqDepth = 0;
        CHK_RET(GetSqDepth(dbindex, sqDepth));

        ctx->contextType = RT_CTX_TYPE_WRITE_VALUE_RDMA;
        ctx->res12[0] = 0;
        ctx->res12[1] = sqDepth;
        ctx->res12[2] = 0;
        ctx->res12[3] = 0;
    }

    fftsCtxsPtr->refreshIndex++;
    UpdateLastThreadIndex(streamId, fftsCtxsPtr, useGraphConstructorV2);
    return HCCL_SUCCESS;
}

HcclResult ConstructFftsWriteValueCtx(u32 dbindex, u64 dbinfo, rtFftsPlusWriteValueCtx_t *ctx, bool isCapture)
{
    const u64 u64HighMask = 0xffffffff00000000;
    const u64 u64LowMask = 0x00000000ffffffff;
    const u32 shift = 32;
    u64 dbAddr = 0;

    if (!IsInvalidRdmaParam(dbindex, dbinfo)) {
        CHK_RET(GetRdmaDoorbellAddr(dbindex, dbAddr));
    }

    ctx->contextType =  IsInvalidRdmaParam(dbindex, dbinfo) ? RT_CTX_TYPE_LABEL : RT_CTX_TYPE_WRITE_VALUE;
    ctx->successorNum = 0;
    ctx->aten = 0;
    ctx->predCntInit = 0;
    ctx->predCnt = 0;
    ctx->atm = 0;
    ctx->threadId = 0;
    ctx->threadDim = 1;
    ctx->awSize = 3; // 3: write 8 Bytes
    ctx->awSnoop = 0;
    ctx->awCache = 0;
    ctx->awProt = 0;
    ctx->awVa = 0;  // 写物理地址（global 表单 write value 的va校验要关闭）
    ctx->res11 = 2; // 2: 标识rdma send
    ctx->writeAddressBaseL = dbAddr & u64LowMask;
    ctx->writeAddressBaseH = (dbAddr & u64HighMask) >> shift;
    ctx->writeAddressOffset = 0;
    ctx->writeValue[0] = dbinfo & u64LowMask;             // index 0: byte0~3
    ctx->writeValue[1] = (dbinfo & u64HighMask) >> shift; // index 1: byte4~8
    ctx->writeValue[2] = 0;                               // index 2: byte8~11
    ctx->writeValue[3] = 0;                               // index 3: byte12~15

    if(isCapture) {
        u32 sqDepth = 0;
        CHK_RET(GetSqDepth(dbindex, sqDepth));

        ctx->contextType = IsInvalidRdmaParam(dbindex, dbinfo) ? RT_CTX_TYPE_LABEL : RT_CTX_TYPE_WRITE_VALUE_RDMA;
        ctx->res12[0] = 0;
        ctx->res12[1] = sqDepth;
        ctx->res12[2] = 0;
        ctx->res12[3] = 0;
    }

    return HCCL_SUCCESS;
}

HcclResult GetSqDepth(u32 dbindex, u32& sqDepth)
{
    const u32 HCCL_SQ_DEPTH_SHIFT = 12;
    const u32 HCCL_SQ_DEPTH_MASK = 0x0000000f;
    const u32 HCCL_PI_MASK = 0xffff;
    sqDepth = (dbindex >> HCCL_SQ_DEPTH_SHIFT) & HCCL_SQ_DEPTH_MASK;
    sqDepth = 1 << sqDepth;
    sqDepth &= HCCL_PI_MASK;

    return HCCL_SUCCESS;
}

bool IsInvalidRdmaParam(u32 dbindex, u64 dbinfo)
{
    return dbindex == LEGACY_INVALID_UINT && dbinfo == LEGACY_INVALID_U64;
}

HcclResult InitFftsDescSdmaReduce(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    uint64_t cnt, const HcclDataType datatype, HcclReduceOp redOp, bool useGraphConstructorV2)
{
    FftsSdmaSqeHeader sdmaSqeHeader;
    CHK_RET(GetSdmaSqeHeader(datatype, redOp, sdmaSqeHeader));

    CHK_RET(InitFftsDescSdma(fftsCtxsPtr, streamId, dst, src, cnt * LEGACY_SIZE_TABLE[datatype],
        sdmaSqeHeader.word, useGraphConstructorV2));
    return HCCL_SUCCESS;
}

HcclResult RefreshFftsDescSdmaReduce(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    uint64_t cnt, const HcclDataType datatype, HcclReduceOp redOp, bool useGraphConstructorV2)
{
    FftsSdmaSqeHeader sdmaSqeHeader;
    CHK_RET(GetSdmaSqeHeader(datatype, redOp, sdmaSqeHeader));
    CHK_RET(RefreshFftsDescSdma(fftsCtxsPtr, streamId, dst, src, cnt * LEGACY_SIZE_TABLE[datatype],
        sdmaSqeHeader.word, useGraphConstructorV2));
    return HCCL_SUCCESS;
}

HcclResult GetSdmaSqeHeader(const HcclDataType &datatype, const HcclReduceOp &redOp,
    FftsSdmaSqeHeader &sdmaSqeHeader)
{
    sdmaSqeHeader.word = 0;
    sdmaSqeHeader.bit.dns = 1;
    sdmaSqeHeader.bit.sns = 1;
    sdmaSqeHeader.bit.dssv = 1;
    sdmaSqeHeader.bit.sssv = 1;

    const std::map<HcclDataType, u32> SDMA_DTYPE_TABLE = {
        {HCCL_DATA_TYPE_INT8, 0},       // int8
        {HCCL_DATA_TYPE_INT16, 1},      // int16
        {HCCL_DATA_TYPE_INT32, 2},      // int32
        {HCCL_DATA_TYPE_FP16, 6},       // fp16
        {HCCL_DATA_TYPE_FP32, 7},       // fp32
        {HCCL_DATA_TYPE_BFP16, 0x8},    // bfp16
    };

    const std::map<HcclReduceOp, u32> SDMA_OPCODE_TABLE = {
        {HCCL_REDUCE_SUM, 1},       // sum
        {HCCL_REDUCE_MAX, 2},       // max
        {HCCL_REDUCE_MIN, 3},       // min
    };
    // inline reduce不支持int64，走tbe reduce

    try {
        sdmaSqeHeader.bit.datatype = SDMA_DTYPE_TABLE.at(datatype);
        sdmaSqeHeader.bit.opcode = SDMA_OPCODE_TABLE.at(redOp);
    } catch (...) {
        HCCL_ERROR("[GraphCtxMgr][GetSdmaSqeHeader]data type[%s] or reduceOp[%s] is not support",
            LegacyGetDataTypeEnumStr(datatype).c_str(), LegacyGetReduceOpEnumStr(redOp).c_str());
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult InitFftsDescSdma(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    u64 cnt, u32 sdmaSqeHeader, bool useGraphConstructorV2)
{
    uint64_t spiltLoop = 0;
    uint64_t addrOffset = 0;
    uint64_t contSplit = 0;
    if (cnt > HCCL_DMA_MAX_COUNT_4GB) {
        spiltLoop = (cnt % HCCL_DMA_MAX_COUNT_4GB) ?
            (cnt / HCCL_DMA_MAX_COUNT_4GB) : (cnt / HCCL_DMA_MAX_COUNT_4GB - 1);
        HCCL_INFO("ffts+ InitFftsDescSdma SDMA task countSize is bigger than 4GB and do segmentation splitloop[%llu]",
            spiltLoop);
    }
    /* ffts+ SDMA任务拆分处理 */
    for (uint64_t index = 0 ; index <= spiltLoop; index++) {
        addrOffset = index * HCCL_DMA_MAX_COUNT_4GB;
        contSplit = (index == spiltLoop) ? (cnt - index * HCCL_DMA_MAX_COUNT_4GB) : (HCCL_DMA_MAX_COUNT_4GB);
        const void *srcSplit = (const void*)(static_cast<char *>(const_cast<void*>(src)) + addrOffset);
        void *dstSplit = static_cast<void *>(static_cast<char *>(dst) + addrOffset);

        EnsureFftsContextsSize(fftsCtxsPtr);
        rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
        CHK_SAFETY_FUNC_RET(memset_s(&comCtx, sizeof(rtFftsPlusComCtx_t), 0, sizeof(rtFftsPlusComCtx_t)));

        rtFftsPlusSdmaCtx_t* ctx = reinterpret_cast<rtFftsPlusSdmaCtx_t*>(&comCtx);

        ConstructFftsSdmaCtx(dstSplit, srcSplit, contSplit, sdmaSqeHeader, ctx, true);
        if (useGraphConstructorV2) {
            CHK_RET(InitTaskAndUpdateDependencies(streamId, HcclFftsTaskType::SDMA, fftsCtxsPtr));
        } else {
            CHK_RET(UpdataFftsDescRelationship(streamId, fftsCtxsPtr));
        }

        fftsCtxsPtr->refreshIndex++;
        fftsCtxsPtr->lastThreadIndex[streamId] = fftsCtxsPtr->refreshIndex;
    }
    return HCCL_SUCCESS;
}

HcclResult RefreshFftsDescSdma(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    u64 cnt, u32 sdmaSqeHeader, bool useGraphConstructorV2)
{
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    rtFftsPlusSdmaCtx_t* ctx = reinterpret_cast<rtFftsPlusSdmaCtx_t*>(&comCtx);

    if ((ctx->contextType != RT_CTX_TYPE_SDMA) && (ctx->contextType != RT_CTX_TYPE_LABEL)) {
        HCCL_ERROR("ffts context type is invalid, expected:0x%x, actual:0x%x", ctx->contextType, RT_CTX_TYPE_SDMA);
        return HCCL_E_PARA;
    }

    ConstructFftsSdmaCtx(dst, src, cnt, sdmaSqeHeader, ctx, false);

    fftsCtxsPtr->refreshIndex++;
    UpdateLastThreadIndex(streamId, fftsCtxsPtr, useGraphConstructorV2);
    return HCCL_SUCCESS;
}

HcclResult ConstructFftsSdmaCtx(void *dst, const void *src, u64 cnt, u32 sdmaSqeHeader,
    rtFftsPlusSdmaCtx_t *ctx, bool isInit)
{
    if (isInit) {
        ctx->successorNum = 0;
        ctx->aten = 0;
        ctx->predCntInit = 0;
        ctx->predCnt = 0;
        ctx->atm = 0;
        ctx->pmg = 0;
        ctx->ns = 0;
        ctx->partId = 0;
        ctx->qos = 0;
        ctx->threadId = 0;
        ctx->threadDim = 1;

        ctx->sourceStreamId = 0;
        ctx->sourceSubstreamId = 0;
        ctx->sourceAddressOffset = 0;
        ctx->destinationStreamId = 0;
        ctx->destinationSubstreamId = 0;
        ctx->destinationAddressOffset = 0;
    }

    ctx->contextType = (cnt == 0 || src == dst) ? RT_CTX_TYPE_LABEL : RT_CTX_TYPE_SDMA;
    ctx->res3 = 0x5A;  // 0x5A tell mcu that it is an HCCL label ctx.
    ctx->sdmaSqeHeader = sdmaSqeHeader;

    const u64 u64HighMask = 0xffffffff00000000;
    const u64 u64LowMask = 0x00000000ffffffff;
    const u32 shift = 32;

    ctx->sourceAddressBaseL = reinterpret_cast<u64>(src) & u64LowMask;
    ctx->sourceAddressBaseH = (reinterpret_cast<u64>(src) & u64HighMask) >> shift;

    ctx->destinationAddressBaseL = reinterpret_cast<u64>(dst) & u64LowMask;
    ctx->destinationAddressBaseH = (reinterpret_cast<u64>(dst) & u64HighMask) >> shift;

    ctx->nonTailDataLength = cnt;
    ctx->tailDataLength = cnt;
    return HCCL_SUCCESS;
}

HcclResult InitFftsDescMemcpy(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dstAddr, void *srcAddr,
    uint64_t size, bool useGraphConstructorV2)
{
    CHK_RET(InitFftsDescSdma(fftsCtxsPtr, streamId, dstAddr, srcAddr, size, SDMA_FP32_ATOMIC_MOVE_SQE,
        useGraphConstructorV2));
    return HCCL_SUCCESS;
}

HcclResult RefreshFftsDescMemcpy(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dstAddr, void *srcAddr,
    uint64_t size, bool useGraphConstructorV2)
{
    CHK_RET(RefreshFftsDescSdma(fftsCtxsPtr, streamId, dstAddr, srcAddr, size, SDMA_FP32_ATOMIC_MOVE_SQE,
        useGraphConstructorV2));
    return HCCL_SUCCESS;
}

HcclResult InitFftsSuccessorEnd(u32 notifyID, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr,
    bool useGraphConstructorV2)
{
    if (useGraphConstructorV2) {
        CHK_RET(InitTaskAndUpdateDependencies(streamId, HcclFftsTaskType::INCHIP_NOTIFY_WAIT, fftsCtxsPtr, notifyID));
    } else {
        if (fftsCtxsPtr->unassignedSuccessorEnd.find(streamId) == fftsCtxsPtr->unassignedSuccessorEnd.end()) {
            std::vector<u32> tmp;
            fftsCtxsPtr->unassignedSuccessorEnd.emplace(streamId, tmp);
        }
        fftsCtxsPtr->unassignedSuccessorEnd[streamId].push_back(notifyID);
    }
    return HCCL_SUCCESS;
}

HcclResult InitFftsDescNotifyWait(void *signal, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr,
    bool useGraphConstructorV2)
{
    EnsureFftsContextsSize(fftsCtxsPtr);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    CHK_SAFETY_FUNC_RET(memset_s(&comCtx, sizeof(rtFftsPlusComCtx_t), 0, sizeof(rtFftsPlusComCtx_t)));

    rtFftsPlusNotifyCtx_t* ctx = reinterpret_cast<rtFftsPlusNotifyCtx_t*>(&comCtx);

    ConstructFFtsNotifyWaitCtx(signal, ctx);
    if (useGraphConstructorV2) {
        CHK_RET(InitTaskAndUpdateDependencies(streamId, HcclFftsTaskType::REMOTE_NOTIFY_WAIT, fftsCtxsPtr));
    } else {
        CHK_RET(UpdataFftsDescRelationship(streamId, fftsCtxsPtr));
    }
    fftsCtxsPtr->refreshIndex++;
    fftsCtxsPtr->lastThreadIndex[streamId] = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult ConstructFftsNotifyCtx(u32 notifyID, uint16_t contextType, rtFftsPlusNotifyCtx_t* ctx)
{
    ctx->contextType = contextType;
    ctx->successorNum = 0;
    ctx->aten = 0;
    ctx->predCntInit = 0;
    ctx->predCnt = 0;
    ctx->satm = 0;
    ctx->atm = 0;
    ctx->threadId = 0;
    ctx->threadDim = 1;
    ctx->notifyIdBase = notifyID;
    ctx->autoWindow = 0;
    return HCCL_SUCCESS;
}

HcclResult ConstructFFtsNotifyWaitCtx(void *signal, rtFftsPlusNotifyCtx_t* ctx)
{
    u32 notifyID = 0;
    CHK_RET(GetNotifyID(signal, &notifyID));
    return ConstructFftsNotifyCtx(notifyID, RT_CTX_TYPE_NOTIFY_WAIT, ctx);
}

HcclResult InitFftsDescNotifyRecordRemote(void *signal, s32 streamId, u64 signalAddr,
    HcclFftsContextsInfo *&fftsCtxsPtr, bool useGraphConstructorV2)
{
    EnsureFftsContextsSize(fftsCtxsPtr);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    CHK_SAFETY_FUNC_RET(memset_s(&comCtx, sizeof(rtFftsPlusComCtx_t), 0, sizeof(rtFftsPlusComCtx_t)));

    rtFftsPlusWriteValueCtx_t* ctx = reinterpret_cast<rtFftsPlusWriteValueCtx_t*>(&comCtx);

    CHK_RET(ConstructFftsNotifyRecordRemoteCtx(signal, ctx, signalAddr));

    if (useGraphConstructorV2) {
        CHK_RET(InitTaskAndUpdateDependencies(streamId, HcclFftsTaskType::REMOTE_NOTIFY_RECORD, fftsCtxsPtr));
    } else {
        CHK_RET(UpdataFftsDescRelationship(streamId, fftsCtxsPtr));
    }

    fftsCtxsPtr->refreshIndex++;
    fftsCtxsPtr->lastThreadIndex[streamId] = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult UpdataFftsDescRelationship(s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (fftsCtxsPtr->refreshIndex > 0) {
        u32 lastThreadPreIndex = fftsCtxsPtr->lastThreadIndex[streamId];
        if (lastThreadPreIndex > 0) {
            fftsCtxsPtr->contexts[lastThreadPreIndex - 1]
                .successorList[fftsCtxsPtr->contexts[lastThreadPreIndex - 1].successorNum] = fftsCtxsPtr->refreshIndex;
            fftsCtxsPtr->contexts[lastThreadPreIndex - 1].successorNum++;
            fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCntInit++;
            fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCnt++;
        }
    }

    std::vector<u32> &notifyVec = fftsCtxsPtr->unassignedSuccessorEnd[streamId];
    for (std::vector<u32>::iterator iterNotify = notifyVec.begin(); iterNotify != notifyVec.end();) {
        std::vector<u32>::iterator iter = std::find(fftsCtxsPtr->ignoredSuccessorStart.begin(),
            fftsCtxsPtr->ignoredSuccessorStart.end(), *iterNotify);
        if (iter != fftsCtxsPtr->ignoredSuccessorStart.end()) {
            // signal record位于stream上的首个task，该signal无需构建successor连接。
            fftsCtxsPtr->ignoredSuccessorStart.erase(iter);
            iterNotify = notifyVec.erase(iterNotify);
        } else {
            if (fftsCtxsPtr->unrelatedSuccessorStart.find(*iterNotify) != fftsCtxsPtr->unrelatedSuccessorStart.end()) {
                u32 preIndex = fftsCtxsPtr->unrelatedSuccessorStart[*iterNotify];
                fftsCtxsPtr->contexts[preIndex].successorList[fftsCtxsPtr->contexts[preIndex].successorNum] =
                    fftsCtxsPtr->refreshIndex;
                fftsCtxsPtr->contexts[preIndex].successorNum++;
                fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCntInit++;
                fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCnt++;

                fftsCtxsPtr->unrelatedSuccessorStart.erase(*iterNotify);
                iterNotify = notifyVec.erase(iterNotify);
            } else {
                fftsCtxsPtr->unrelatedSuccessorEnd[*iterNotify] = fftsCtxsPtr->refreshIndex;
                iterNotify = notifyVec.erase(iterNotify);
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ConstructFftsNotifyRecordRemoteCtx(void *signal, rtFftsPlusWriteValueCtx_t *ctx, u64 signalAddr)
{
    const u64 u64HighMask = 0xffffffff00000000;
    const u64 u64LowMask = 0x00000000ffffffff;
    const u32 shift = 32;
    u64 notifyAddr = signalAddr;
    if (notifyAddr == LEGACY_INVALID_U64) {
        CHK_RET(NotifyGetAddr(signal, &notifyAddr));
    }
    ctx->contextType = RT_CTX_TYPE_WRITE_VALUE;
    ctx->successorNum = 0;
    ctx->aten = 0;
    ctx->predCntInit = 0;
    ctx->predCnt = 0;
    ctx->atm = 0;
    ctx->threadId = 0;
    ctx->threadDim = 1;
    ctx->awSize = 2; // 2: write 4 bytes
    ctx->awSnoop = 0;
    ctx->awCache = 0;
    ctx->awProt = 0;
    ctx->awVa = 0;  // 写物理地址（global 表单 write value 的va校验要关闭）
    ctx->res11 = 4; // 4: 标识notify record
    ctx->writeAddressBaseL = notifyAddr & u64LowMask;
    ctx->writeAddressBaseH = (notifyAddr & u64HighMask) >> shift;
    ctx->writeAddressOffset = 0;
    ctx->writeValue[0] = 1; // index 1: byte0~3
    ctx->writeValue[1] = 0; // index 1: byte4~7
    ctx->writeValue[2] = 0; // index 2: byte8~11
    ctx->writeValue[3] = 0; // index 3: byte12~15
    return HCCL_SUCCESS;
}

void EnsureFftsContextsSize(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (fftsCtxsPtr->refreshIndex >= fftsCtxsPtr->contexts.size()) {
        fftsCtxsPtr->contexts.resize(fftsCtxsPtr->contexts.size() * 2);     // context空间不足，申请当前context num的2倍
    }
    return;
}

HcclResult InitFftsSuccessorStart(u32 notifyID, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr,
    bool useGraphConstructorV2)
{
    if (useGraphConstructorV2) {
        // 片内signal record前没有ctx, 需要插入一个空task作为附着点
        if (fftsCtxsPtr->lastThreadIndex[streamId] == 0) {
            CHK_RET(InitFftsPlaceHolder(streamId, fftsCtxsPtr));
        }
        CHK_RET(InitTaskAndUpdateDependencies(streamId, HcclFftsTaskType::INCHIP_NOTIFY_RECORD,
            fftsCtxsPtr, notifyID));
    } else {
        u32 lastThreadPreIndex = fftsCtxsPtr->lastThreadIndex[streamId];
        if (lastThreadPreIndex == 0) {
            // 该stream首个task为signal record时，则忽略该signal
            fftsCtxsPtr->ignoredSuccessorStart.push_back(notifyID);
        } else {
            if (fftsCtxsPtr->unrelatedSuccessorEnd.find(notifyID) != fftsCtxsPtr->unrelatedSuccessorEnd.end()) {
                u32 successorIndex = fftsCtxsPtr->unrelatedSuccessorEnd[notifyID];
                fftsCtxsPtr->contexts[lastThreadPreIndex - 1]
                    .successorList[fftsCtxsPtr->contexts[lastThreadPreIndex - 1].successorNum] = successorIndex;
                fftsCtxsPtr->contexts[lastThreadPreIndex - 1].successorNum++;
                fftsCtxsPtr->contexts[successorIndex].predCntInit++;
                fftsCtxsPtr->contexts[successorIndex].predCnt++;
    
                fftsCtxsPtr->unrelatedSuccessorEnd.erase(notifyID);
            } else {
                fftsCtxsPtr->unrelatedSuccessorStart[notifyID] = fftsCtxsPtr->lastThreadIndex[streamId] - 1;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult AddCtxExpasionSuccessor(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    for (auto& elem : fftsCtxsPtr->ctxExpasionSuccessorMap) {
        u32 ctxId = elem.first;
        fftsCtxsPtr->contexts[ctxId].successorNum = 0;
        for (auto& successor : elem.second) {
            // 第26个位置放用来扩充边的ctxId
            if (fftsCtxsPtr->contexts[ctxId].successorNum == RT_CTX_SUCCESSOR_NUM - 1) {
                CHK_RET(AddExpasionEdgeCtx(ctxId, fftsCtxsPtr));
                ctxId = fftsCtxsPtr->refreshIndex - 1;
                fftsCtxsPtr->contexts[ctxId].successorNum = 0;
            } 
            fftsCtxsPtr->contexts[ctxId].successorList[fftsCtxsPtr->contexts[ctxId].successorNum] = successor;
            fftsCtxsPtr->contexts[ctxId].successorNum++;
            fftsCtxsPtr->contexts[successor].predCnt++;
            fftsCtxsPtr->contexts[successor].predCntInit++;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult AddExpasionEdgeCtx(u32 preCtxId, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    PLF_CONFIG_INFO(PLF_TASK, "AddExpasionEdgeCtx para: preCtxId[%u], replaceCtxId[%u]", preCtxId, fftsCtxsPtr->refreshIndex);
    EnsureFftsContextsSize(fftsCtxsPtr);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    CHK_SAFETY_FUNC_RET(memset_s(&comCtx, sizeof(rtFftsPlusComCtx_t), 0, sizeof(rtFftsPlusComCtx_t)));
 
    rtFftsPlusWriteValueCtx_t* ctx = reinterpret_cast<rtFftsPlusWriteValueCtx_t*>(&comCtx);
    ctx->contextType = RT_CTX_TYPE_LABEL;
    ctx->threadDim = 1;
    // 更新依赖关系
    fftsCtxsPtr->contexts[preCtxId]
        .successorList[fftsCtxsPtr->contexts[preCtxId].successorNum] = fftsCtxsPtr->refreshIndex;
    fftsCtxsPtr->contexts[preCtxId].successorNum++;
    fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCnt++;
    fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCntInit++;

    // 刷新用来扩边的ctx数量
    fftsCtxsPtr->expasionSuccessorCtxNum++;
    fftsCtxsPtr->refreshIndex++;
    return HCCL_SUCCESS;
}

HcclResult BuildFFTSGraph(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    CHK_RET(FindInDegree0Task(fftsCtxsPtr));
    fftsCtxsPtr->isVisitedTasks.resize(fftsCtxsPtr->refreshTaskIndex, std::vector<bool>(fftsCtxsPtr->refreshTaskIndex, false));
    while (!fftsCtxsPtr->inDegree0TaskQueue.empty()) {
        HcclFfftsTaskInfo &curTask = fftsCtxsPtr->inDegree0TaskQueue.front();
        for (u32 i = 0; i < curTask.successorNum; i++) {
            auto& afterTaskIndex = curTask.successorList[i];
            if (afterTaskIndex < fftsCtxsPtr->refreshTaskIndex) {
                HcclFfftsTaskInfo &nextTask = fftsCtxsPtr->taskInfos[afterTaskIndex];
                CHK_RET(TraverseEdges(curTask.taskIndex, nextTask.taskIndex, fftsCtxsPtr));
            } else {
                HCCL_ERROR("[TraverseEdges] nextTaskIndex[%u] successorTaskIndex[%u] is bigger than taskNum[%u]",
                    curTask.taskIndex, afterTaskIndex, fftsCtxsPtr->refreshTaskIndex);
                return HCCL_E_PARA;
            }
        }
        fftsCtxsPtr->inDegree0TaskQueue.pop();
    }
    return HCCL_SUCCESS;
}

HcclResult AddDependencyEdge(u32 preCtxId, u32 afterCtxId, u32 preTaskIndex,
    u32 afterTaskIndex, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (preCtxId == afterCtxId) {
        return HCCL_SUCCESS;
    }
    if (preCtxId >= fftsCtxsPtr->refreshIndex || afterCtxId >= fftsCtxsPtr->refreshIndex) {
        HCCL_ERROR("[GraphCtxMgr][AddDependencyEdge] preCtxId[%u] or afterCtxId[%u] is bigger than ctxNum[%u]",
             preCtxId, afterCtxId, fftsCtxsPtr->refreshIndex);
         return HCCL_E_PARA;
    }
    rtFftsPlusComCtx_t& preCtx = fftsCtxsPtr->contexts[preCtxId];
    // preCtxId 的successorList中找下有没有 afterCtxId这个successor, 有的话跳过
    for (u32 i = 0; i < preCtx.successorNum; i++) {
        if (afterCtxId == preCtx.successorList[i]) {
            return HCCL_SUCCESS;
        }
    }
    if (preCtx.successorNum >= RT_CTX_SUCCESSOR_NUM) {
        if (fftsCtxsPtr->ctxExpasionSuccessorMap.find(preCtxId) == fftsCtxsPtr->ctxExpasionSuccessorMap.end()) {
            for (auto successor : preCtx.successorList) {
                fftsCtxsPtr->ctxExpasionSuccessorMap[preCtxId].insert(successor);
                // 先把successor的前序恢复, 后续扩充ctx的时候再更新
                fftsCtxsPtr->contexts[successor].predCntInit--;
                fftsCtxsPtr->contexts[successor].predCnt--;
            }
            fftsCtxsPtr->ctxExpasionSuccessorMap[preCtxId].insert(afterCtxId);
        } else {
            fftsCtxsPtr->ctxExpasionSuccessorMap[preCtxId].insert(afterCtxId);
        }
        HCCL_DEBUG("[AddDependencyEdge] preCtxId[%u] afterCtxId[%u] preTaskIndex[%u], afterTaskIndex[%u]",
            preCtxId, afterCtxId, preTaskIndex, afterTaskIndex);
        return HCCL_SUCCESS;
    }
    rtFftsPlusComCtx_t& afterCtx = fftsCtxsPtr->contexts[afterCtxId];
    // 没有的话更新record和waitAfter之间的边
    preCtx.successorList[preCtx.successorNum] = afterCtxId;
    preCtx.successorNum++;
    afterCtx.predCntInit++;
    afterCtx.predCnt++;
    // 插入排序: sucessorList中存的是ctxId, 按照ctxId升入排序, 多个ctx后续successorList相同时, 无论谁最后执行完, 保证后续执行的ctx是固定的
    // 片内和片间并发拷贝时候, 片内先开始性能较优, 片间先开始SDMA性能会劣化
    for (int i = 1; i < preCtx.successorNum; i++) {
        int key = preCtx.successorList[i];
        int j = i - 1;
        // 将大于key的值向后移动
        while (j >= 0 && preCtx.successorList[j] > key) {
            preCtx.successorList[j + 1] = preCtx.successorList[j];
            j = j - 1;
        }
        preCtx.successorList[j + 1] = key;
    }
    HCCL_DEBUG("[AddDependencyEdge] preCtxId[%u] afterCtxId[%u] preTaskIndex[%u], afterTaskIndex[%u].",
        preCtxId, afterCtxId, preTaskIndex, afterTaskIndex);
    return HCCL_SUCCESS;
}

HcclResult TraverseEdges(u32 curTaskIndex, u32 nextTaskIndex, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (curTaskIndex >= fftsCtxsPtr->refreshTaskIndex || nextTaskIndex >= fftsCtxsPtr->refreshTaskIndex) {
        HCCL_ERROR("[GraphCtxMgr][TraverseEdges] curTaskIndex[%u] or nextTaskIndex[%u] is bigger than taskNum[%u]",
            curTaskIndex, nextTaskIndex, fftsCtxsPtr->refreshTaskIndex);
        return HCCL_E_PARA;
    }
    std::queue<std::pair<u32, u32>> edgeCandidateQueue;
    edgeCandidateQueue.push(std::make_pair(curTaskIndex, nextTaskIndex));
    while (!edgeCandidateQueue.empty()) {
        std::pair<u32, u32> &front = edgeCandidateQueue.front();
        if (!fftsCtxsPtr->isVisitedTasks[front.first][front.second]) {
            fftsCtxsPtr->isVisitedTasks[front.first][front.second] = true;
            HcclFfftsTaskInfo& curTask =  fftsCtxsPtr->taskInfos[front.first];
            HcclFfftsTaskInfo& nextTask = fftsCtxsPtr->taskInfos[front.second];
            if (curTask.taskType == HcclFftsTaskType::INCHIP_NOTIFY_RECORD) {
                switch (nextTask.taskType) {
                    // curTask是record, nextTask是wait, 更新record和waitAfter之间的边; 继续遍历curTask和wait的后继
                    case HcclFftsTaskType::INCHIP_NOTIFY_WAIT:
                        CHK_RET(AddDependencyEdge(curTask.ctxId, nextTask.ctxId, curTask.taskIndex,
                            nextTask.taskIndex, fftsCtxsPtr));
                    // curTask是record, nextTask是record, 继续找curTask和nextTask的后继是否有边
                    case HcclFftsTaskType::INCHIP_NOTIFY_RECORD:
                        for (u32 i = 0; i < nextTask.successorNum; i++) {
                            auto &afterTaskIndex = nextTask.successorList[i];
                            if (afterTaskIndex < fftsCtxsPtr->refreshTaskIndex) {
                                edgeCandidateQueue.push(std::make_pair(curTask.taskIndex, afterTaskIndex));
                            } else {
                                HCCL_ERROR("[TraverseEdges]  nextTaskIndex[%u] successorTaskIndex[%u] is bigger than taskNum[%u]",
                                    nextTask.taskIndex, afterTaskIndex, fftsCtxsPtr->refreshTaskIndex);
                                return HCCL_E_PARA;
                            }
                        }
                    default:
                        break;
                }
            }
            // 遍历nextTask和nextTask的后继是否有边
            for (u32 i = 0; i < nextTask.successorNum; i++) {
                auto& afterTaskIndex = nextTask.successorList[i];
                if (afterTaskIndex < fftsCtxsPtr->refreshTaskIndex) {
                    edgeCandidateQueue.push(std::make_pair(nextTask.taskIndex, afterTaskIndex));
                } else {
                    HCCL_ERROR("[TraverseEdges] nextTaskIndex[%u] successorTaskIndex[%u] is bigger than taskNum[%u]",
                        nextTask.taskIndex, afterTaskIndex, fftsCtxsPtr->refreshTaskIndex);
                    return HCCL_E_PARA;
                }
            }
        }
        edgeCandidateQueue.pop();
    }
    return HCCL_SUCCESS;
}

HcclResult FindInDegree0Task(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    bool isExistInDegree0 = false;
    for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
        auto& curTask = fftsCtxsPtr->taskInfos[i];
        if (curTask.predCnt == 0) {
            isExistInDegree0 = true;
            fftsCtxsPtr->inDegree0TaskQueue.push(curTask);
        }
    }
    if (!isExistInDegree0) {
        HCCL_ERROR("[UpdateInchipNotifyCtxID] task orchestration is error, no exist in-degree 0 task");
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult PrintFFTSDebugDetails(rtFftsPlusSqe_t &fftsPlusSqe, rtFftsPlusTaskInfo_t &task,
    HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (CheckLogLevel(HCCL, HCCL_LOG_DEBUG) || (GetExterInputDebugConfigLegacy() & PLF_TASK)) {
        PLF_CONFIG_DEBUG(PLF_TASK, "-------------------------------");
        PLF_CONFIG_DEBUG(PLF_TASK, "totalContextNum:0x%04x", fftsPlusSqe.totalContextNum);
        PLF_CONFIG_DEBUG(PLF_TASK, "readyContextNum:0x%04x", fftsPlusSqe.readyContextNum);
        PLF_CONFIG_DEBUG(PLF_TASK, "preloadContextNum:0x%04x", fftsPlusSqe.preloadContextNum);
        PLF_CONFIG_DEBUG(PLF_TASK, "descBuf:%p", task.descBuf);
        PLF_CONFIG_DEBUG(PLF_TASK, "descBufLen:%u", task.descBufLen);
        PLF_CONFIG_DEBUG(PLF_TASK, "descAddrType:%u", task.descAddrType);
        const u32 printLineNum = 32;
        const u32 printBytePreLine = sizeof(rtFftsPlusComCtx_t) / printLineNum;
        const u32 byte3Offset = 3;
        const u32 byte2Offset = 2;
        const u32 byte1Offset = 1;
        const u32 byte0Offset = 0;
        for (u32 i = 0; i < fftsCtxsPtr->ctxNum; i++) {
            PLF_CONFIG_DEBUG(PLF_TASK, "-------------------------------");
            PLF_CONFIG_DEBUG(PLF_TASK, "index:0x%02x", i);
            for (u32 j = 0; j < printLineNum; j++) {
                u8 *prtPtr = reinterpret_cast<u8 *>(reinterpret_cast<u64>(task.descBuf) +
                    i * sizeof(rtFftsPlusComCtx_t) + j * printBytePreLine);
                PLF_CONFIG_DEBUG(PLF_TASK,
                    "%02x %02x %02x %02x", *(prtPtr + byte3Offset), *(prtPtr + byte2Offset),
                    *(prtPtr + byte1Offset), *(prtPtr + byte0Offset));
            }
        }
        PLF_CONFIG_DEBUG(PLF_TASK, "-------------------------------");
    }
    return HCCL_SUCCESS;
}

HcclResult PrintOriginalGraph(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (CheckLogLevel(HCCL, HCCL_LOG_DEBUG) || (GetExterInputDebugConfigLegacy() & PLF_TASK)) {
        PLF_CONFIG_DEBUG(PLF_TASK, "[PrintOriginalGraph] totalTaskNum:%08x", fftsCtxsPtr->refreshTaskIndex);
        for (u32 i = 0; i < fftsCtxsPtr->refreshTaskIndex; i++) {
            auto& taskInfo =  fftsCtxsPtr->taskInfos[i];
            PLF_CONFIG_DEBUG(PLF_TASK, "[PrintOriginalGraph] %08x %08x %08x %08x %08x %08x %08x %08x %08x",  taskInfo.taskIndex, taskInfo.taskType,
                taskInfo.ctxId, taskInfo.streamId, taskInfo.notifyId, taskInfo.predCnt, taskInfo.successorNum, taskInfo.successorList[0],
                taskInfo.successorList[1]);
        }
        PLF_CONFIG_DEBUG(PLF_TASK, "[PrintOriginalGraph] totalCtxNum:%08x", fftsCtxsPtr->refreshIndex);
        for (u32 i = 0; i < fftsCtxsPtr->refreshIndex; i++) {
            auto& ctxInfo = fftsCtxsPtr->contexts[i];
            PLF_CONFIG_DEBUG(PLF_TASK, "[PrintOriginalGraph] %04x %04x %04x %04x %04x",  i, ctxInfo.contextType, ctxInfo.successorNum,
                ctxInfo.successorList[24], ctxInfo.successorList[25]);
            const u32 index0 = 0;
            const u32 index1 = 1;
            const u32 index2 = 2;
            const u32 index3 = 3;
            const u32 index4 = 4;
            const u32 index5 = 5;
            const u32 index6 = 6;
            const u32 index7 = 7;
            for (auto j = 0; j < RT_CTX_SUCCESSOR_NUM / PRE_LINE_SUCCESSORNUM; j++) {
                u32 offset = j * PRE_LINE_SUCCESSORNUM;
                PLF_CONFIG_DEBUG(PLF_TASK, "[PrintOriginalGraph] successorList: %04x %04x %04x %04x %04x %04x %04x %04x",
                ctxInfo.successorList[index0 + offset], ctxInfo.successorList[index1 + offset], ctxInfo.successorList[index2 + offset],
                ctxInfo.successorList[index3 + offset], ctxInfo.successorList[index4 + offset], ctxInfo.successorList[index5 + offset],
                ctxInfo.successorList[index6 + offset], ctxInfo.successorList[index7 + offset]);
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ConstructFftsSqe(u32 timeout, rtFftsPlusSqe_t &fftsPlusSqe, HcclFftsContextsInfo *&fftsCtxsPtr,
    std::vector<void *> &argsHandleList)
{
    fftsPlusSqe.fftsType = RT_FFTS_PLUS_TYPE;
    fftsPlusSqe.totalContextNum = static_cast<uint16_t>(fftsCtxsPtr->ctxNum);
    fftsPlusSqe.readyContextNum = 1;
    fftsPlusSqe.preloadContextNum = (fftsPlusSqe.totalContextNum <= CONTEXT_MAX_NUM ?\
        fftsPlusSqe.totalContextNum : CONTEXT_MAX_NUM);
    fftsPlusSqe.timeout = timeout;
    // 标识通信task，优化调度性能 (RTS要求带AIV/AIC任务的是 0x5B, 否则是0x5A)
    fftsPlusSqe.subType = argsHandleList.empty() ? 0x5A : 0x5B;
    fftsPlusSqe.subType = 0x5A;
    return HCCL_SUCCESS;
}

HcclResult ConstructFftsTask(rtFftsPlusTaskInfo_t &task, rtFftsPlusSqe_t &fftsPlusSqe,
    HcclFftsContextsInfo *&fftsCtxsPtr, std::vector<void *> &argsHandleList)
{
    task.fftsPlusSqe = &fftsPlusSqe;
    task.descBuf = fftsCtxsPtr->contexts.data();
    task.descBufLen = sizeof(rtFftsPlusComCtx_t) * fftsCtxsPtr->ctxNum;
    task.descAddrType = 0;
    if (!argsHandleList.empty()) {
        task.argsHandleInfoNum = argsHandleList.size();
        task.argsHandleInfoPtr = argsHandleList.data();
        argsHandleList.clear();
    }
    return HCCL_SUCCESS;
}

HcclResult GroupTasksByStreamId(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    // 把task按照stream分开
    for (auto x : fftsCtxsPtr->lastTaskIndex) {
        auto streamID = x.first;
        std::vector<HcclFfftsTaskInfo> tmp;
        tmp.reserve(fftsCtxsPtr->refreshTaskIndex); // 预留内存, 减少内存分配次数
        fftsCtxsPtr->streamTaskMap[streamID] = tmp;
    }
    for (auto& taskInfo : fftsCtxsPtr->taskInfos) {
        fftsCtxsPtr->streamTaskMap[taskInfo.streamId].emplace_back(taskInfo);
    }
    return HCCL_SUCCESS;
}

HcclResult UpdateInchipNotifyCtxID(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    // 更新片内notify的ctxID
    // 1. inchipNotifyRecord: 更新为前序的ctxID
    // 2. inchipNotifyWait: 更新为后续的ctxID, 如果没有后续的ctxID, 需要插入placeholder
    u32 oldTaskIndex = fftsCtxsPtr->refreshTaskIndex;
    for(auto& streamInfo : fftsCtxsPtr->streamTaskMap) {
        bool isPlaceHolderWait = false;
        for (u32 i = 0; i < streamInfo.second.size(); i++) {
            auto& curTask = streamInfo.second[i];
            if (curTask.taskType == HcclFftsTaskType::INCHIP_NOTIFY_RECORD) {
                CHK_RET(FindInchipRecordPreCtx(streamInfo.second, i, fftsCtxsPtr));
            } else if (curTask.taskType == HcclFftsTaskType::INCHIP_NOTIFY_WAIT) {
                CHK_RET(FindInchipWaitAfterCtx(streamInfo.second, i, isPlaceHolderWait, fftsCtxsPtr));
            }
        }
    }
    // 最差情况, 只有streamSize条流片内的wait后面没有ctx, 需要的streamSize个placeHolder; placeHolder个数不可能超过streamSize
    if (fftsCtxsPtr->inchipwaitPlaceHolderNum > fftsCtxsPtr->streamTaskMap.size()) {
        HCCL_ERROR("inchip wait after place holder is bigger than stream size, inchipwaitPlaceHolderNum:%u, streamSize:%u",
            fftsCtxsPtr->inchipwaitPlaceHolderNum, fftsCtxsPtr->streamTaskMap.size());
        return HCCL_E_PTR;
    }
    // 把插入的placeholder放到每条流的对应位置
    for (u32 i = 0; i < fftsCtxsPtr->inchipwaitPlaceHolderNum; i++) {
        auto& taskInfo = fftsCtxsPtr->taskInfos[oldTaskIndex + i];
        fftsCtxsPtr->streamTaskMap[taskInfo.streamId].push_back(taskInfo);
    }
    return HCCL_SUCCESS;
}

HcclResult FindInchipRecordPreCtx(const std::vector<HcclFfftsTaskInfo>& taskList, u32 index,
    HcclFftsContextsInfo *&fftsCtxsPtr)
{
    const HcclFfftsTaskInfo& curTask = taskList[index];
    // 找inchip notifyRecord的前序ctxId
    if (index == 0) {
        HCCL_ERROR("[UpdateInchipNotifyCtxID] NotifyRecord not exist pre task, curStream[%d] curTaskIndex[%u] ctxId[%d]",
            curTask.streamId, curTask.taskIndex, curTask.ctxId);
        return HCCL_E_INTERNAL;
    }
    u32 preIndex = index - 1;
    bool isExistPreCtx = false;
    while (preIndex < taskList.size()) {
        if (taskList[preIndex].taskType != HcclFftsTaskType::INCHIP_NOTIFY_RECORD &&
            taskList[preIndex].taskType != HcclFftsTaskType::INCHIP_NOTIFY_WAIT) {
            isExistPreCtx = true;
            break;
        }
        if (preIndex == 0) {
            break;
        }
        preIndex--;
    }
    if (!isExistPreCtx) {
        HCCL_ERROR("[UpdateInchipNotifyCtxID] NotifyRecord not exist pre ctx, curStream[%d] curTaskIndex[%u] ctxId[%d]",
            curTask.streamId, curTask.taskIndex, curTask.ctxId);
        return HCCL_E_INTERNAL;
    }
    fftsCtxsPtr->taskInfos[curTask.taskIndex].ctxId = taskList[preIndex].ctxId;
    HCCL_DEBUG("[UpdateInchipNotifyCtxID] NotifyRecord: curStream[%d] curTaskIndex[%u] ctxId[%u], preTaskIndex[%u]",
        curTask.streamId, curTask.taskIndex, fftsCtxsPtr->taskInfos[curTask.taskIndex].ctxId, taskList[preIndex].taskIndex);
    return HCCL_SUCCESS;
}

HcclResult FindInchipWaitAfterCtx(const std::vector<HcclFfftsTaskInfo>& taskList, u32 index,
    bool& isPlaceHolderWait, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    const HcclFfftsTaskInfo& curTask = taskList[index];
    // 当前流之前已经在最后插入了一个ctx用来占位，后续所有的inchipWait的后继ctxID都是这个占位的ctx
    if (isPlaceHolderWait) {
        fftsCtxsPtr->taskInfos[curTask.taskIndex].ctxId = fftsCtxsPtr->lastThreadIndex[curTask.streamId] - 1;
        return HCCL_SUCCESS;
    }
    // 找inchip notifyWait的后继ctxID
    u32 aftIndex = index + 1;
    while (aftIndex < taskList.size()) {
        if (taskList[aftIndex].taskType != HcclFftsTaskType::INCHIP_NOTIFY_RECORD &&
            taskList[aftIndex].taskType != HcclFftsTaskType::INCHIP_NOTIFY_WAIT) {
            break;
        }
        aftIndex++;
    }
    if (aftIndex >= taskList.size()) {
        CHK_RET(InitFftsPlaceHolder(curTask.streamId, fftsCtxsPtr));
        fftsCtxsPtr->taskInfos[curTask.taskIndex].ctxId = fftsCtxsPtr->refreshIndex - 1;
        fftsCtxsPtr->inchipwaitPlaceHolderNum++;
        isPlaceHolderWait = true;
        HCCL_DEBUG("[UpdateInchipNotifyCtxID] NotifyWait not exist after task, add place hloder. curStream[%d] curTaskIndex[%u] ctxId[%d] \
            inchipwaitPlaceHolderNum[%u], placeHolderCtxId[%u]", curTask.streamId, curTask.taskIndex, curTask.ctxId,
            fftsCtxsPtr->inchipwaitPlaceHolderNum, fftsCtxsPtr->taskInfos[curTask.taskIndex].ctxId);
    } else {
        fftsCtxsPtr->taskInfos[curTask.taskIndex].ctxId = taskList[aftIndex].ctxId;
        HCCL_DEBUG("[UpdateInchipNotifyCtxID] NotifyWait: curStream[%d] curTaskIndex[%u] ctxId[%d], afterTaskIndex[%u]",
            curTask.streamId, curTask.taskIndex, fftsCtxsPtr->taskInfos[curTask.taskIndex].ctxId, taskList[aftIndex].taskIndex);
    }
    return HCCL_SUCCESS;
}

HcclResult InitFftsPlaceHolder(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    PLF_CONFIG_INFO(PLF_TASK, "PlaceHolder para: streamId[%d], ctxId[%u], taskIndex[%u]", streamID, fftsCtxsPtr->refreshIndex,
        fftsCtxsPtr->refreshTaskIndex);
    EnsureFftsContextsSize(fftsCtxsPtr);
    rtFftsPlusComCtx_t &comCtx = fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex];
    CHK_SAFETY_FUNC_RET(memset_s(&comCtx, sizeof(rtFftsPlusComCtx_t), 0, sizeof(rtFftsPlusComCtx_t)));
 
    rtFftsPlusWriteValueCtx_t* ctx = reinterpret_cast<rtFftsPlusWriteValueCtx_t*>(&comCtx);
    ctx->contextType = RT_CTX_TYPE_LABEL;
    ctx->threadDim = 1;
    // PlaceHolder只有V2版本使用, 不需要额外判断
    CHK_RET(InitTaskAndUpdateDependencies(streamID, HcclFftsTaskType::PLACE_HOLDER, fftsCtxsPtr));
    fftsCtxsPtr->refreshIndex++;
    fftsCtxsPtr->lastThreadIndex[streamID] = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult InitTaskAndUpdateDependencies(s32 streamID, HcclFftsTaskType taskType,
    HcclFftsContextsInfo *&fftsCtxsPtr, u32 notfiyId)
{
    // 更新当前流的上一个ctx和当前ctx的依赖关系
    // 片内的notifywait和record不实例化context, 不需要更新ctx依赖关系
    if (taskType != HcclFftsTaskType::INCHIP_NOTIFY_RECORD && taskType != HcclFftsTaskType::INCHIP_NOTIFY_WAIT) {
        if (fftsCtxsPtr->refreshIndex > 0) {
            // lastThreadPreIndex: 当前context的ctxId
            u32 lastThreadPreIndex = fftsCtxsPtr->lastThreadIndex[streamID];
            if (lastThreadPreIndex > 0) {
                fftsCtxsPtr->contexts[lastThreadPreIndex - 1]
                    .successorList[fftsCtxsPtr->contexts[lastThreadPreIndex - 1].successorNum] = fftsCtxsPtr->refreshIndex;
                fftsCtxsPtr->contexts[lastThreadPreIndex - 1].successorNum++;
                fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCntInit++;
                fftsCtxsPtr->contexts[fftsCtxsPtr->refreshIndex].predCnt++;
            }
        }
    }
    CHK_RET(InitFftsDescTaskInfo(streamID, taskType, fftsCtxsPtr, notfiyId));
    return HCCL_SUCCESS;
}

void EnsureFftsTaskInfosSize(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (fftsCtxsPtr->refreshTaskIndex >= fftsCtxsPtr->taskInfos.size()) {
        fftsCtxsPtr->taskInfos.resize(fftsCtxsPtr->taskInfos.size() * 2);     // taskInfos空间不足，申请当前taskInfos的2倍
    }
    return;
}

HcclResult InitFftsDescTaskInfo(s32 streamID, HcclFftsTaskType taskType, 
    HcclFftsContextsInfo *&fftsCtxsPtr, u32 notfiyId)
{
    EnsureFftsTaskInfosSize(fftsCtxsPtr);
    HcclFfftsTaskInfo &taskInfo = fftsCtxsPtr->taskInfos[fftsCtxsPtr->refreshTaskIndex];
    taskInfo.taskIndex = fftsCtxsPtr->refreshTaskIndex;
    taskInfo.ctxId = fftsCtxsPtr->refreshIndex;
    taskInfo.streamId = streamID;
    taskInfo.taskType = taskType;
    taskInfo.notifyId = notfiyId;
    CHK_RET(GraphCtxMgrTaskRelationship(streamID, fftsCtxsPtr));
    fftsCtxsPtr->refreshTaskIndex++;
    fftsCtxsPtr->lastTaskIndex[streamID] = fftsCtxsPtr->refreshTaskIndex;
    return HCCL_SUCCESS;
}

HcclResult GraphCtxMgrTaskRelationship(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    if (fftsCtxsPtr->refreshTaskIndex > 0) {
        u32 lastTaskPreIndex = fftsCtxsPtr->lastTaskIndex[streamID];
        if (lastTaskPreIndex > 0) {
            fftsCtxsPtr->taskInfos[lastTaskPreIndex - 1]
                .successorList[fftsCtxsPtr->taskInfos[lastTaskPreIndex - 1].successorNum] = fftsCtxsPtr->refreshTaskIndex;
            fftsCtxsPtr->taskInfos[lastTaskPreIndex - 1].successorNum++;
            fftsCtxsPtr->taskInfos[fftsCtxsPtr->refreshTaskIndex].predCnt++;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult BuildOriginalGraph(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    // 逐个流扫描, 把原始task 流间的依赖序画出来
    for(auto& streamInfo : fftsCtxsPtr->streamTaskMap) {
        auto curStreamId = streamInfo.first;
        for (auto& curTask : streamInfo.second) {
            bool isRecordFound = false;
            if (curTask.taskType == HcclFftsTaskType::INCHIP_NOTIFY_WAIT) {
                CHK_RET(AssociateWaitToRecord(curTask, curStreamId, isRecordFound, fftsCtxsPtr));
                if (!isRecordFound) {
                    HCCL_ERROR("[BuildOriginalGraph] recordInfo: curStreamId[%d] curTaskIndex[%u] notifyId[%d] not find correspond wait",
                        curStreamId, curTask.taskIndex, curTask.notifyId);
                    return HCCL_E_INTERNAL;
                }
            }            
        }
    }
    fftsCtxsPtr->streamTaskMap.clear();
    PrintOriginalGraph(fftsCtxsPtr);
    return HCCL_SUCCESS;
}

HcclResult  AssociateWaitToRecord(const HcclFfftsTaskInfo& curTask, s32 curStreamId,
    bool& isRecordFound, HcclFftsContextsInfo *&fftsCtxsPtr)
{
    for (auto& otherStream : fftsCtxsPtr->streamTaskMap) {
        if (otherStream.first != curStreamId) {
            for (auto& otherTask : otherStream.second) {
                if (otherTask.taskType == HcclFftsTaskType::INCHIP_NOTIFY_RECORD && otherTask.notifyId == curTask.notifyId) {
                    otherTask.notifyId = LEGACY_INVALID_UINT;
                    if (fftsCtxsPtr->taskInfos[otherTask.taskIndex].successorNum >= HCCL_TASK_SUCCESSOR_NUM) {
                        HCCL_ERROR("[AssociateWaitToRecord] task successor exceed [%d], curStreamId[%d] curTaskIndex[%u] otherStreamId[%d] otherTaskIndex[%u]",
                            HCCL_TASK_SUCCESSOR_NUM, curStreamId, curTask.taskIndex, otherStream.first, otherTask.taskIndex);
                        return HCCL_E_INTERNAL;
                    }
                    fftsCtxsPtr->taskInfos[otherTask.taskIndex].successorList[fftsCtxsPtr->taskInfos[otherTask.taskIndex].successorNum] = curTask.taskIndex;
                    fftsCtxsPtr->taskInfos[otherTask.taskIndex].successorNum++;
                    fftsCtxsPtr->taskInfos[curTask.taskIndex].predCnt++;
                    HCCL_DEBUG("[AssociateWaitToRecord] curStreamId[%d] curTaskIndex[%u] notifyId[%d] add successor otherStreamId[%d] otherTaskIndex[%u]",
                        curStreamId, curTask.taskIndex, curTask.notifyId, otherStream.first, otherTask.taskIndex);
                    isRecordFound = true;
                    break;
                }
            }
            if (isRecordFound) {
                break;
            }
        }
    }
    return HCCL_SUCCESS;
}