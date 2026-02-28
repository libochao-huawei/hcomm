/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_NOTIFY_UTILS_H
#define HCOMM_NOTIFY_UTILS_H

#include "resource_entities.h"
#include "aicpu_ts_thread.h"
#include "new/hccl_primitive_local.h"
#include "exception_util.h"

using RequestServiceWithHandleFunc = int32_t(*)(ThreadHandle, ThreadServiceHandle, const void*, uint64_t);

// specify namespaces for the macro TRY_CATCH_*
using string = std::string;
using exception = std::exception;
using HcclException = Hccl::HcclException;

namespace hccl {

enum class NotifyRecordOpType {
    AicpuTs_to_AicpuTs,
    AicpuTs_to_Cpu,
    Cpu_to_AicpuTs,
    Others
};

NotifyRecordOpType GetNotifyRecordOpType(const ThreadEntity &srcEnt, const ThreadEntity &dstEnt) {
    if (srcEnt.engine == COMM_ENGINE_AICPU && srcEnt.type == THREAD_TYPE_TS &&
        dstEnt.engine == COMM_ENGINE_AICPU && dstEnt.type == THREAD_TYPE_TS) {
        return NotifyRecordOpType::AicpuTs_to_AicpuTs;
    } else if (srcEnt.engine == COMM_ENGINE_AICPU && srcEnt.type == THREAD_TYPE_TS &&
               dstEnt.engine == COMM_ENGINE_CPU && dstEnt.type == THREAD_TYPE_CPU) {
        return NotifyRecordOpType::AicpuTs_to_Cpu;
    } else if (srcEnt.engine == COMM_ENGINE_CPU && srcEnt.type == THREAD_TYPE_CPU &&
               dstEnt.engine == COMM_ENGINE_AICPU && dstEnt.type == THREAD_TYPE_TS) {
        return NotifyRecordOpType::Cpu_to_AicpuTs;
    }
    return NotifyRecordOpType::Others;
}

HcclResult RecordAicpuTsToAicpuTs(const ThreadEntity &srcEnt, const ThreadEntity &dstEnt, uint32_t dstNotifyIdx) {
    auto *const threadPtr = reinterpret_cast<AicpuTsThread *>(srcEnt.threadObjAddr);
    CHK_PTR_NULL(threadPtr);
    auto *const dstThreadPtr = reinterpret_cast<AicpuTsThread *>(dstEnt.threadObjAddr);
    CHK_PTR_NULL(dstThreadPtr);

    if (threadPtr->IsDeviceA5()) {
        LocalNotify *const notifyPtr = dstThreadPtr->GetNotify(dstNotifyIdx);
        CHK_PTR_NULL(notifyPtr);
        const uint32_t notifyId = notifyPtr->notifyId_;
        TRY_CATCH_RETURN(threadPtr->LocalNotifyRecord(notifyId));
    } else {
        Stream *stream = GetStream(srcEnt.threadObjAddr);
        CHK_PTR_NULL(stream);
        LocalNotify *notify = GetNotify(dstEnt.threadObjAddr, dstNotifyIdx);
        CHK_PTR_NULL(notify);
        CHK_RET(HcclLocalNotifyRecord(stream, notify));
    }
    return HCCL_SUCCESS;
}

HcclResult RecordAicpuTsToCpu(const ThreadEntity &srcEnt, const ThreadEntity &dstEnt, uint32_t dstNotifyIdx) {
    auto *const threadPtr = reinterpret_cast<AicpuTsThread *>(srcEnt.threadObjAddr);
    CHK_PTR_NULL(threadPtr);
    if (!threadPtr->IsDeviceA5()) {
        HCCL_ERROR("[%s] AICPU-TS -> CPU is NOT supported on A3.", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    if (dstNotifyIdx >= dstEnt.notifyNum) {
        HCCL_ERROR("[%s] dstNotifyIdx[%u] is out of range.", __func__, dstNotifyIdx);
        return HCCL_E_PARA;
    }
    const NotifyEntity dstNotifyEntity = dstEnt.notifies[dstNotifyIdx];
    CHK_RET(threadPtr->ThreadNotifyRecordCrossType(dstNotifyEntity));
    return HCCL_SUCCESS;
}

HcclResult RecordCpuToAicpuTs(const ThreadHandle srcHdl, const ThreadHandle dstHdl, uint32_t dstNotifyIdx, RequestServiceWithHandleFunc requestServiceFunc) {
    RecordServiceArgs tempRecordArgs = {
        .threadHandle = srcHdl,
        .dstThreadHandle = dstHdl,
        .dstNotifyIdx = dstNotifyIdx,
    };
    ThreadEntity *const srcEntPtr = reinterpret_cast<ThreadEntity *>(srcHdl); // nullptr checked outside
    CHK_RET(static_cast<HcclResult>(requestServiceFunc(srcHdl, srcEntPtr->cpuRes.recordService, &tempRecordArgs, sizeof(tempRecordArgs))));
    return HCCL_SUCCESS;
}

} // namespace hccl

#endif // HCOMM_NOTIFY_UTILS_H