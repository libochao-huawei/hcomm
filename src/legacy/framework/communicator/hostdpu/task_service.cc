/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "task_service.h"
#include <algorithm>
#include "acl/acl_rt.h"
#include "log.h"
#include <thread>
#include <chrono>

namespace Hccl {
constexpr uint32_t TASKTYPE_ADDR_LENGTH    = 256;
constexpr uint8_t  TASK_UNSET              = 0;
constexpr uint8_t  TASK_OK                 = 1;
constexpr uint8_t  TASK_TERMINATE          = 2;
constexpr uint8_t  TASK_TERMINATE_RESPONSE = 3;
constexpr uint8_t  MEMORY_DEVIDE           = 2;

TaskService::TaskService(void *deviceMem, int32_t deviceMemSize, void *hostMem, int32_t hostMemSize)
    : npu2dpuMem_(deviceMem), shmemSize_(deviceMemSize / MEMORY_DEVIDE), hostMem_(hostMem), hostMemSize_(hostMemSize)
{
    int32_t controlSize = sizeof(uint8_t) + sizeof(char) * TASKTYPE_ADDR_LENGTH + sizeof(uint32_t) + sizeof(uint32_t);
    if (shmemSize_ < controlSize) {
        hostSize_ = 0;
    } else {
        hostSize_ = shmemSize_ - controlSize;
    }
    dpu2npuMem_ = static_cast<uint8_t *>(npu2dpuMem_) + shmemSize_;
}

HcclResult TaskService::TaskRegister(std::string taskType, CallbackTemplate callback)
{
    HCCL_INFO("[TaskService::%s] taskType[%s]", __func__, taskType.c_str());
    callbacks_.insert({taskType, callback});
    return HCCL_SUCCESS;
}

HcclResult TaskService::TaskUnRegister(std::string taskType)
{
    HCCL_INFO("[TaskService::%s] taskType[%s]", __func__, taskType.c_str());
    if (callbacks_.find(taskType) == callbacks_.end()) {
        HCCL_WARNING("[TaskService::%s] TaskType Not Found", __func__);
        return HCCL_E_NOT_FOUND;
    }
    callbacks_.erase(taskType);
    return HCCL_SUCCESS;
}

HcclResult TaskService::WriteFlag(uint8_t *flagPtr, uint8_t newFlag) const
{
    errno_t ret = memcpy_s(flagPtr, sizeof(newFlag), &newFlag, sizeof(newFlag));
    if (ret != EOK) {
        HCCL_ERROR("[TaskService::TaskRun] set flag failed: %d", ret);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult TaskService::ReadFlag(uint8_t *srcFlagPtr, uint8_t &flag) const
{
    CHK_PTR_NULL(srcFlagPtr);
    errno_t ret = memcpy_s(&flag, sizeof(flag), srcFlagPtr, sizeof(flag));
    if (ret != EOK) {
        HCCL_ERROR("[TaskService::%s] memcpy_s failed on flag, return[%d].", __func__, ret);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult TaskService::ReadTaskType(uint8_t *srcTaskTypePtr, std::string &taskTypeStr) const
{
    CHK_PTR_NULL(srcTaskTypePtr);
    // 读 taskType
    char    *taskType = new char[TASKTYPE_ADDR_LENGTH];
    CHK_PTR_NULL(taskType);
    errno_t ret      = memcpy_s(taskType, (sizeof(char) * TASKTYPE_ADDR_LENGTH), srcTaskTypePtr,
                                    (sizeof(char) * TASKTYPE_ADDR_LENGTH));
    if (ret != EOK) {
        HCCL_ERROR("[%s] memcpy_s failed on taskType, return[%d].", __func__, ret);
        delete[] taskType;
        return HCCL_E_INTERNAL;
    }
    //  查找 \0
    auto it = std::find(taskType, taskType + TASKTYPE_ADDR_LENGTH, '\0');
    if (it == taskType + TASKTYPE_ADDR_LENGTH) {
        HCCL_ERROR("[TaskService::TaskRun] No Null Character Found Within TaskType Max Length");
        delete[] taskType;
        return HCCL_E_PARA;
    }
    taskTypeStr = std::string(taskType);
    delete[] taskType;
    HCCL_INFO("[TaskService::TaskRun] read taskType = %s", taskTypeStr.c_str());
 
    uint32_t msgId{0};
    ret = memcpy_s(&msgId, sizeof(msgId), srcTaskTypePtr + TASKTYPE_ADDR_LENGTH, sizeof(msgId));
    if (ret != EOK) {
        HCCL_ERROR("[%s] memcpy_s failed on msgId, return[%d].", __func__, ret);
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[TaskService::TaskRun] read msgId = %u.", msgId);
    return HCCL_SUCCESS;
}

HcclResult TaskService::ExecuteTask(uint8_t *srcPtr, std::string taskTypeStr)
{
    auto itFunc = callbacks_.find(taskTypeStr);
    if (itFunc == callbacks_.end()) {
        HCCL_ERROR("[TaskService::TaskRun] Callback of taskType[%s] Not Found", taskTypeStr.c_str());
        std::string taskTypeMsg{"map{"};
        for (const auto &pair : callbacks_) {
            taskTypeMsg += pair.first;
        }
        taskTypeMsg += "}";
        HCCL_ERROR("[TaskService::TaskRun] Callback key : %s", taskTypeMsg.c_str());
        return HCCL_E_NOT_FOUND;
    }

    // copy dataSize
    uint32_t dataSize{0};
    uint8_t *dataSizePtr = srcPtr + sizeof(uint8_t) + (sizeof(char) * TASKTYPE_ADDR_LENGTH) + sizeof(uint32_t);
    errno_t ret = memcpy_s(&dataSize, sizeof(dataSize), dataSizePtr, sizeof(dataSize));
    if (ret != EOK) {
        HCCL_ERROR("[TaskService::%s] dataSize memcpy failed: %d", __func__, ret);
        return HCCL_E_INTERNAL;
    }

    if (hostSize_ < 0 || dataSize > static_cast<uint32_t>(hostSize_)) {
        HCCL_ERROR("[TaskService::%s] dataSize[%u] larger than hostMemSize[%d]", __func__, dataSize, hostMemSize_);
        return HCCL_E_INTERNAL;
    }

    // copy data
    uint8_t *dataPtr = dataSizePtr + sizeof(uint32_t);
    ret     = memcpy_s(hostMem_, dataSize, dataPtr, dataSize);
    if (ret != EOK) {
        HCCL_ERROR("control data memcpy failed: %d", ret);
        return HCCL_E_INTERNAL;
    }

    if (itFunc->second(reinterpret_cast<uint64_t>(hostMem_), dataSize) != 0) {
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult TaskService::SynchronizeControlInfo()
{
    CHK_PTR_NULL(npu2dpuMem_);
    CHK_PTR_NULL(dpu2npuMem_);
    // npu2dpu -> dpu2npu
    int32_t controlDataSize = sizeof(uint8_t) + sizeof(char) * TASKTYPE_ADDR_LENGTH + sizeof(uint32_t);
    HCCL_INFO("[TaskService::TaskRun] Send response: npu2dpu -> dpu2npu memcpy");
    errno_t ret = memcpy_s(dpu2npuMem_, controlDataSize, npu2dpuMem_, controlDataSize);
    if (ret != EOK) {
        HCCL_ERROR("[TaskService::TaskRun] npu2dpu -> dpu2npu memcpy failed: %d", ret);
        return HCCL_E_INTERNAL;
    }

    uint8_t newFlag = 1;
    HCCL_INFO("[TaskService::TaskRun] Send response: Set dpu2npu flag -> 1");
    ret = memcpy_s(dpu2npuMem_, sizeof(newFlag), &newFlag, sizeof(newFlag));
    if (ret != EOK) {
        HCCL_ERROR("[TaskService::TaskRun] set flag failed: %d", ret);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult TaskService::TaskRun()
{
    CHK_PTR_NULL(hostMem_);
    CHK_PTR_NULL(npu2dpuMem_);
    CHK_PTR_NULL(dpu2npuMem_);
    HCCL_INFO("[TaskService::%s] TaskService{npu2dpuMem:%p; dpu2npuMem:%p; hostMem:%p}", __func__, npu2dpuMem_,
              dpu2npuMem_, hostMem_);
    if (hostSize_ == 0) {
        HCCL_ERROR("[TaskService::%s] dataSize[%d] illegal", __func__, hostSize_);
        return HCCL_E_INTERNAL;
    }
    if (hostSize_ > hostMemSize_) {
        HCCL_ERROR("[TaskService::%s] hostMemSize[%d] less than dataSize[%d]", __func__, hostMemSize_, hostSize_);
        return HCCL_E_INTERNAL;
    }
    uint8_t flag{0};
    uint8_t *srcFlagPtr = static_cast<uint8_t *>(npu2dpuMem_);
    uint8_t *srcTaskTypePtr = srcFlagPtr + sizeof(flag);
    std::string taskTypeStr;

    CHK_RET(WriteFlag(srcFlagPtr, TASK_UNSET)); // 初始化重置flag 为 0

    while (true) {
        CHK_RET(ReadFlag(srcFlagPtr, flag));
        switch (flag) {
            case TASK_UNSET:
                continue;
            case TASK_OK:
                HCCL_INFO("[TaskService::TaskRun] flag = %u.", flag);
                HCCL_INFO("[TaskService::TaskRun] Set npu2dpu flag -> %u.", TASK_UNSET);
                CHK_RET(WriteFlag(srcFlagPtr, TASK_UNSET));
                CHK_RET(ReadTaskType(srcTaskTypePtr, taskTypeStr));
                CHK_RET(ExecuteTask(srcFlagPtr, taskTypeStr));
                CHK_RET(SynchronizeControlInfo());
                continue;
            case TASK_TERMINATE:
                HCCL_INFO("[TaskService::TaskRun] flag = %u.", flag);
                HCCL_INFO("[TaskService::TaskRun] Set npu2dpu flag -> %u. Task Run END.", TASK_TERMINATE_RESPONSE);
                CHK_RET(WriteFlag(srcFlagPtr, TASK_TERMINATE_RESPONSE));
                HCCL_INFO("[TaskService::TaskRun] Exiting.");
                return HCCL_SUCCESS;
            case TASK_TERMINATE_RESPONSE:
                HCCL_INFO("[TaskService::TaskRun] flag = %u. Exiting.", flag);
                return HCCL_SUCCESS;
            default:
                HCCL_INFO("[TaskService::TaskRun] flag = %u. Unimplemented flag. Exiting.", flag);
                return HCCL_SUCCESS;
        }
    }
    return HCCL_SUCCESS;
}
} // namespace Hccl