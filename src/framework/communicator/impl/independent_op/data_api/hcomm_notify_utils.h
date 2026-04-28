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
#include "hcomm_primitives.h"
#include "new/hccl_primitive_local.h"
#include "aicpu_ts_thread.h"
#include "exception_util.h"

// specify namespaces for the macro TRY_CATCH_*
using string = std::string;
using exception = std::exception;
using HcclException = Hccl::HcclException;

#ifdef __cplusplus
extern "C" {
#endif
int32_t HcommRequestServiceOnThread(ThreadHandle dstThreadHandle, ThreadServiceHandle serviceHandle,
    const void *args, uint64_t argsSizeByte);
#ifdef __cplusplus
}
#endif

namespace hccl {

enum class NotifyRecordOpType {
    AicpuTs_to_AicpuTs,
    AicpuTs_to_Cpu,
    Cpu_to_AicpuTs,
    Others
};

inline NotifyRecordOpType GetNotifyRecordOpType(const ThreadEntity &srcEnt, const ThreadEntity &dstEnt) {
    if (srcEnt.type == THREAD_TYPE_TS &&
        dstEnt.type == THREAD_TYPE_TS) {
        return NotifyRecordOpType::AicpuTs_to_AicpuTs;
    } else if (srcEnt.type == THREAD_TYPE_TS &&
               dstEnt.type == THREAD_TYPE_CPU) {
        return NotifyRecordOpType::AicpuTs_to_Cpu;
    } else if (srcEnt.type == THREAD_TYPE_CPU &&
               dstEnt.type == THREAD_TYPE_TS) {
        return NotifyRecordOpType::Cpu_to_AicpuTs;
    }
    return NotifyRecordOpType::Others;
}

inline HcclResult RecordAicpuTsToAicpuTs(const ThreadEntity &srcEnt, const ThreadEntity &dstEnt, uint32_t dstNotifyIdx) {
    auto *const threadPtr = reinterpret_cast<AicpuTsThread *>(srcEnt.threadObjAddr);
    CHK_PTR_NULL(threadPtr);
    auto *const dstThreadPtr = reinterpret_cast<AicpuTsThread *>(dstEnt.threadObjAddr);
    CHK_PTR_NULL(dstThreadPtr);

    if (dstNotifyIdx >= dstEnt.notifyNum) {
        HCCL_ERROR("[%s] dstNotifyIdx[%u] is out of range, notifyNum[%u].", __func__, dstNotifyIdx, dstEnt.notifyNum);
        return HCCL_E_PARA;
    }

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

inline HcclResult RecordAicpuTsToCpu(const ThreadEntity &srcEnt, const ThreadEntity &dstEnt, uint32_t dstNotifyIdx) {
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
    CHK_RET(threadPtr->NotifyRecord(dstNotifyEntity));
    return HCCL_SUCCESS;
}

inline HcclResult RecordCpuToAicpuTs(const ThreadEntity &srcEnt, const ThreadEntity &dstEnt, uint32_t dstNotifyIdx) {
    auto *const dstThreadPtr = reinterpret_cast<AicpuTsThread *>(dstEnt.threadObjAddr);
    CHK_PTR_NULL(dstThreadPtr);
    if (!dstThreadPtr->IsDeviceA5()) {
        HCCL_ERROR("[%s] CPU -> AICPU-TS is NOT supported on non-A5 device.", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    const ThreadHandle srcHdl = reinterpret_cast<ThreadHandle>(&srcEnt);
    const ThreadHandle dstHdl = reinterpret_cast<ThreadHandle>(&dstEnt);
    RecordServiceArgs tempRecordArgs = {
        .threadHandle = dstHdl,
        .dstThreadHandle = dstHdl,
        .dstNotifyIdx = dstNotifyIdx,
    };
    CHK_RET(static_cast<HcclResult>(HcommRequestServiceOnThread(srcHdl, srcEnt.cpuRes.recordService, &tempRecordArgs, sizeof(tempRecordArgs))));
    return HCCL_SUCCESS;
}

inline HcclResult WaitAicpuTs(const ThreadEntity &ent, uint32_t dstNotifyIdx, uint32_t timeOut) {
    auto *const threadPtr = reinterpret_cast<AicpuTsThread *>(ent.threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    if (threadPtr->IsDeviceA5()) {
        LocalNotify *const notifyPtr = threadPtr->GetNotify(dstNotifyIdx);
        CHK_PTR_NULL(notifyPtr);
        const uint32_t notifyId = notifyPtr->notifyId_;
        TRY_CATCH_RETURN(threadPtr->LocalNotifyWait(notifyId));
    } else {
        Stream *stream = GetStream(ent.threadObjAddr);
        CHK_PTR_NULL(stream);
        LocalNotify *notify = GetNotify(ent.threadObjAddr, dstNotifyIdx);
        CHK_PTR_NULL(notify);
        CHK_RET(HcclLocalNotifyWait(stream, notify, timeOut));
    }
    return HCCL_SUCCESS;
}

inline HcclResult WaitCpu(const ThreadEntity &ent, uint32_t notifyIdx, uint32_t timeOut) {
    const ThreadHandle hdl = reinterpret_cast<ThreadHandle>(&ent);
    WaitServiceArgs tempWaitArgs = {
        .threadHandle = hdl,
        .notifyIdx = notifyIdx,
    };
    (void)timeOut;
    CHK_RET(static_cast<HcclResult>(HcommRequestServiceOnThread(hdl, ent.cpuRes.waitService, &tempWaitArgs, sizeof(tempWaitArgs))));
    return HCCL_SUCCESS;
}

} // namespace hccl

#endif // HCOMM_NOTIFY_UTILS_H
