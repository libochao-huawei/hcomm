/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "graph_ctx_mgr_common.h"
#include "legacy_log.h"
#include "graph_ctx_mgr.h"
#include "ffts_ctx_provider.h"
#include "legacy_common.h"

using HcclFftsPubInfo = struct HcclFftsPubDef {
    u32 timeout{0};
    FftsCtxProvider fftsCtxProvider;
    bool useGraphConstructorV2{false};
    std::vector<void*> argsHandleList;
};

void *GraphMgrInit()
{
    return new (std::nothrow) HcclFftsPubInfo;
}

void GraphMgrDeInit(void *fftsPubInfo){
    if (fftsPubInfo == nullptr) {
        HCCL_ERROR("fftsPubInfo is nullpty");
        return;
    }
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    fftsPubInfoPtr->argsHandleList.clear();
    delete fftsPubInfoPtr;
    return;
}

void* GetGraphCtx(void *fftsPubInfo, const char *key, uint32_t keyLen)
{
    CHK_PRT_RET(fftsPubInfo == nullptr, HCCL_ERROR("fftsPubInfo is nullpty"), nullptr);
    CHK_PRT_RET(key == nullptr, HCCL_ERROR("GetGraphCtx key is nullpty"), nullptr);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    fftsPubInfoPtr->useGraphConstructorV2 = false;
    return fftsPubInfoPtr->fftsCtxProvider.GetFftsCtx(key, keyLen, false);
}

void* GetGraphCtxV2(void *fftsPubInfo, const char *key, uint32_t keyLen)
{
    CHK_PRT_RET(fftsPubInfo == nullptr, HCCL_ERROR("fftsPubInfo is nullpty"), nullptr);
    CHK_PRT_RET(key == nullptr, HCCL_ERROR("GetGraphCtx key is nullpty"), nullptr);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    fftsPubInfoPtr->useGraphConstructorV2 = true;
    return fftsPubInfoPtr->fftsCtxProvider.GetFftsCtx(key, keyLen, true);
}

HcclResult LaunchGraph(void *fftsPubInfo, void *streamPtr, void *ctx, uint32_t timeout, uint32_t *ctxNum)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(streamPtr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxNum);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    fftsPubInfoPtr->timeout = timeout;
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    if (fftsPubInfoPtr->useGraphConstructorV2) {
        // 首次构图的时候如果流上的inchipWait后面没有ctx, 会实例化一个新的placeholder用来占位
        // 构图完成, 后续重复执行这张图的时候需要跳过这些占位的ctx
        HCCL_DEBUG("refreshIndex:%u, inchipwaitPlaceHolderNum:%u, expasionSuccessorCtxNum:%u", fftsCtxsPtr->refreshIndex,
            fftsCtxsPtr->inchipwaitPlaceHolderNum, fftsCtxsPtr->expasionSuccessorCtxNum);
        if (fftsCtxsPtr->completed) {
            fftsCtxsPtr->refreshIndex += fftsCtxsPtr->inchipwaitPlaceHolderNum;
            fftsCtxsPtr->refreshIndex += fftsCtxsPtr->expasionSuccessorCtxNum;
        }
    }

    if (!fftsCtxsPtr->completed) {
        if (fftsPubInfoPtr->useGraphConstructorV2) {
            CHK_RET(GroupTasksByStreamId(fftsCtxsPtr));
            // 更新wait的后续ctxId的时候, 如果发现wait后面没有ctx, 会在wait后增加一个占位的ctx
            CHK_RET(UpdateInchipNotifyCtxID(fftsCtxsPtr));
            CHK_RET(BuildOriginalGraph(fftsCtxsPtr));
            CHK_RET(BuildFFTSGraph(fftsCtxsPtr));
            CHK_RET(AddCtxExpasionSuccessor(fftsCtxsPtr));
        }
        fftsCtxsPtr->ctxNum = fftsCtxsPtr->refreshIndex;
        fftsCtxsPtr->completed = true;
    }

    if (fftsPubInfoPtr->useGraphConstructorV2) {
        for (auto &streamLastIndex : fftsCtxsPtr->lastThreadIndex) {
            streamLastIndex.second = 0;
        }
    }

    if (fftsCtxsPtr->refreshIndex != fftsCtxsPtr->ctxNum) {
        HCCL_ERROR("ffts context num is invaild, expected:%u, actual:%u.", fftsCtxsPtr->ctxNum,
            fftsCtxsPtr->refreshIndex);
        return HCCL_E_PARA;
    }

    if (fftsCtxsPtr->ctxNum == 0) {
        *ctxNum = fftsCtxsPtr->ctxNum;
        HCCL_INFO("ffts context num is 0, will not submit this context.");
        return HCCL_SUCCESS;
    }

    if (fftsCtxsPtr->ctxNum > LEGACY_HCCL_FFTS_CAPACITY) {
        HCCL_ERROR("[GraphCtxMgr][LaunchTasksEx] CtxNum[%u] exceeds the limit of FFTS+ graph.", fftsCtxsPtr->ctxNum);
        return HCCL_E_NOT_SUPPORT;
    }

    fftsCtxsPtr->refreshIndex = 0;
    rtFftsPlusSqe_t fftsPlusSqe = {};
    ConstructFftsSqe(timeout, fftsPlusSqe, fftsCtxsPtr, fftsPubInfoPtr->argsHandleList);

    rtFftsPlusTaskInfo_t task = {};
    task.argsHandleInfoNum = 0;
    task.argsHandleInfoPtr = nullptr;
    ConstructFftsTask(task, fftsPlusSqe, fftsCtxsPtr, fftsPubInfoPtr->argsHandleList);
    PrintFFTSDebugDetails(fftsPlusSqe, task, fftsCtxsPtr);
    CHK_RET(FftsPlusTaskLaunch(&task, streamPtr));
    *ctxNum = fftsCtxsPtr->ctxNum;
    return HCCL_SUCCESS;
}

void GraphDump(void *fftsPubInfo, void *ctx)
{
    if (ctx == nullptr || fftsPubInfo == nullptr) {
        HCCL_ERROR("GraphDump ctx or fftsPubInfo is nullptr");
        return;
    }
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    rtFftsPlusSqe_t fftsPlusSqe = {};
    ConstructFftsSqe(fftsPubInfoPtr->timeout, fftsPlusSqe, fftsCtxsPtr, fftsPubInfoPtr->argsHandleList);

    rtFftsPlusTaskInfo_t task = {};
    task.argsHandleInfoNum = 0;
    task.argsHandleInfoPtr = nullptr;
    ConstructFftsTask(task, fftsPlusSqe, fftsCtxsPtr, fftsPubInfoPtr->argsHandleList);
    PrintFFTSDebugDetails(fftsPlusSqe, task, fftsCtxsPtr);
    if (fftsPubInfoPtr->useGraphConstructorV2) {
        PrintOriginalGraph(fftsCtxsPtr);
    }
    return;
}

HcclResult GraphAddRecordTaskInner(void *fftsPubInfo, void *ctx, uint32_t streamId, void *signal, bool inchip, u64 signalAddr, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(signal);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        if (inchip) {
            u32 notifyID = 0;
            CHK_RET(GetNotifyID(signal, &notifyID));
            CHK_RET(InitFftsSuccessorStart(notifyID, streamId, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2));
        } else {
            CHK_RET(InitFftsDescNotifyRecordRemote(signal, streamId, signalAddr, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2));
            *ctxIdx = fftsCtxsPtr->refreshIndex;
        }
    } else {
        if (!inchip) {
            SkipFftsRefresh(streamId, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2);
            *ctxIdx = fftsCtxsPtr->refreshIndex;
        } else {
            // V2的构图逻辑下, 这个片内的notify record如果是当前流第一个task, 会实例化一个空task作为附着点, 需要跳过
            if (fftsPubInfoPtr->useGraphConstructorV2 && fftsCtxsPtr->lastThreadIndex[streamId] == 0) {
                HCCL_DEBUG("cur stream first task is inchip notify record, need skip, ctxId[%u]", fftsCtxsPtr->refreshIndex);
                fftsCtxsPtr->refreshIndex++;
                fftsCtxsPtr->lastThreadIndex[streamId] = fftsCtxsPtr->refreshIndex;
            }
        }
    }
    PLF_CONFIG_INFO(PLF_TASK, "%s para: taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex - 1);
    return HCCL_SUCCESS;
}

HcclResult GraphAddRecordTaskWithSignalAddr(void *fftsPubInfo, void *ctx, uint32_t streamId,
    void *signal, bool inchip, u64 signalAddr, uint32_t *ctxIdx)
{
    CHK_RET(GraphAddRecordTaskInner(fftsPubInfo, ctx, streamId, signal, inchip, signalAddr, ctxIdx));
    return HCCL_SUCCESS;
}

HcclResult GraphAddRecordTask(void *fftsPubInfo, void *ctx, uint32_t streamId, void *signal, bool inchip, uint32_t *ctxIdx)
{
    CHK_RET(GraphAddRecordTaskInner(fftsPubInfo, ctx, streamId, signal, inchip, LEGACY_INVALID_U64, ctxIdx));
    return HCCL_SUCCESS;
}

HcclResult GraphAddWaitTask(void *fftsPubInfo, void *ctx, uint32_t streamId, void *signal, bool inchip, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(signal);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        if (inchip) {
            u32 notifyID = 0;
            CHK_RET(GetNotifyID(signal, &notifyID));
            CHK_RET(InitFftsSuccessorEnd(notifyID, streamId, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2));
        } else {
            CHK_RET(InitFftsDescNotifyWait(signal, streamId, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2));
            *ctxIdx = fftsCtxsPtr->refreshIndex;
        }
    } else {
        if (!inchip) {
            SkipFftsRefresh(streamId, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2);
            *ctxIdx = fftsCtxsPtr->refreshIndex;
        } else {
            // 卡内notify使用successor间依赖关系替代
        }
    }
    PLF_CONFIG_INFO(PLF_TASK, "%s para: taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex - 1);
    return HCCL_SUCCESS;
}

HcclResult GraphAddMemcpyTask(void *fftsPubInfo, void *ctx, uint32_t streamId, void *dstAddr, void *srcAddr,
    uint64_t size, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(dstAddr);
    CHK_PTR_NULL(srcAddr);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    PLF_CONFIG_INFO(PLF_TASK, "%s para:taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsDescMemcpy(fftsCtxsPtr, streamId, dstAddr, srcAddr, size,
            fftsPubInfoPtr->useGraphConstructorV2));
    } else {
        CHK_RET(RefreshFftsDescMemcpy(fftsCtxsPtr, streamId, dstAddr, srcAddr, size,
            fftsPubInfoPtr->useGraphConstructorV2));
    }
    *ctxIdx = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult GraphAddReduceTask(void *fftsPubInfo, void *ctx, uint32_t streamId, void *dstAddr, const void *srcAddr, uint64_t dataCount,
    const HcclDataType datatype, HcclReduceOp redOp, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(dstAddr);
    CHK_PTR_NULL(srcAddr);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    PLF_CONFIG_INFO(PLF_TASK, "%s para:taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsDescSdmaReduce(fftsCtxsPtr, streamId, dstAddr, srcAddr, dataCount, datatype,
            redOp, fftsPubInfoPtr->useGraphConstructorV2));
    } else {
        CHK_RET(RefreshFftsDescSdmaReduce(fftsCtxsPtr, streamId, dstAddr, srcAddr, dataCount, datatype,
            redOp, fftsPubInfoPtr->useGraphConstructorV2));
    }
    *ctxIdx = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult GraphAddInlineReduceTask(void *fftsPubInfo, void *ctx, uint32_t streamId, void *dstAddr, const void *srcAddr,
    uint64_t dataCount, const HcclDataType datatype, HcclReduceOp redOp, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(dstAddr);
    CHK_PTR_NULL(srcAddr);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
     PLF_CONFIG_INFO(PLF_TASK, "%s para:taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsDescSdmaReduce(fftsCtxsPtr, streamId, dstAddr, srcAddr, dataCount, datatype,
            redOp, fftsPubInfoPtr->useGraphConstructorV2));
    } else {
        CHK_RET(RefreshFftsDescSdmaReduce(fftsCtxsPtr, streamId, dstAddr, srcAddr, dataCount, datatype,
            redOp, fftsPubInfoPtr->useGraphConstructorV2));
    }
    *ctxIdx = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult GraphAddRdmaSendTask(void *fftsPubInfo, void *ctx, uint32_t streamId, u32 dbindex, u64 dbinfo,
    bool isCapture, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    PLF_CONFIG_INFO(PLF_TASK, "%s para:taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsDescRdmaSend(fftsCtxsPtr, streamId, dbindex, dbinfo,
            fftsPubInfoPtr->useGraphConstructorV2, isCapture));
    } else {
        CHK_RET(RefreshFftsDescRdmaSend(fftsCtxsPtr, streamId, dbindex, dbinfo,
            fftsPubInfoPtr->useGraphConstructorV2, isCapture));
    }
    *ctxIdx = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

// 因为tailCount == 0的话, addrListDevMemPtr和funcAddr是nullptr,所以此处不对addrListDevMemPtr和funcAddr做校验
HcclResult GraphAddVectorReduceTask(void *fftsPubInfo, void *ctx, uint32_t streamId, int count, void *addrListDevMemPtr,
    void *funcAddr, uint32_t blockDim, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsDescVectorReduce(fftsCtxsPtr, streamId, count, addrListDevMemPtr, funcAddr, blockDim,
            fftsPubInfoPtr->useGraphConstructorV2));
    } else {
        CHK_RET(RefreshFftsDescVectorReduce(fftsCtxsPtr, streamId, count, addrListDevMemPtr, funcAddr, blockDim,
            fftsPubInfoPtr->useGraphConstructorV2));
    }
    *ctxIdx = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

// 因为tailCount == 0的话, dst和src是nullptr,所以此处不对dst和src做校验
HcclResult GraphAddTailVectorReduceTask(void *fftsPubInfo, void *ctx, uint32_t streamId, void *dst, const void *src,
    u64 cnt, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxIdx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsDescSdma(fftsCtxsPtr, streamId, dst, src, cnt, SDMA_FP32_ATOMIC_MOVE_SQE,
            fftsPubInfoPtr->useGraphConstructorV2));
    } else {
        CHK_RET(RefreshFftsDescSdma(fftsCtxsPtr, streamId, dst, src, cnt, SDMA_FP32_ATOMIC_MOVE_SQE,
            fftsPubInfoPtr->useGraphConstructorV2));
    }
    *ctxIdx = fftsCtxsPtr->refreshIndex;
    return HCCL_SUCCESS;
}

HcclResult GraphAddVectorReduceArgs(void *fftsPubInfo, void *argsHandle)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(argsHandle);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    fftsPubInfoPtr->argsHandleList.push_back(argsHandle);
    return HCCL_SUCCESS;
}

HcclResult GraphAddRecordTaskById(void *fftsPubInfo, void *ctx, uint32_t streamId, u32 notifyID)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsSuccessorStart(notifyID, streamId, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2));
    } else {
        // V2的构图逻辑下, 这个片内的notify record如果是当前流第一个task, 会实例化一个空task作为附着点, 需要跳过
        if (fftsPubInfoPtr->useGraphConstructorV2 && fftsCtxsPtr->lastThreadIndex[streamId] == 0) {
            HCCL_DEBUG("cur stream first task is inchip notify record, need skip, ctxId[%u]", fftsCtxsPtr->refreshIndex);
            fftsCtxsPtr->refreshIndex++;
            fftsCtxsPtr->lastThreadIndex[streamId] = fftsCtxsPtr->refreshIndex;
        }
    }
    PLF_CONFIG_INFO(PLF_TASK, "%s para:taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex);
    return HCCL_SUCCESS;
}

HcclResult GraphAddWaitTaskById(void *fftsPubInfo, void *ctx, uint32_t streamId, u32 notifyID)
{
    CHK_PTR_NULL(fftsPubInfo);
    CHK_PTR_NULL(ctx);
    HcclFftsPubInfo *fftsPubInfoPtr = static_cast<HcclFftsPubInfo *>(fftsPubInfo);
    HcclFftsContextsInfo *fftsCtxsPtr = static_cast<HcclFftsContextsInfo*>(ctx);
    if (UNLIKELY(!FftsCtxReady(fftsCtxsPtr))) {
        CHK_RET(InitFftsSuccessorEnd(notifyID, streamId, fftsCtxsPtr, fftsPubInfoPtr->useGraphConstructorV2));
    } else {
        // 卡内notify使用successor间依赖关系替代
    }
    PLF_CONFIG_INFO(PLF_TASK, "%s para:taskIndex[%u]", __func__, fftsCtxsPtr->refreshTaskIndex);
    return HCCL_SUCCESS;
}