/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "externalinput.h"
#include "adapter_rts.h"
#include "log.h"
#include "dtype_common.h"
#include "dispatcher_graph.h"
#include "hccl_tbe_task.h"
#include "graph_ctx_mgr_common.h"
#include "config_plf_log.h"

constexpr u32 UB_BLOCK_SIZE = 32;
constexpr u64 TBE_REDUCE_MAX_COUNT = INT32_MAX;

__attribute__((weak)) HcclResult GraphAddRecordTaskWithSignalAddr(void *fftsPubInfo, void *ctx, uint32_t streamId,
    void *signal, bool inchip, u64 signalAddr, uint32_t *ctxIdx);
__attribute__((weak)) HcclResult LaunchGraphWithTaskInfo(void *fftsPubInfo, void *streamPtr, void *ctx,
    uint32_t timeout, uint32_t *ctxNum, rtFftsPlusTaskInfo_t *task);

namespace hccl {
DispatcherGraph::DispatcherGraph(const s32 deviceLogicId)
    : DispatcherPub(deviceLogicId), fftsCtxsPtr(nullptr), disableFfts_(true), multiQpMode_(false)
{}

DispatcherGraph::~DispatcherGraph()
{}

void DispatcherGraph::SetNormalMode()
{
    disableFfts_ = true;
}

HcclResult DispatcherGraph::SetMultiQpMode(bool multiQpMode)
{
    multiQpMode_ = multiQpMode;
    HCCL_DEBUG("[MultiQp][DispatcherGraph::SetMultiQpMode] [%d]", multiQpMode);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult DispatcherGraph::ResetGraphCtx(bool enableCache, const std::string &key, bool useGraphConstructorV2)
{
    disableFfts_ = false;
    if (GetWorkflowMode() != HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE) {
        HCCL_DEBUG("ffts task is disabled.");
        disableFfts_ = true;
    } else {
        if (multiQpMode_) {
            enableCache = false;
        }
        
        std::string sKey = key;
        if (UNLIKELY(!enableCache)) {
            // 当enableCache使能时,key传空值，用来区分当enableCache
            sKey = "";
        }
        HCCL_INFO("useGraphConstructorV2[%d] sKey[%s] length[%u] key[%s]",
            useGraphConstructorV2, sKey.c_str(), sKey.length(), key.c_str());
        if (useGraphConstructorV2) {
            fftsCtxsPtr = GetGraphCtxV2(fftsPubInfo_, sKey.c_str(), sKey.length());
        } else {
            fftsCtxsPtr = GetGraphCtx(fftsPubInfo_, sKey.c_str(), sKey.length());
        }
        CHK_PTR_NULL(fftsCtxsPtr);
        disableFfts_ = false;
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::LaunchTasksEx(Stream &stream, std::vector<Stream> &subStreams)
{
    if (UNLIKELY(disableFfts_)) {
        return HCCL_SUCCESS;
    }

    uint64_t beginTime = GetMsprofSysCycleTime();
    CHK_PTR_NULL(fftsCtxsPtr); // 检查Context是否进行过Reset

    u32 timeout = 0;
    // 配置notify wait 超时时间
    // 因为老版本用户设置HCCL_EXEC_TIMEOUT为0，hccl将0传递给rts,rts将0转换为1770s传递给硬件, 未达到永不超时效果，不符合预期；
    // 所以现版用户配置为0时，hccl转换成65535，rts识别到65535后会又会转换成0，去硬件设置永不超时
    if (execTimeOut_  == 0) {
        timeout = FFTS_TIMEOUT_MAX;
    // 因为65535被当作永不超时处理，所以当用户配置65535时需要改变他的值，防止误错做成永不超时
    } else if (execTimeOut_  == FFTS_TIMEOUT_MAX) {
        timeout = FFTS_TIMEOUT_MAX - 1;
    } else {
        timeout = execTimeOut_ ;
    }
    u32 ctxNum;
    const void *descBuf = nullptr;
    size_t descBufLen = 0;
    rtFftsPlusTaskInfo_t task = {};
    if (LaunchGraphWithTaskInfo != nullptr) {
        CHK_RET(LaunchGraphWithTaskInfo(fftsPubInfo_, stream.ptr(), fftsCtxsPtr, timeout, &ctxNum, &task));
        descBuf = task.descBuf;
        descBufLen = task.descBufLen;
    } else {
        CHK_RET(LaunchGraph(fftsPubInfo_, stream.ptr(), fftsCtxsPtr, timeout, &ctxNum));
    }
    disableFfts_ = true;
    // 调用回调来保存task信息
    if (callback_ != nullptr) {
        struct TaskPara taskPara;
        taskPara.type = TaskType::TASK_GRAPH_LAUNCH;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.graphLaunch.ctxNum = ctxNum;
        taskPara.descBuf = descBuf;
        taskPara.descBufLen = descBufLen;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }

    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::GetNotifyDfxInfo(HcclRtNotify signal, u32 userRank, u64 &offset, u32 &remoteUserRank,
    u64 &notifyID)
{
    if (offset == INVALID_U64) {
        CHK_RET(hrtNotifyGetOffset(static_cast<HcclRtNotify>(signal), offset));
    }
    notifyID = userRank;
    notifyID = (notifyID << 32) | (offset & 0x00000000FFFFFFFF);  // 0x00000000FFFFFFFF用于取offset的低32位
    remoteUserRank = (remoteUserRank == INVALID_UINT) ? static_cast<u32>(notifyID >> 32) : remoteUserRank;
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::SignalTaskParaSave(HcclRtNotify signal, Stream &stream, u32 userRank, u32 remoteUserRank,
    u64 offset, s32 stage, TaskType taskType, uint64_t beginTime, u32 ctxIdx)
{
    if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        u64 notifyID;
        CHK_RET(GetNotifyDfxInfo(signal, userRank, offset, remoteUserRank, notifyID));
        // 调用回调来保存task信息
        hccl::TaskParaNotify para(notifyID, stage, remoteUserRank, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.notify = para;
        taskPara.type = taskType;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        u64 notifyID;
        CHK_RET(GetNotifyDfxInfo(signal, userRank, offset, remoteUserRank, notifyID));
        hccl::TaskParaNotify para(notifyID, stage, remoteUserRank, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.notify = para;
        taskPara.type = taskType;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }

    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::SignalRecord(HcclRtNotify signal, Stream &stream, u32 userRank, u64 offset, s32 stage,
    bool inchip, u64 signalAddr, u32 notifyId)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    (void)notifyId;
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::SignalRecord(signal, stream, userRank, offset, stage, inchip);
    }
    u32 ctxIdx;
    if (GraphAddRecordTaskWithSignalAddr != nullptr) {
        CHK_RET(GraphAddRecordTaskWithSignalAddr(fftsPubInfo_, fftsCtxsPtr, stream.id(), signal, inchip, signalAddr, &ctxIdx));
    } else {
        CHK_RET(GraphAddRecordTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), signal, inchip, &ctxIdx));
    }
    if (!inchip && ctxIdx > 0) {
        CHK_RET(SignalTaskParaSave(signal, stream, userRank, INVALID_UINT,
                offset, stage, TaskType::TASK_NOTIFY_RECORD, beginTime, ctxIdx));
    }

    if (HcclCheckLogLevel(HCCL_LOG_INFO) || (GetExternalInputDebugConfig() & PLF_TASK)) {
        u64 notifyID = userRank;
        u32 remoteUserRank = INVALID_UINT;
        CHK_RET(GetNotifyDfxInfo(signal, userRank, offset, remoteUserRank, notifyID));
        PLF_CONFIG_INFO(PLF_TASK,
            "%s para: notifyId[0x%016llx] streamId[%u] userRank[%u] remoteUserRank[%u] offset[%llu] stage[%d] inchip[%d]",
            __func__, notifyID, stream.id(), userRank, remoteUserRank, offset, stage, inchip);
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::SignalWait(HcclRtNotify signal, Stream &stream, u32 userRank, u32 remoteUserRank, s32 stage,
    bool inchip, u32 notifyId, u32 timeOut)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::SignalWait(signal, stream, userRank, remoteUserRank, stage, inchip, notifyId, timeOut);
    }
    u32 ctxIdx;
    CHK_RET(GraphAddWaitTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), signal, inchip, &ctxIdx));
    if (!inchip && ctxIdx > 0) {
        CHK_RET(SignalTaskParaSave(signal, stream, userRank, remoteUserRank,
                INVALID_U64, stage, TaskType::TASK_NOTIFY_WAIT, beginTime, ctxIdx));
    }

    if (HcclCheckLogLevel(HCCL_LOG_INFO) || (GetExternalInputDebugConfig() & PLF_TASK)) {
        u64 notifyID = userRank;
        u64 offset = INVALID_U64;
        CHK_RET(GetNotifyDfxInfo(signal, userRank, offset, remoteUserRank, notifyID));
        PLF_CONFIG_INFO(PLF_TASK,
            "%s para: notifyId[0x%016llx] streamId[%u] userRank[%u] remoteUserRank[%u] offset[%llu] stage[%d] inchip[%d]",
            __func__, notifyID, stream.id(), userRank, remoteUserRank, offset, stage, inchip);
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::MemcpyAsync(hccl::DeviceMem &dst, const hccl::DeviceMem &src, hccl::Stream &stream,
    u32 remoteUserRank, hccl::LinkType inLinkType)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    if (dst.size() < src.size()) {
        HCCL_ERROR(
            "The size of dst is smaller than that of src. dst addr[%p], dst size[%llu], src addr[%p], src size[%llu]",
            dst.ptr(), dst.size(), src.ptr(), src.size());
        return HCCL_E_PTR;
    }

    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::MemcpyAsync(dst, src, stream, remoteUserRank, inLinkType);
    }
    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: dst[%p] destMax[%llu] src[%p] count[%llu] rtMemcpyKind[%d] inLinkType[%d] remoteUserRank[%u] streamId[%u]",
        __func__, dst.ptr(), dst.size(), src.ptr(), src.size(), HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE,
        inLinkType, remoteUserRank, stream.id());
    u32 ctxIdx;
    CHK_RET(GraphAddMemcpyTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), dst.ptr(), src.ptr(), src.size(), &ctxIdx));
    // 调用回调来保存task信息
    if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        hccl::TaskParaDMA para(src.ptr(), dst.ptr(), src.size(), inLinkType, remoteUserRank,
            hccl::RdmaType::RDMA_TYPE_RESERVED, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_SDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        hccl::TaskParaDMA para(src.ptr(), dst.ptr(), src.size(), inLinkType, remoteUserRank,
            hccl::RdmaType::RDMA_TYPE_RESERVED, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_SDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::ReduceAsync(const void *src, void *dst, u64 dataCount, const HcclDataType datatype,
    HcclReduceOp redOp, Stream &stream, HcclReduceType reduceType)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::ReduceAsync(src, dst, dataCount, datatype, redOp, stream, reduceType);
    }
    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: src[%p] dst[%p] dataCount[%llu] datatype[%s] redOp[%s] reduceType[%d]  streamID[%u]",
        __func__, src, dst, dataCount, GetDataTypeEnumStr(datatype).c_str(),
        GetReduceOpEnumStr(redOp).c_str(), reduceType, stream.id());

    if (reduceType == HcclReduceType::HCCL_TBE_REDUCE) {
        // dtype=int64 或者 redOp=prod 都会走TbeReduce, 算法层控制的
        return TbeReduceAsync(src, dst, dataCount, datatype, redOp, stream, dst);
    }

    u32 ctxIdx = 0;
    CHK_RET(GraphAddReduceTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), dst, src, dataCount, datatype,
        redOp, &ctxIdx));
    // 调用回调来保存 task 信息
    if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        hccl::TaskParaReduce para(src, dst, dataCount * SIZE_TABLE[datatype], redOp, datatype,
            LinkType::LINK_ONCHIP, INVALID_VALUE_RANKID, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.reduce = para;
        taskPara.type = TaskType::TASK_REDUCE_INLINE;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        hccl::TaskParaReduce para(src, dst, dataCount * SIZE_TABLE[datatype], redOp, datatype,
                LinkType::LINK_ONCHIP, INVALID_VALUE_RANKID, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.reduce = para;
        taskPara.type = TaskType::TASK_REDUCE_INLINE;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }

    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::InlineReduceAsync(const void *src, u64 dataCount, const HcclDataType datatype,
    HcclReduceOp redOp, Stream &stream, void *dst, u32 remoteUserRank, hccl::LinkType inLinkType)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::InlineReduceAsync(
            src, dataCount, datatype, redOp, stream, dst, remoteUserRank, inLinkType);
    }
    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: src[%p] dst[%p] dataCount[%llu] datatype[%s] redOp[%s] inLinkType[%d] remoteUserRank[%u]  streamID[%u]",
            __func__, src, dst, dataCount, GetDataTypeEnumStr(datatype).c_str(), GetReduceOpEnumStr(redOp).c_str(),
            inLinkType, remoteUserRank, stream.id());
    u32 ctxIdx = 0;
    CHK_RET(GraphAddInlineReduceTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), dst, src, dataCount, datatype,
        redOp, &ctxIdx));

    // 调用回调来保存 task 信息
    if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        hccl::TaskParaReduce para(src, dst, dataCount * SIZE_TABLE[datatype], redOp, datatype, inLinkType,
            remoteUserRank, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.reduce = para;
        taskPara.type = TaskType::TASK_REDUCE_INLINE;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        hccl::TaskParaReduce para(src, dst, dataCount * SIZE_TABLE[datatype], redOp, datatype, inLinkType,
            remoteUserRank, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.reduce = para;
        taskPara.type = TaskType::TASK_REDUCE_INLINE;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::RdmaSend(u32 dbindex, u64 dbinfo, const struct SendWr &wr, hccl::Stream &stream,
    u32 remoteUserRank, bool isCapture)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::RdmaSend(dbindex, dbinfo, wr, stream, remoteUserRank);
    }

    u64 notifyID = (static_cast<u64>(remoteUserRank) << 32) | (0x00000000FFFFFFFF);
    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: dbindex[%u], dbinfo[%llu], notifyId[0x%016llx], remoteUserRank[%u], isCapture[%d], streamID[%u]",
        __func__, dbindex, dbinfo, notifyID, remoteUserRank, stream.id(), isCapture);

    u32 ctxIdx = 0;
    CHK_RET(GraphAddRdmaSendTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), dbindex, dbinfo, isCapture, &ctxIdx));
    // 调用回调来保存task信息
    if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        // 0x00000000FFFFFFFF usrrank位于notifyID的高32位
        hccl::TaskParaDMA para(reinterpret_cast<void *>(static_cast<uintptr_t>(wr.bufList[0].addr)),
                            reinterpret_cast<void *>(static_cast<uintptr_t>(wr.dstAddr)),
                            wr.bufList[0].len, notifyID, hccl::LinkType::LINK_ROCE, RdmaType::RDMA_SEND_PAYLOAD,
                            (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_RDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        hccl::TaskParaDMA para(reinterpret_cast<void *>(static_cast<uintptr_t>(wr.bufList[0].addr)),
                            reinterpret_cast<void *>(static_cast<uintptr_t>(wr.dstAddr)),
                            wr.bufList[0].len, notifyID, hccl::LinkType::LINK_ROCE, RdmaType::RDMA_SEND_PAYLOAD,
                            (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_RDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::RdmaSend(u32 dbindex, u64 dbinfo, const struct SendWr &wr, hccl::Stream &stream,
    u32 userRank, u64 offset, bool isCapture)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::RdmaSend(dbindex, dbinfo, wr, stream, userRank, offset);
    }

    u64 notifyID = (static_cast<u64>(userRank) << 32) | (offset & 0x00000000FFFFFFFF);
    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: dbindex[%u], dbinfo[%llu], notifyId[0x%016llx], userRank[%u], offset[%llu], streamID[%u]",
        __func__, dbindex, dbinfo, notifyID, userRank, offset, stream.id());

    u32 ctxIdx = 0;
    CHK_RET(GraphAddRdmaSendTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), dbindex, dbinfo, isCapture, &ctxIdx));
    // 调用回调来保存task信息
     if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        // 0x00000000FFFFFFFF usrrank位于notifyID的高32位
        hccl::TaskParaDMA para(reinterpret_cast<void *>(static_cast<uintptr_t>(wr.bufList[0].addr)),
                            reinterpret_cast<void *>(static_cast<uintptr_t>(wr.dstAddr)),
                            wr.bufList[0].len, notifyID, hccl::LinkType::LINK_ROCE, RdmaType::RDMA_SEND_NOTIFY,
                            (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_RDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        hccl::TaskParaDMA para(reinterpret_cast<void *>(static_cast<uintptr_t>(wr.bufList[0].addr)),
                            reinterpret_cast<void *>(static_cast<uintptr_t>(wr.dstAddr)),
                            wr.bufList[0].len, notifyID, hccl::LinkType::LINK_ROCE, RdmaType::RDMA_SEND_NOTIFY,
                            (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_RDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::VectorReduce(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
    HcclReduceOp redOp, Stream &stream, const void *dst)
{
    TbeReduceArg args{};
    if (count != 0) {
#ifndef HCCD
        TbeReduceParam param;
        std::vector<void *> overflowAddrs;
        overflowAddrs.push_back(overflowAddr_);
        param.src1 = const_cast<void*>(src1);
        param.src2 = const_cast<void*>(src2);
        param.dst = const_cast<void*>(dst);
        param.count = count;
        param.dataType = dataType;
        param.redOp = redOp;
        CHK_RET(HcclTbeReduceGenArgs(&param, stream.ptr(), overflowAddrs.data(), overflowAddrs.size(),
            &args, deviceLogicId_));
#else
        HCCL_ERROR("[DispatcherGraph][VectorReduce] does not support this interface.");
        return HCCL_E_PARA;
#endif
        GraphAddVectorReduceArgs(fftsPubInfo_, args.argsHandle);
    }
    CHK_RET(SetGraphDescVectorReduce(src1, dst, count, args.addrListDevMem, args.funcAddr,
        args.blockDim, dataType, redOp, stream));
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::VectorReduceLoop(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
    HcclReduceOp redOp, Stream &stream, const void *dst)
{
    const u32 unitSize = SIZE_TABLE[dataType];
    void *currentSrc1 = const_cast<void *>(src1);
    void *currentSrc2 = const_cast<void *>(src2);
    void *currentDst = const_cast<void *>(dst);
 
    // 计算出字节数为32字节整倍数的最大count
    const u64 maxCountPerLoop = ((TBE_REDUCE_MAX_COUNT * unitSize) / UB_BLOCK_SIZE * UB_BLOCK_SIZE) / unitSize;
 
    u64 countLeft = count;

    // 使用do while循环，是为了保证count为0时也进入一次VectorReduce，避免子图复用出错
    do  {
        u64 currentCount = countLeft > maxCountPerLoop ? maxCountPerLoop : countLeft;
        HCCL_DEBUG(
            "[VectorReduceLoop] currentCount[%llu], countLeft[%llu], currentSrc1[%p], currentSrc2[%p], currentDst[%p]",
            currentCount, countLeft, currentSrc1, currentSrc2, currentDst);
 
        CHK_RET(VectorReduce(currentSrc1, currentSrc2, currentCount, dataType, redOp, stream, currentDst));
 
        currentSrc1 = static_cast<void *>(static_cast<s8 *>(currentSrc1) + currentCount * unitSize);
        currentSrc2 = static_cast<void *>(static_cast<s8 *>(currentSrc2) + currentCount * unitSize);
        currentDst = static_cast<void *>(static_cast<s8 *>(currentDst) + currentCount * unitSize);
        countLeft -= currentCount;
    } while (countLeft > 0);

    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::SetGraphTailVectorReduceDescSdma(void *devMem, const void *tailSrc, u64 count,
    const HcclDataType dataType, HcclReduceOp redOp, Stream &stream)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    u32 ctxIdx = 0;
    CHK_RET(GraphAddTailVectorReduceTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), devMem, tailSrc, count, &ctxIdx));

    // 调用回调来保存 task 信息
    if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        hccl::TaskParaDMA para(tailSrc, devMem, count, LinkType::LINK_ONCHIP, INVALID_VALUE_RANKID,
            hccl::RdmaType::RDMA_TYPE_RESERVED, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_SDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        hccl::TaskParaDMA para(tailSrc, devMem, count, LinkType::LINK_ONCHIP, INVALID_VALUE_RANKID,
            hccl::RdmaType::RDMA_TYPE_RESERVED, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.dma = para;
        taskPara.type = TaskType::TASK_SDMA;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::SetGraphDescVectorReduce(const void *src, const void *dst, int count, void *addrListDevMemPtr,
    void *funcAddr, uint32_t numBlocks, const HcclDataType dataType, HcclReduceOp redOp, Stream &stream)
{
    uint64_t beginTime = GetMsprofSysCycleTime();
    u32 ctxIdx = 0;
    CHK_RET(GraphAddVectorReduceTask(fftsPubInfo_, fftsCtxsPtr, stream.id(), count, addrListDevMemPtr,
        funcAddr, numBlocks, &ctxIdx));

    // 调用回调来保存 task 信息
    if (DispatcherPub::IsProfSubscribeAdditionInfo() && callback_ != nullptr) {
        hccl::TaskParaReduce para(src, dst, count * SIZE_TABLE[dataType], redOp, dataType,
            LinkType::LINK_ONCHIP, INVALID_VALUE_RANKID, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.reduce = para;
        taskPara.type = TaskType::TASK_REDUCE_TBE;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_PROFILING;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    if (GetExternalInputHcclEnableFfts() && GetWorkflowMode() == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE &&
        GetExternalInputTaskExceptionSwitch() == 1 && callback_ != nullptr) {
        hccl::TaskParaReduce para(src, dst, count * SIZE_TABLE[dataType], redOp, dataType,
            LinkType::LINK_ONCHIP, INVALID_VALUE_RANKID, (ctxIdx - 1));
        struct TaskPara taskPara;
        taskPara.stream = stream.ptr();
        taskPara.isMainStream = stream.IsMainStream();
        taskPara.beginTime = beginTime;
        taskPara.reduce = para;
        taskPara.type = TaskType::TASK_REDUCE_TBE;
        taskPara.isFftsDispatcher = true;
        taskPara.profilerType = ProfilerType::TASK_EXCEPTION;
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::TailVectorReduce(const void *tailSrc1, const void *tailSrc2, u64 tailCount,
    const HcclDataType dataType, HcclReduceOp redOp, Stream &stream, void *tailDst)
{
    void *devMem1 = nullptr;
    void *devMem2 = nullptr;
    TbeReduceArg args{};
    if (tailCount != 0) {
#ifndef HCCD
        CHK_RET(DevMemMalloc(stream.ptr(), devMem1, devMem2));
        TbeReduceParam param;
        std::vector<void *> overflowAddrs;
        overflowAddrs.push_back(overflowAddr_);
        param.src1 = devMem1;
        param.src2 = devMem2;
        param.dst = devMem2;
        param.count = tailCount;
        param.dataType = dataType;
        param.redOp = redOp;
        CHK_RET(HcclTbeReduceGenArgs(&param, stream.ptr(), overflowAddrs.data(), overflowAddrs.size(),
            &args, deviceLogicId_));
#else
        HCCL_ERROR("[DispatcherGraph][VectorReduce] does not support this interface.");
        return HCCL_E_PARA;
#endif
        GraphAddVectorReduceArgs(fftsPubInfo_, args.argsHandle);
    }
    u64 dataCount = tailCount * SIZE_TABLE[dataType];

    CHK_RET(SetGraphTailVectorReduceDescSdma(devMem1, tailSrc1, dataCount, dataType, redOp, stream));
    CHK_RET(SetGraphTailVectorReduceDescSdma(devMem2, tailSrc2, dataCount, dataType, redOp, stream));
    CHK_RET(SetGraphDescVectorReduce(devMem1, tailDst, tailCount, args.addrListDevMem, args.funcAddr,
        args.blockDim, dataType, redOp, stream));
    CHK_RET(SetGraphTailVectorReduceDescSdma(tailDst, devMem2, dataCount, dataType, redOp, stream));
    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::TbeReduceAsync(const void *src1, const void *src2, u64 count, const HcclDataType dataType,
    HcclReduceOp redOp, Stream &stream, const void *dst)
{
    void *tailSrc1 = nullptr;
    void *tailSrc2 = nullptr;
    void *tailDst = nullptr;
    u64 headCount = 0;
    u64 tailCount = 0;
#ifndef HCCD
    CHK_RET(JudgeIsTail(src1, src2, dst, count, dataType, headCount, tailCount, tailSrc1, tailSrc2,
        tailDst));
#else
        HCCL_ERROR("[DispatcherGraph][TbeReduceAsync] does not support this interface.");
        return HCCL_E_PARA;
#endif
    CHK_RET(VectorReduceLoop(src1, src2, headCount, dataType, redOp, stream, dst));
    CHK_RET(TailVectorReduce(tailSrc1, tailSrc2, tailCount, dataType, redOp, stream, tailDst));

    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::SignalRecord(Stream &stream, u64 notifyId)
{
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::SignalRecord(stream, notifyId);
    }

    CHK_RET(GraphAddRecordTaskById(fftsPubInfo_, fftsCtxsPtr, static_cast<u32>(notifyId), stream.id()));

    if (HcclCheckLogLevel(HCCL_LOG_INFO) || (GetExternalInputDebugConfig() & PLF_TASK)) {
        PLF_CONFIG_INFO(PLF_TASK,
            "%s para: notifyId[0x%016llx] streamId[%u]", __func__, notifyId, stream.id());
    }

    return HCCL_SUCCESS;
}

HcclResult DispatcherGraph::SignalWait(Stream &stream, u32 notifyId, u32 timeOut)
{
    if (UNLIKELY(disableFfts_)) {
        return DispatcherPub::SignalWait(stream, notifyId, timeOut);
    }
    CHK_RET(GraphAddWaitTaskById(fftsPubInfo_, fftsCtxsPtr, static_cast<u32>(notifyId), stream.id()));

    if (HcclCheckLogLevel(HCCL_LOG_INFO) || (GetExternalInputDebugConfig() & PLF_TASK)) {
        PLF_CONFIG_INFO(PLF_TASK,
            "%s para: notifyId[0x%016llx] streamId[%u]", __func__, notifyId, stream.id());
    }

    return HCCL_SUCCESS;
}

} // namespace hccl
