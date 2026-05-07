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
constexpr uint32_t CTRL_HDR_FLAG_LENGTH    = 1;
constexpr uint32_t TASKTYPE_ADDR_LENGTH    = 256;
constexpr uint32_t CTRL_HDR_MSG_ID_LEN     = 4;
constexpr uint32_t CTRL_HDR_DATA_SIZE_LEN  = 8;
constexpr uint32_t CTRL_HDR_DEFAULT_DATA_LEN  = 512;

constexpr uint8_t  TASK_UNSET              = 0;
constexpr uint8_t  TASK_OK                 = 1;
constexpr uint8_t  TASK_TERMINATE          = 2;
constexpr uint8_t  TASK_TERMINATE_RESPONSE = 3;
constexpr uint8_t  MEMORY_DEVIDE           = 2;

TaskService::TaskService(void *deviceMem, int32_t deviceMemSize, void *hostMem, int32_t hostMemSize)
    : npu2dpuMem_(deviceMem), shmemSize_(deviceMemSize / MEMORY_DEVIDE), hostMem_(hostMem), hostMemSize_(hostMemSize)
{
    int32_t controlSize = sizeof(uint8_t) + sizeof(char) * TASKTYPE_ADDR_LENGTH + sizeof(uint32_t);
    if (shmemSize_ < controlSize) {
        dataSize_ = 0;
    } else {
        dataSize_ = shmemSize_ - controlSize;
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
    aclError ret = aclrtMemcpy(flagPtr, sizeof(newFlag), &newFlag, sizeof(newFlag), aclrtMemcpyKind::ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[TaskService::TaskRun] set flag failed: %d", ret);
        return HCCL_E_RUNTIME;
    }
    return HCCL_SUCCESS;
}

HcclResult TaskService::ReadFlag(uint8_t *ctrlHdr, uint64_t hdrLen, uint8_t *srcFlagPtr, uint8_t &flag) const
{
    aclError ret = aclrtMemcpy(ctrlHdr, hdrLen, npu2dpuMem_, hdrLen, aclrtMemcpyKind::ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[TaskService::%s] aclrtMemcpy failed on flag, return[%d].", __func__, ret);
        return HCCL_E_RUNTIME;
    }
    flag = *ctrlHdr;
    return HCCL_SUCCESS;
}

HcclResult TaskService::ReadTaskType(uint8_t *ctrlHdr, uint64_t hdrLen, uint8_t *srcTaskTypePtr, std::string &taskTypeStr) const
{
    CHK_PTR_NULL(srcTaskTypePtr);
    // 读 taskType
    char    *taskType = new char[TASKTYPE_ADDR_LENGTH];
    CHK_PTR_NULL(taskType);
    int ret = memcpy_s(taskType, (sizeof(char) * TASKTYPE_ADDR_LENGTH),
        ctrlHdr + CTRL_HDR_FLAG_LENGTH, (sizeof(char) * TASKTYPE_ADDR_LENGTH));
    if (ret < 0) {
        HCCL_ERROR("[%s] aclrtMemcpy failed on taskType, return[%d].", __func__, ret);
        delete[] taskType;
        return HCCL_E_RUNTIME;
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
    ret = memcpy_s(&msgId, sizeof(msgId),  ctrlHdr + CTRL_HDR_FLAG_LENGTH + TASKTYPE_ADDR_LENGTH, sizeof(msgId));
    if (ret < 0) {
        HCCL_ERROR("[%s] aclrtMemcpy failed on msgId, return[%d].", __func__, ret);
        return HCCL_E_RUNTIME;
    }
    HCCL_INFO("[TaskService::TaskRun] read msgId = %u.", msgId);
    return HCCL_SUCCESS;
}

HcclResult TaskService::ExecuteTask(uint8_t *ctrlHdr, uint64_t hdrLen, uint8_t *srcPtr, std::string taskTypeStr)
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

    // copy data
    uint32_t dataLen = *(size_t *)(ctrlHdr + CTRL_HDR_FLAG_LENGTH + TASKTYPE_ADDR_LENGTH + CTRL_HDR_MSG_ID_LEN);
    uint32_t ctrlHdrLen = CTRL_HDR_FLAG_LENGTH + TASKTYPE_ADDR_LENGTH + CTRL_HDR_MSG_ID_LEN + CTRL_HDR_DATA_SIZE_LEN;
    /* ctrlHdr提前从deviceMem copy一定长度，如果长度够，直接从ctrlHdr copy，减少一次aclmemcpy耗时 */
    if (hdrLen < ctrlHdrLen + dataLen) {
        uint8_t *dataPtr = srcPtr + ctrlHdrLen;
        aclError ret     = aclrtMemcpy(hostMem_, dataSize_, dataPtr, dataLen, aclrtMemcpyKind::ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != ACL_SUCCESS) {
            HCCL_ERROR("control data memcpy failed: %d", ret);
            return HCCL_E_RUNTIME;
        }
    } else {
        uint8_t *dataPtr = ctrlHdr + ctrlHdrLen;
        int ret = memcpy_s(hostMem_, dataSize_, dataPtr, dataLen);
        if (ret < 0) {
            HCCL_ERROR("control data memcpy failed: %d", ret);
            return HCCL_E_RUNTIME;
        }
    }
    if (itFunc->second(reinterpret_cast<uint64_t>(hostMem_), dataLen) != 0) {
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult TaskService::SynchronizeControlInfo(uint8_t *ctrlHdr, uint64_t hdrLen)
{
    CHK_PTR_NULL(npu2dpuMem_);
    CHK_PTR_NULL(dpu2npuMem_);
    // npu2dpu -> dpu2npu
    *ctrlHdr = 1;
    int32_t controlDataSize = sizeof(uint8_t) + sizeof(char) * TASKTYPE_ADDR_LENGTH + sizeof(uint32_t);
    HCCL_INFO("[TaskService::TaskRun] Send response: npu2dpu -> dpu2npu memcpy");
    aclError ret = aclrtMemcpy(dpu2npuMem_, controlDataSize, ctrlHdr, controlDataSize,
                               aclrtMemcpyKind::ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[TaskService::TaskRun] npu2dpu -> dpu2npu memcpy failed: %d", ret);
        return HCCL_E_RUNTIME;
    }
    return HCCL_SUCCESS;
}

HcclResult TaskService::ProcessTaskOk(uint8_t *ctrlHdr, uint64_t hdrLen, uint8_t *srcFlagPtr, uint8_t *srcTaskTypePtr)
{
    std::string taskTypeStr;
    HCCL_INFO("[TaskService::TaskRun] flag = %u.", TASK_OK);
    HCCL_INFO("[TaskService::TaskRun] Set npu2dpu flag -> %u.", TASK_UNSET);
    CHK_RET(WriteFlag(srcFlagPtr, TASK_UNSET));
    CHK_RET(ReadTaskType((uint8_t *)ctrlHdr, hdrLen, srcTaskTypePtr, taskTypeStr));
    CHK_RET(ExecuteTask((uint8_t *)ctrlHdr, hdrLen, srcFlagPtr, taskTypeStr));
    CHK_RET(SynchronizeControlInfo((uint8_t *)ctrlHdr, hdrLen));
    return HCCL_SUCCESS;
}

HcclResult TaskService::TaskRun()
{
    CHK_PTR_NULL(hostMem_);
    CHK_PTR_NULL(npu2dpuMem_);
    CHK_PTR_NULL(dpu2npuMem_);
    HCCL_INFO("[TaskService::%s] TaskService{npu2dpuMem:%p; dpu2npuMem:%p; hostMem:%p}", __func__, npu2dpuMem_,
              dpu2npuMem_, hostMem_);
    if (dataSize_ == 0) {
        HCCL_ERROR("[TaskService::%s] dataSize[%d] illegal", __func__, dataSize_);
        return HCCL_E_INTERNAL;
    }
    if (dataSize_ > hostMemSize_) {
        HCCL_ERROR("[TaskService::%s] hostMemSize[%d] less than dataSize[%d]", __func__, hostMemSize_, dataSize_);
        return HCCL_E_INTERNAL;
    }
    uint8_t flag{0};
    uint8_t *srcFlagPtr = static_cast<uint8_t *>(npu2dpuMem_);
    uint8_t *srcTaskTypePtr = srcFlagPtr + sizeof(flag);
    uint64_t hdrLen = CTRL_HDR_FLAG_LENGTH + TASKTYPE_ADDR_LENGTH + CTRL_HDR_MSG_ID_LEN + CTRL_HDR_DATA_SIZE_LEN +
        CTRL_HDR_DEFAULT_DATA_LEN;
    uint8_t ctrlHdr[hdrLen];

    CHK_RET(WriteFlag(srcFlagPtr, TASK_UNSET)); // 初始化重置flag 为 0

    while (true) {
        CHK_RET(ReadFlag((uint8_t *)ctrlHdr, hdrLen, srcFlagPtr, flag));
        switch (flag) {
            case TASK_UNSET:
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            case TASK_OK:
                CHK_RET(ProcessTaskOk((uint8_t *)ctrlHdr, hdrLen, srcFlagPtr, srcTaskTypePtr));
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