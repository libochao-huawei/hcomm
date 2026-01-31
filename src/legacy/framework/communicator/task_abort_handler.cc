/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "task_abort_handler.h"
#include <algorithm>
#include "pthread.h"
#include "log.h"

namespace Hccl {
using HcclUs = std::chrono::steady_clock::time_point;
static std::mutex vecMutex;

int32_t ProcessTaskAbortHandleCallback(uint32_t deviceLogicId, rtDeviceTaskAbortStage stage, uint32_t timeout, void *args)
{
    HcclUs startut = std::chrono::steady_clock::now();
    CHK_PTR_NULL(args);
    auto &commVector = *(static_cast<std::vector<HcclCommunicator *> *>(args));
    HCCL_INFO("[NsRecovery][Callback] ProcessTaskAbortHandleCallback begin, deviceLogicId [%u], stage [%d], commVector "
              "size [%lu]",
              deviceLogicId, stage, commVector.size());
    const std::chrono::seconds localtimeout = std::chrono::seconds(timeout);
    HcclResult                 ret          = HCCL_SUCCESS;
    if (localtimeout != std::chrono::seconds(0)) {
        if (stage == RT_DEVICE_TASK_ABORT_PRE) {
            for (size_t i = 0; i < commVector.size(); i++) {
                std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
                ret                                             = commVector[i]->Suspend();
                std::chrono::steady_clock::time_point curTime   = std::chrono::steady_clock::now();
                if (ret != HCCL_SUCCESS && ret != HCCL_E_SUSPENDING) {
                    HCCL_ERROR("[NsRecovery][Callback] finish suspend failed");
                    return static_cast<int>(TaskAbortResult::TASK_ABORT_FAIL);
                }
                HCCL_INFO("[NsRecovery][Callback] finish suspend success");
                const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(curTime - startTime);
                CHK_PRT_RET(elapsed > localtimeout, HCCL_ERROR("[NsRecovery][Callback] NsRecovery suspend timeout"),
                            static_cast<int>(TaskAbortResult::TASK_ABORT_TIMEOUT));
            }
        } else if (stage == RT_DEVICE_TASK_ABORT_POST) {
            CHK_RET(HcclCcuTaskKillPreProcess(deviceLogicId));
            for (size_t i = 0; i < commVector.size(); i++) {
                std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
                ret                                             = commVector[i]->Clean();
                std::chrono::steady_clock::time_point curTime   = std::chrono::steady_clock::now();
                if (ret != HCCL_SUCCESS && ret != HCCL_E_SUSPENDING) {
                    HCCL_ERROR("[NsRecovery][Callback] finish clean failed");
                    return static_cast<int>(TaskAbortResult::TASK_ABORT_FAIL);
                }
                HCCL_INFO("[NsRecovery][Callback] finish clean success");
                const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(curTime - startTime);
                CHK_PRT_RET(elapsed > localtimeout, HCCL_ERROR("[NsRecovery][Callback] NsRecovery Clean timeout"),
                            static_cast<int>(TaskAbortResult::TASK_ABORT_TIMEOUT));
            }
            CHK_RET(HcclCcuTaskKillPostProcess(deviceLogicId));
        }
    } else {
        if (stage == RT_DEVICE_TASK_ABORT_PRE) {
            for (size_t i = 0; i < commVector.size(); i++) {
                ret = commVector[i]->Suspend();
                if (ret != HCCL_SUCCESS && ret != HCCL_E_SUSPENDING) {
                    HCCL_ERROR("[NsRecovery][Callback] finish suspend failed");
                    return static_cast<int>(TaskAbortResult::TASK_ABORT_FAIL);
                }
                HCCL_INFO("[NsRecovery][Callback] finish suspend success");
            }
        } else if (stage == RT_DEVICE_TASK_ABORT_POST) {
            CHK_RET(HcclCcuTaskKillPreProcess(deviceLogicId));
            for (size_t i = 0; i < commVector.size(); i++) {
                ret = commVector[i]->Clean();
                if (ret != HCCL_SUCCESS && ret != HCCL_E_SUSPENDING) {
                    HCCL_ERROR("[NsRecovery][Callback] finish clean failed");
                    return static_cast<int>(TaskAbortResult::TASK_ABORT_FAIL);
                }
                HCCL_INFO("[NsRecovery][Callback] finish clean success");
            }
            CHK_RET(HcclCcuTaskKillPostProcess(deviceLogicId));
        }
    }

    HcclUs endut = std::chrono::steady_clock::now();
    HCCL_INFO("[NsRecovery][Callback] ProcessTaskAbortHandleCallback success, take time:[%lld]us",
              std::chrono::duration_cast<std::chrono::microseconds>(endut - startut).count());
    return static_cast<int>(TaskAbortResult::TASK_ABORT_SUCCESS);
}

TaskAbortHandler::TaskAbortHandler()
{
    HrtDeviceAbortRegCallBack(ProcessTaskAbortHandleCallback, static_cast<void *>(&commVector));
}

TaskAbortHandler::~TaskAbortHandler()
{
    DECTOR_TRY_CATCH("TaskAbortHandler", HrtDeviceAbortRegCallBack(nullptr, nullptr));
}

TaskAbortHandler &TaskAbortHandler::GetInstance()
{
    static TaskAbortHandler handler;
    return handler;
}

HcclResult TaskAbortHandler::Register(HcclCommunicator *communicator)
{
    std::lock_guard<std::mutex> lock(vecMutex);
    commVector.push_back(communicator);
    HCCL_INFO("TaskAbortHandler::Register success, commVector size is [%lu]", commVector.size());

    return HCCL_SUCCESS;
}

HcclResult TaskAbortHandler::UnRegister(HcclCommunicator *communicator)
{
    std::lock_guard<std::mutex> lock(vecMutex);
    HCCL_INFO("TaskAbortHandler::UnRegister Begin, commVector size is [%lu]", commVector.size());
    auto it = std::find(commVector.begin(), commVector.end(), communicator);
    if (it != commVector.end()) {
        commVector.erase(it);
    } else {
        HCCL_WARNING("TaskAbortHandler::UnRegister, comm not found.");
    }
    HCCL_INFO("TaskAbortHandler::UnRegister finish, commVector size is [%lu]", commVector.size());
    return HCCL_SUCCESS;
}
}