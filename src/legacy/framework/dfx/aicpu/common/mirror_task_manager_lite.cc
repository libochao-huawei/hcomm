/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "mirror_task_manager_lite.h"

namespace Hccl {

MirrorTaskManagerLite::MirrorTaskManagerLite()
{
    currDfxOpInfo_ = std::make_shared<Hccl::DfxOpInfo>();
}

void MirrorTaskManagerLite::RegFullyCallBack(std::function<void(const std::string&, u32)> callBack)
{
    fullyNewCallBack_ = callBack;
    return;
}

void MirrorTaskManagerLite::RegFullyCallBack(std::function<void()> callBack)
{
    fullyCallBack_ = callBack;
    return;
}

void MirrorTaskManagerLite::AddTaskInfo(std::shared_ptr<TaskInfo> taskInfo)
{
    if (UNLIKELY(taskInfo == nullptr)) {
        THROW<InternalException>(
            StringFormat("MirrorTaskManagerLite::AddTaskInfo taskInfo is nullptr"));
    }

    auto queueIt = queueMap_.find(taskInfo->streamId_);
    if (UNLIKELY(queueIt == queueMap_.end())) {
        queueMap_[taskInfo->streamId_] = std::make_unique<CircularQueue<std::shared_ptr<TaskInfo>>>(MAX_CIRCULAR_QUEUE_LENGTH);
        queueIt = queueMap_.find(taskInfo->streamId_);
        queueTaskNum[taskInfo->streamId_] = 0;
    }

    auto taskNumIt = queueTaskNum.find(taskInfo->streamId_);
    auto &queue = queueIt->second;
    auto &taskNum = taskNumIt->second;
    if (UNLIKELY(taskNum == queue->Capacity())) {
        fullyCallBack_();
        taskNum = 0;
    }

    queue->Append(taskInfo);
    taskNum++;
    return;
}

HcclResult MirrorTaskManagerLite::SetCurrDfxOpInfo(std::shared_ptr<DfxOpInfo> dfxOpInfo)
{
    CHK_PTR_NULL(dfxOpInfo);
    currDfxOpInfo_ = dfxOpInfo;
    return HCCL_SUCCESS;
}

std::shared_ptr<DfxOpInfo> MirrorTaskManagerLite::GetCurrDfxOpInfo() const
{
    return currDfxOpInfo_;
}

TaskInfoQueue *MirrorTaskManagerLite::GetQueue(u32 streamId) const
{
    auto it = queueMap_.find(streamId);
    if (it == queueMap_.end()) {
        HCCL_ERROR("MirrorTaskManagerLite::GetQueue streamId(sqId)[%u] out of range", streamId);
        return nullptr;
    }
    return it->second.get();
}

std::shared_ptr<TaskInfo> MirrorTaskManagerLite::GetTaskInfo(u32 streamId, u32 taskId) const
{
    TaskInfoQueue *queue = nullptr;
    try {
        queue = GetQueue(streamId);
    } catch (HcclException &e) {
        HCCL_ERROR("Hccl exception %s was caught.", e.what());
        return nullptr;
    }

    auto FindTask = [taskId](const std::shared_ptr<TaskInfo> &taskInfo) {
        return taskInfo->taskId_ == taskId;
    };

    auto task = *queue->Find(FindTask);
    if (task == *queue->End()) {
        return nullptr;
    };

    HCCL_INFO("[MirrorTaskManagerLite][GetTaskInfo]find streamdId(sqId)[%u] taskId(sqeId)[%u]", streamId, taskId);

    return *task;
}

TaskInfoQueueMap::iterator MirrorTaskManagerLite::Begin() 
{
    return queueMap_.begin();
}

TaskInfoQueueMap::iterator MirrorTaskManagerLite::End()
{
    return queueMap_.end();
}

MirrorTaskManagerLite::~MirrorTaskManagerLite()
{
}

} // namespace Hccl