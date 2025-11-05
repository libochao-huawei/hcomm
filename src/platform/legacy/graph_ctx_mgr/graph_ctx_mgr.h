/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef GRAPH_CTX_MGR_H
#define GRAPH_CTX_MGR_H

#include <hccl/base.h>
#include "ffts_common.h"
#include "ffts_ctx_provider.h"
 
 
namespace GraphMgr {

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

class GraphCtxMgr {
public:
    GraphCtxMgr();
    ~GraphCtxMgr();
    void* GetGraphCtx(const char *key, uint32_t keyLen);
    void* GetGraphCtxV2(const char *key, uint32_t keyLen);
    HcclResult LaunchGraph(void *streamPtr, void *ctx, uint32_t timeout, uint32_t *ctxNum);
    HcclResult GraphAddRecordTask(void *ctx, uint32_t streamId, void *signal, bool inchip, uint32_t *ctxIdx);
    HcclResult GraphAddWaitTask(void *ctx, uint32_t streamId, void *signal, bool inchip, uint32_t *ctxIdx);
    HcclResult GraphAddMemcpyTask(void *ctx, uint32_t streamId, void *dstAddr, void *srcAddr, uint64_t size,
        uint32_t *ctxIdx);
    HcclResult GraphAddReduceTask(void *ctx, uint32_t streamId, void *dstAddr, const void *srcAddr, uint64_t dataCount,
        const HcclDataType datatype, HcclReduceOp redOp, uint32_t *ctxIdx);
    HcclResult GraphAddInlineReduceTask(void *ctx, uint32_t streamId, void *dstAddr, const void *srcAddr,
        uint64_t dataCount, const HcclDataType datatype, HcclReduceOp redOp, uint32_t *ctxIdx);
    HcclResult GraphAddRdmaSendTask(void *ctx, uint32_t streamId, u32 dbindex, u64 dbinfo,
        bool isCapture, uint32_t *ctxIdx);
    HcclResult GraphAddTailVectorReduceTask(void *ctx, uint32_t streamId, void *dst, const void *src,
        u64 cnt, uint32_t *ctxIdx);
    HcclResult GraphAddVectorReduceTask(void *ctx, uint32_t streamId, int count, void *addrListDevMemPtr,
        void *funcAddr, uint32_t blockDim, uint32_t *ctxIdx);
    HcclResult GraphAddVectorReduceArgs(void *argsHandle);
    HcclResult GraphAddRecordTaskById(void *ctx, uint32_t streamId, u32 notifyID);
    HcclResult GraphAddWaitTaskById(void *ctx, uint32_t streamId, u32 notifyID);
    void GraphDump(void *ctx);
protected:
    
private:
    HcclResult GroupTasksByStreamId(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult UpdateInchipNotifyCtxID(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult FindInchipRecordPreCtx(const std::vector<HcclFfftsTaskInfo>& taskList, u32 index,
        HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult FindInchipWaitAfterCtx(const std::vector<HcclFfftsTaskInfo>& taskList, u32 index,
        bool& isPlaceHolderWait, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult InitFftsPlaceHolder(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr);
    void EnsureFftsContextsSize(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult InitTaskAndUpdateDependencies(s32 streamID, HcclFftsTaskType taskType, HcclFftsContextsInfo *&fftsCtxsPtr,
        u32 notfiyId = INVALID_UINT);
    void EnsureFftsTaskInfosSize(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult InitFftsDescTaskInfo(s32 streamID, HcclFftsTaskType taskType, HcclFftsContextsInfo *&fftsCtxsPtr,
        u32 notfiyId = INVALID_UINT);
    HcclResult DispatcherFFTSTaskRelationship(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult BuildOriginalGraph(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult ConstructFftsSqe(rtFftsPlusSqe_t &fftsPlusSqe, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult ConstructFftsTask(rtFftsPlusTaskInfo_t &task, rtFftsPlusSqe_t &fftsPlusSqe,
        HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult PrintFFTSDebugDetails(rtFftsPlusSqe_t &fftsPlusSqe, rtFftsPlusTaskInfo_t &task,
        HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult PrintOriginalGraph(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult FindInDegree0Task(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult TraverseEdges(u32 curTaskIndex, u32 nextTaskIndex, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult AddDependencyEdge(u32 preCtxId, u32 afterCtxId, u32 preTaskIndex,
        u32 afterTaskIndex, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult AddCtxExpasionSuccessor(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult AddExpasionEdgeCtx(u32 preCtxId, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult InitFftsSuccessorStart(u32 notifyID, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult InitFftsDescNotifyRecordRemote(void *signal, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult ConstructFftsNotifyRecordRemoteCtx(void *signal, rtFftsPlusWriteValueCtx_t *ctx);
    HcclResult UpdataFftsDescRelationship(s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult InitFftsSuccessorEnd(u32 notifyID, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult InitFftsDescNotifyWait(void *signal, s32 streamId, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult ConstructFFtsNotifyWaitCtx(void *signal, rtFftsPlusNotifyCtx_t* ctx);
    HcclResult ConstructFftsNotifyCtx(u32 notifyID, uint16_t contextType, rtFftsPlusNotifyCtx_t* ctx);
    HcclResult ConstructFftsSdmaCtx(void *dst, const void *src, u64 cnt, u32 sdmaSqeHeader,
        rtFftsPlusSdmaCtx_t *ctx, bool isInit);
    HcclResult InitFftsDescSdma(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
        u64 cnt, u32 sdmaSqeHeader);
    HcclResult RefreshFftsDescSdma(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
        u64 cnt, u32 sdmaSqeHeader);
    HcclResult InitFftsDescMemcpy(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dstAddr, void *srcAddr,
        uint64_t size);
    HcclResult RefreshFftsDescMemcpy(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dstAddr, void *srcAddr,
        uint64_t size);
    HcclResult InitFftsDescSdmaReduce(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
        uint64_t cnt, const HcclDataType datatype, HcclReduceOp redOp);
    HcclResult RefreshFftsDescSdmaReduce(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, void *dst, const void *src,
        uint64_t cnt, const HcclDataType datatype, HcclReduceOp redOp);
    HcclResult InitFftsDescRdmaSend(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, u32 dbindex, u64 dbinfo,
        bool isCapture = false);
    HcclResult RefreshFftsDescRdmaSend(HcclFftsContextsInfo *&fftsCtxsPtr, s32 streamId, u32 dbindex, u64 dbinfo,
        bool isCapture = false);
    HcclResult ConstructFftsWriteValueCtx(u32 dbindex, u64 dbinfo, rtFftsPlusWriteValueCtx_t *ctx, bool isCapture);
    bool IsInvalidRdmaParam(u32 dbindex, u64 dbinfo);
    HcclResult GetSqDepth(u32 dbindex, u32& sqDepth);
    HcclResult GetSdmaSqeHeader(const HcclDataType &datatype, const HcclReduceOp &redOp,
        FftsSdmaSqeHeader &sdmaSqeHeader);
    HcclResult ConstructFftsAicAivCtx(int count, void *addrListDevMemPtr, void *funcAddr, uint32_t blockDim,
        rtFftsPlusAicAivCtx_t *ctx, bool isInit);
    HcclResult RefreshFftsDescVectorReduce(HcclFftsContextsInfo *&fftsCtxsPtr, uint32_t streamId, int count, void *addrListDevMemPtr,
        void *funcAddr, uint32_t blockDim);
    HcclResult InitFftsDescVectorReduce(HcclFftsContextsInfo *&fftsCtxsPtr, uint32_t streamId, int count, void *addrListDevMemPtr,
        void *funcAddr, uint32_t blockDim);
    HcclResult AssociateWaitToRecord(const HcclFfftsTaskInfo& curTask, s32 curStreamId,
        bool& isRecordFound, HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult BuildFFTSGraph(HcclFftsContextsInfo *&fftsCtxsPtr);
    HcclResult GraphCtxMgrTaskRelationship(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr);

    inline void UpdateLastThreadIndex(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr) const
    {
        if (useGraphConstructorV2_) {
            fftsCtxsPtr->lastThreadIndex[streamID] = fftsCtxsPtr->refreshIndex;
        }
        return;
    }

    inline void SkipFftsRefresh(s32 streamID, HcclFftsContextsInfo *&fftsCtxsPtr) const
    {
        fftsCtxsPtr->refreshIndex++;
        UpdateLastThreadIndex(streamID, fftsCtxsPtr);
        return;
    }
    inline bool FftsCtxReady(HcclFftsContextsInfo *&fftsCtxsPtr)
    {
        return fftsCtxsPtr->completed;
    }

    u32 timeout_{0};
    FftsCtxProvider fftsCtxProvider;
    bool useGraphConstructorV2_{false};
    std::vector<void*> argsHandleList_;
};
}
#endif // GRAPH_CTX_MGR_H