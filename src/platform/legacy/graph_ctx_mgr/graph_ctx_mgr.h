/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef HCOMM_GRAPH_CTX_MGR_H
#define HCOMM_GRAPH_CTX_MGR_H

#include <hccl/base.h>
#include "ffts_common.h"
#include "ffts_ctx_provider.h"
#include "rt_external.h"

using FftsSdmaSqeHeader = union {
    uint32_t word;
    struct {
        uint32_t opcode : 4;   // 0:非atomic搬运 1:atomic add 2:atomic max 3:atomic min 4:atomic equal 5:memory set
                               // 6:L2Cache Preload 7:L2Cache Prewriteback 8:L2Cache invalid 9:L2Cache Flush
        uint32_t datatype : 4; // 0:int8 1:int16 2:int32 6:fp16 normal 7:fp32 8:bf16 9:fp16 sat
        uint32_t ie2 : 1;      // 完成后是否上报中断; HCCL设为0
        uint32_t sssv : 1;     // source address对应的substream id是否有效; HCCL设为1
        uint32_t dssv : 1;     // destination address对应的substream id是否有效; HCCL设为1
        uint32_t sns : 1;      // source address对应的安全属性, 0:secure 1:non-secure; HCCL设为1
        uint32_t dns : 1;      // destination address对应的安全属性, 0:secure 1:non-secure; HCCL设为1
        uint32_t qos : 4;      // 0
        uint32_t sro : 1;      // 0
        uint32_t dro : 1;      // 0
        uint32_t partid : 8;   // 0
        uint32_t mpam : 1;     // 0
        uint32_t pmg : 2;      // 0
        uint32_t format : 1;   // 0
        uint32_t res6 : 1;     // 0
    } bit;
};

HcclResult GroupTasksByStreamId(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult UpdateInchipNotifyCtxID(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult FindInchipRecordPreCtx(const std::vector<HcclFfftsTaskInfo>& taskList, u32 index,
    HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult FindInchipWaitAfterCtx(const std::vector<HcclFfftsTaskInfo>& taskList, u32 index,
    bool& isPlaceHolderWait, HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult InitFftsPlaceHolder(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr);
void EnsureFftsContextsSize(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult InitTaskAndUpdateDependencies(s32 streamID, HcclFftsTaskType taskType, HcclFftsContextsInfo *&fftsCtxsPtr,
    u32 notfiyId = LEGACY_INVALID_UINT);
void EnsureFftsTaskInfosSize(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult InitFftsDescTaskInfo(s32 streamID, HcclFftsTaskType taskType, HcclFftsContextsInfo *&fftsCtxsPtr,
    u32 notfiyId = LEGACY_INVALID_UINT);
HcclResult DispatcherFFTSTaskRelationship(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult BuildOriginalGraph(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult ConstructFftsSqe(u32 timeout, rtFftsPlusSqe_t &fftsPlusSqe, HcclFftsContextsInfo *&fftsCtxsPtr,
    std::vector<void *> &argsHandleList);
HcclResult ConstructFftsTask(rtFftsPlusTaskInfo_t &task, rtFftsPlusSqe_t &fftsPlusSqe,
    HcclFftsContextsInfo *&fftsCtxsPtr, std::vector<void *> &argsHandleList);
HcclResult PrintFFTSDebugDetails(rtFftsPlusSqe_t &fftsPlusSqe, rtFftsPlusTaskInfo_t &task,
    HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult PrintOriginalGraph(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult FindInDegree0Task(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult TraverseEdges(u32 curTaskIndex, u32 nextTaskIndex, HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult AddDependencyEdge(u32 preCtxId, u32 afterCtxId, u32 preTaskIndex,
    u32 afterTaskIndex, HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult AddCtxExpasionSuccessor(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult AddExpasionEdgeCtx(u32 preCtxId, HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult InitFftsSuccessorStart(u32 notifyID, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr, bool useGraphConstructorV2);
HcclResult InitFftsDescNotifyRecordRemote(void *signal, s32 streamId, u64 signalAddr, HcclFftsContextsInfo *&fftsCtxsPtr, bool useGraphConstructorV2);
HcclResult ConstructFftsNotifyRecordRemoteCtx(void *signal, rtFftsPlusWriteValueCtx_t *ctx, u64 signalAddr);
HcclResult UpdataFftsDescRelationship(s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult InitFftsSuccessorEnd(u32 notifyID, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr, bool useGraphConstructorV2);
HcclResult InitFftsDescNotifyWait(void *signal, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr, bool useGraphConstructorV2);
HcclResult ConstructFFtsNotifyWaitCtx(void *signal, rtFftsPlusNotifyCtx_t* ctx);
HcclResult ConstructFftsNotifyCtx(u32 notifyID, uint16_t contextType, rtFftsPlusNotifyCtx_t* ctx);
HcclResult ConstructFftsSdmaCtx(void *dst, const void *src, u64 cnt, u32 sdmaSqeHeader,
    rtFftsPlusSdmaCtx_t *ctx, bool isInit);
HcclResult InitFftsDescSdma(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    u64 cnt, u32 sdmaSqeHeader, bool useGraphConstructorV2);
HcclResult RefreshFftsDescSdma(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    u64 cnt, u32 sdmaSqeHeader, bool useGraphConstructorV2);
HcclResult InitFftsDescMemcpy(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dstAddr, void *srcAddr,
    uint64_t size, bool useGraphConstructorV2);
HcclResult RefreshFftsDescMemcpy(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dstAddr, void *srcAddr,
    uint64_t size, bool useGraphConstructorV2);
HcclResult InitFftsDescSdmaReduce(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    uint64_t cnt, const HcclDataType datatype, HcclReduceOp redOp, bool useGraphConstructorV2);
HcclResult RefreshFftsDescSdmaReduce(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
    uint64_t cnt, const HcclDataType datatype, HcclReduceOp redOp, bool useGraphConstructorV2);
HcclResult InitFftsDescRdmaSend(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, u32 dbindex, u64 dbinfo,
    bool useGraphConstructorV2, bool isCapture = false);
HcclResult RefreshFftsDescRdmaSend(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, u32 dbindex, u64 dbinfo,
    bool useGraphConstructorV2, bool isCapture = false);
HcclResult ConstructFftsWriteValueCtx(u32 dbindex, u64 dbinfo, rtFftsPlusWriteValueCtx_t *ctx, bool isCapture);
    bool IsInvalidRdmaParam(u32 dbindex, u64 dbinfo);
HcclResult GetSqDepth(u32 dbindex, u32& sqDepth);
HcclResult GetSdmaSqeHeader(const HcclDataType &datatype, const HcclReduceOp &redOp,
    FftsSdmaSqeHeader &sdmaSqeHeader);
HcclResult ConstructFftsAicAivCtx(int count, void *addrListDevMemPtr, void *funcAddr, uint32_t blockDim,
    rtFftsPlusAicAivCtx_t *ctx, bool isInit);
HcclResult RefreshFftsDescVectorReduce(HcclFftsContextsInfo *&fftsCtxsPtr, uint32_t streamId, int count, void *addrListDevMemPtr,
    void *funcAddr, uint32_t blockDim, bool useGraphConstructorV2);
HcclResult InitFftsDescVectorReduce(HcclFftsContextsInfo *&fftsCtxsPtr, uint32_t streamId, int count, void *addrListDevMemPtr,
    void *funcAddr, uint32_t blockDim, bool useGraphConstructorV2);
HcclResult AssociateWaitToRecord(const HcclFfftsTaskInfo& curTask, s32 curStreamId,
    bool& isRecordFound, HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult BuildFFTSGraph(HcclFftsContextsInfo *&fftsCtxsPtr);
HcclResult GraphCtxMgrTaskRelationship(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr);

inline void UpdateLastThreadIndex(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr, bool useGraphConstructorV2)
{
    if (useGraphConstructorV2) {
        fftsCtxsPtr->lastThreadIndex[streamID] = fftsCtxsPtr->refreshIndex;
    }
    return;
}

inline void SkipFftsRefresh(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr, bool useGraphConstructorV2)
{
    fftsCtxsPtr->refreshIndex++;
    UpdateLastThreadIndex(streamID, fftsCtxsPtr, useGraphConstructorV2);
    return;
}
inline bool FftsCtxReady(HcclFftsContextsInfo *&fftsCtxsPtr)
{
    return fftsCtxsPtr->completed;
}
#endif // HCOMM_GRAPH_CTX_MGR_H