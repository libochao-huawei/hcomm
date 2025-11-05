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

void *GraphMgrInit()
{
    return new (std::nothrow) GraphMgr::GraphCtxMgr();
}

void GraphMgrDeInit(void *graphMgr){
    if (graphMgr == nullptr) {
        HCCL_ERROR("graphMgr is nullpty");
        return;
    }
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    delete graphMgrPtr;
    return;
}

void* GetGraphCtx(void *graphMgr, const char *key, uint32_t keyLen)
{
    CHK_PRT_RET(key == nullptr, HCCL_ERROR("GetGraphCtx key is nullpty"), nullptr);
    CHK_PRT_RET(graphMgr == nullptr, HCCL_ERROR("graphMgr is nullpty"), nullptr);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GetGraphCtx(key, keyLen);
}

void* GetGraphCtxV2(void *graphMgr, const char *key, uint32_t keyLen)
{
    CHK_PRT_RET(key == nullptr, HCCL_ERROR("GetGraphCtx key is nullpty"), nullptr);
    CHK_PRT_RET(graphMgr == nullptr, HCCL_ERROR("graphMgr key is nullpty"), nullptr);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GetGraphCtxV2(key, keyLen);
}

HcclResult LaunchGraph(void *graphMgr, void *streamPtr, void *ctx, uint32_t timeout, uint32_t *ctxNum)
{
    CHK_PTR_NULL(streamPtr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxNum);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->LaunchGraph(streamPtr, ctx, timeout, ctxNum);
}

void GraphDump(void *graphMgr, void *ctx)
{
    if (ctx == nullptr || graphMgr == nullptr) {
        HCCL_ERROR("GraphDump ctx or graphMgr is nullptr");
        return;
    }
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphDump(ctx);
}

HcclResult GraphAddRecordTask(void *graphMgr, void *ctx, uint32_t streamId, void *signal, bool inchip, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(signal);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddRecordTask(ctx, streamId, signal, inchip, ctxIdx);
}

HcclResult GraphAddWaitTask(void *graphMgr, void *ctx, uint32_t streamId, void *signal, bool inchip, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(signal);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddWaitTask(ctx, streamId, signal, inchip, ctxIdx);
}

HcclResult GraphAddMemcpyTask(void *graphMgr, void *ctx, uint32_t streamId, void *dstAddr, void *srcAddr,
    uint64_t size, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(dstAddr);
    CHK_PTR_NULL(srcAddr);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddMemcpyTask(ctx, streamId, dstAddr, srcAddr, size, ctxIdx);
}

HcclResult GraphAddReduceTask(void *graphMgr, void *ctx, uint32_t streamId, void *dstAddr, const void *srcAddr, uint64_t dataCount,
    const HcclDataType datatype, HcclReduceOp redOp, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(dstAddr);
    CHK_PTR_NULL(srcAddr);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddReduceTask(ctx, streamId, dstAddr, srcAddr, dataCount, datatype,
        redOp, ctxIdx);
}

HcclResult GraphAddInlineReduceTask(void *graphMgr, void *ctx, uint32_t streamId, void *dstAddr, const void *srcAddr,
    uint64_t dataCount, const HcclDataType datatype, HcclReduceOp redOp, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(dstAddr);
    CHK_PTR_NULL(srcAddr);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddInlineReduceTask(ctx, streamId, dstAddr, srcAddr, dataCount, datatype,
        redOp, ctxIdx);
}

HcclResult GraphAddRdmaSendTask(void *graphMgr, void *ctx, uint32_t streamId, u32 dbindex, u64 dbinfo,
    bool isCapture, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddRdmaSendTask(ctx, streamId, dbindex, dbinfo, isCapture, ctxIdx);
}

// 因为tailCount == 0的话, addrListDevMemPtr和funcAddr是nullptr,所以此处不对addrListDevMemPtr和funcAddr做校验
HcclResult GraphAddVectorReduceTask(void *graphMgr, void *ctx, uint32_t streamId, int count, void *addrListDevMemPtr,
    void *funcAddr, uint32_t blockDim, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddVectorReduceTask(ctx, streamId, count, addrListDevMemPtr, funcAddr,
        blockDim, ctxIdx);
}

// 因为tailCount == 0的话, dst和src是nullptr,所以此处不对dst和src做校验
HcclResult GraphAddTailVectorReduceTask(void *graphMgr, void *ctx, uint32_t streamId, void *dst, const void *src,
    u64 cnt, uint32_t *ctxIdx)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    CHK_PTR_NULL(ctxIdx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddTailVectorReduceTask(ctx, streamId, dst, src, cnt, ctxIdx);
}

HcclResult GraphAddVectorReduceArgs(void *graphMgr, void *argsHandle)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(argsHandle);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddVectorReduceArgs(argsHandle);
}

HcclResult GraphAddRecordTaskById(void *graphMgr, void *ctx, uint32_t streamId, u32 notifyID)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddRecordTaskById(ctx, streamId, notifyID);
}

HcclResult GraphAddWaitTaskById(void *graphMgr, void *ctx, uint32_t streamId, u32 notifyID)
{
    CHK_PTR_NULL(graphMgr);
    CHK_PTR_NULL(ctx);
    GraphMgr::GraphCtxMgr* graphMgrPtr = static_cast<GraphMgr::GraphCtxMgr*>(graphMgr);
    return graphMgrPtr->GraphAddRecordTaskById(ctx, streamId, notifyID);
}