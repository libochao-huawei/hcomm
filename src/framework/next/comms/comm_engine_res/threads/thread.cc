/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "thread.h"
#include "cpu_ts_thread.h"
#include "aicpu_ts_thread.h"
#include "sal_pub.h"
#include "stream_lite.h"
#include "task_info.h"

namespace hccl {

HcclResult CreateThread(CommEngine engine, StreamType streamType,
    uint32_t notifyNum, NotifyLoadType loadType, std::shared_ptr<Thread>& out_thread)  
{
    out_thread = nullptr;  // 初始化出参
 
    if (engine == COMM_ENGINE_CPU_TS || engine == COMM_ENGINE_CPU
        || engine == COMM_ENGINE_CCU) {
        EXECEPTION_CATCH(out_thread = std::make_shared<CpuTsThread>(streamType, notifyNum, loadType), return HCCL_E_PTR);
    } else if (engine == COMM_ENGINE_AICPU_TS || engine == COMM_ENGINE_AICPU) {
        EXECEPTION_CATCH(out_thread = std::make_shared<AicpuTsThread>(streamType, notifyNum, loadType), return HCCL_E_PTR);
    } else {
        return HCCL_E_NOT_SUPPORT;
    }
 
    return HCCL_SUCCESS;
}

HcclResult CommHostEngineToNotifyLoadType(CommEngine engine, NotifyLoadType &type)
{
    switch (engine) {
        case COMM_ENGINE_CPU:
        case COMM_ENGINE_CPU_TS:
        case COMM_ENGINE_CCU:
            type =  NotifyLoadType::HOST_NOTIFY;
            break;
        default:
            HCCL_ERROR("[ThreadMgr] Unsupported comm engine type: %d", engine);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult CommEngineToNotifyLoadType(CommEngine engine, NotifyLoadType &type)
{
    switch (engine) {
        case COMM_ENGINE_CPU:
        case COMM_ENGINE_CPU_TS:
        case COMM_ENGINE_CCU:
            type =  NotifyLoadType::HOST_NOTIFY;
            break;
        case COMM_ENGINE_AICPU:
        case COMM_ENGINE_AICPU_TS:
            type =  NotifyLoadType::DEVICE_NOTIFY;
            break;
        default:
            HCCL_ERROR("[ThreadMgr] Unknown comm engine type: %d", engine);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult CommEngineToStreamType(CommEngine engine, StreamType &type)
{
    switch (engine) {
        case COMM_ENGINE_CPU:
        case COMM_ENGINE_CPU_TS:
        case COMM_ENGINE_CCU:
            type = StreamType::STREAM_TYPE_ONLINE; // 单算子使用online，图模式使用offine
            break;
        case COMM_ENGINE_AICPU:
        case COMM_ENGINE_AICPU_TS:
            type = StreamType::STREAM_TYPE_DEVICE;
            break;
        // 暂不支持AIV
        case COMM_ENGINE_AIV:
        default:
            HCCL_ERROR("[ThreadMgr] Unknown comm engine type: %d", engine);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult Thread::AddThreadHandleToMap(CommEngine commEngine, ThreadHandle threadHandle)
{
    if (threadHandleMap_.find(commEngine) != threadHandleMap_.end() && threadHandleMap_[commEngine] != threadHandle) {
        HCCL_ERROR("[Thread][%s]Mapping already exists:commEngine[%d], threadHandle[%lu], new threadHandle[%lu]",
                   __func__, threadHandleMap_[commEngine], threadHandle);
    }

    threadHandleMap_[commEngine] = threadHandle;
    return HCCL_SUCCESS;
}

Thread *Thread::FindThreadByCommEngine(CommEngine commEngine)
{
    if (threadHandleMap_.find(commEngine) != threadHandleMap_.end()) {
        return reinterpret_cast<Thread *>(threadHandleMap_[commEngine]);
    }

    return nullptr;
}

HcclResult Thread::ReportNotifyWaitTask(u64 notifyId) const
{
    Hccl::TaskParam taskParam{};
    taskParam.taskType                 = Hccl::TaskParamType::TASK_NOTIFY_WAIT;
    taskParam.beginTime                = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Notify.notifyID = notifyId;
    taskParam.taskPara.Notify.value    = 1;

    Hccl::StreamLite* streamLite = static_cast<Hccl::StreamLite *>(GetStreamLitePtr());
    CHK_PTR_NULL(streamLite);
    u32 streamId = streamLite->GetId();

    Hccl::RtsqBase* rtsqBase = streamLite->GetRtsq();
    CHK_PTR_NULL(rtsqBase);

    u32 taskId = rtsqBase->GetTaskId();
    CHK_PTR_NULL(callback_);
    CHK_RET(callback_(streamId, taskId, taskParam, INVALID_U64));
    HCCL_INFO("[Thread][%s] streamId[%u], taskId[%u], notifyId[%llu], %s", __func__, streamId, taskId,
        notifyId, taskParam.Describe().c_str());
    return HCCL_SUCCESS;
}

HcclResult Thread::ReportNotifyRecordTask(u64 notifyId) const
{
    Hccl::TaskParam taskParam{};
    taskParam.taskType                 = Hccl::TaskParamType::TASK_NOTIFY_RECORD;
    taskParam.beginTime                = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Notify.notifyID = notifyId;
    taskParam.taskPara.Notify.value    = 1;

    Hccl::StreamLite* streamLite = static_cast<Hccl::StreamLite *>(GetStreamLitePtr());
    CHK_PTR_NULL(streamLite);
    u32 streamId = streamLite->GetId();

    Hccl::RtsqBase* rtsqBase = streamLite->GetRtsq();
    CHK_PTR_NULL(rtsqBase);
    u32 taskId = rtsqBase->GetTaskId();

    CHK_PTR_NULL(callback_);
    CHK_RET(callback_(streamId, taskId, taskParam, INVALID_U64));
    HCCL_INFO("[Thread][%s] streamId[%u], taskId[%u], notifyId[%llu], %s", __func__, streamId, taskId,
        notifyId, taskParam.Describe().c_str());
    return HCCL_SUCCESS;
}

HcclResult Thread::ReportLocalCopyTask(void *dst, const void *src, uint64_t sizeByte) const
{
    Hccl::TaskParam taskParam{};
    taskParam.taskType              = Hccl::TaskParamType::TASK_SDMA;
    taskParam.beginTime             = ProfGetCurCpuTimestamp();
    taskParam.taskPara.DMA.src      = src;
    taskParam.taskPara.DMA.dst      = dst;
    taskParam.taskPara.DMA.size     = sizeByte;
    taskParam.taskPara.DMA.notifyID = INVALID_U64;
    taskParam.taskPara.DMA.linkType = Hccl::DfxLinkType::ONCHIP;
    taskParam.taskPara.DMA.dmaOp    = Hccl::DmaOp::HCCL_DMA_READ;

    Hccl::StreamLite* streamLite = static_cast<Hccl::StreamLite *>(GetStreamLitePtr());
    CHK_PTR_NULL(streamLite);
    u32 streamId = streamLite->GetId();

    Hccl::RtsqBase* rtsqBase = streamLite->GetRtsq();
    CHK_PTR_NULL(rtsqBase);
    u32 taskId = rtsqBase->GetTaskId();

    CHK_RET(callback_(streamId, taskId, taskParam, INVALID_U64));
    HCCL_INFO("[Thread][%s] streamId[%u], taskId[%u], src[%p], dst[%p], len[%llu] %s", __func__, streamId, taskId,
        src, dst, sizeByte, taskParam.Describe().c_str());
    return HCCL_SUCCESS;
}

HcclResult Thread::ReportLocalReduceTask(void *dst, const void *src, uint64_t sizeByte, HcommDataType dataType,
    HcommReduceOp reduceOp) const
{
    Hccl::TaskParam taskParam{};
    taskParam.taskType = Hccl::TaskParamType::TASK_REDUCE_INLINE;
    taskParam.beginTime = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Reduce.src = src;
    taskParam.taskPara.Reduce.dst = dst;
    taskParam.taskPara.Reduce.size = sizeByte;
    taskParam.taskPara.Reduce.notifyID = INVALID_U64;
    taskParam.taskPara.Reduce.linkType = Hccl::DfxLinkType::ONCHIP;
    taskParam.taskPara.Reduce.dataType = static_cast<HcclDataType>(dataType);
    taskParam.taskPara.Reduce.reduceOp = static_cast<HcclReduceOp>(reduceOp);

    Hccl::StreamLite* streamLite = static_cast<Hccl::StreamLite *>(GetStreamLitePtr());
    CHK_PTR_NULL(streamLite);
    u32 streamId = streamLite->GetId();

    Hccl::RtsqBase* rtsqBase = streamLite->GetRtsq();
    CHK_PTR_NULL(rtsqBase);
    u32 taskId = rtsqBase->GetTaskId();
    CHK_RET(callback_(streamId, taskId, taskParam, INVALID_U64));
    HCCL_INFO("[Thread][%s] streamId[%u], taskId[%u], src[%p], dst[%p], len[%llu], dataType[%d], reduceOp[%d], %s",
        __func__, streamId, taskId, src, dst, sizeByte, dataType, reduceOp, taskParam.Describe().c_str());
    return HCCL_SUCCESS;
}

}  // namespace hccl