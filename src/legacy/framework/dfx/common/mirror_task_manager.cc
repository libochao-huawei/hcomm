/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "mirror_task_manager.h"

namespace Hccl {

MirrorTaskManager::MirrorTaskManager(u32 devId, GlobalMirrorTasks *globalMirrorTasks, bool devUsed)
    : devId_(devId), globalMirrorTasks_(globalMirrorTasks), devUsed_(devUsed)
{
}

void MirrorTaskManager::RegFullyCallBack(std::function<void()> callBack)
{
    fullyCallBack_ = callBack;
    return;
}

QueueType MirrorTaskManager::GetQueueType() const
{
    if(currDfxOpInfo_ == nullptr) {
        THROW<InternalException>(
            StringFormat("MirrorTaskManager::GetQueueType currDfxOpInfo_ is nullptr!"));
    }
    QueueType queueType = QueueType::Vector_Queue;

    if (devUsed_ || isStaticGraphMode_ || (opMode_ == OpMode::OPBASE)) {
        queueType = QueueType::Circular_Queue;
    }
    return queueType;
}

void MirrorTaskManager::AddTaskInfo(std::shared_ptr<TaskInfo> taskInfo)
{
    if (taskInfo == nullptr) {
        THROW<InternalException>(
            StringFormat("MirrorTaskManager::AddTaskInfo taskInfo is nullptr"));
    }

    if(taskInfo->dfxOpInfo_ == nullptr) {
        taskInfo->dfxOpInfo_ = currDfxOpInfo_;
    }

    if (queueMap_.find(taskInfo->streamId_) == queueMap_.end()) {
        QueueType queueType            = GetQueueType();
        queueMap_[taskInfo->streamId_] = &(globalMirrorTasks_->CreateQueue(devId_, taskInfo->streamId_, queueType));
    }

    if(queueMap_[taskInfo->streamId_]->IsFull()) {
        fullyCallBack_();
    }

    queueMap_[taskInfo->streamId_]->Append(taskInfo);

    HCCL_INFO("[MirrorTaskManager][AddTaskInfo]add devId[%u] streamId(sqId)[%u] taskId(sqeId)[%u]",
              devId_, taskInfo->streamId_, taskInfo->taskId_);

    return;
}

bool MirrorTaskManager::IsStaticGraphMode(const CollOperator &collOperator) const
{
    return (collOperator.staticAddr == false) && (collOperator.staticShape == false);
}

void MirrorTaskManager::SetCurrDfxOpInfo(std::shared_ptr<DfxOpInfo> dfxOpInfo)
{
    currDfxOpInfo_     = dfxOpInfo;
    isStaticGraphMode_ = IsStaticGraphMode(dfxOpInfo->op_);
    opMode_            = dfxOpInfo->op_.opMode;
    HCCL_INFO("[MirrorTaskManager][SetCurrDfxOpInfo] SetCurrDfxOpInfo Succeed!");
    return;
}

std::shared_ptr<DfxOpInfo> MirrorTaskManager::GetCurrDfxOpInfo() const
{
    return currDfxOpInfo_;
}

TaskInfoQueue *MirrorTaskManager::GetQueue(u32 streamId) const
{
    if (queueMap_.find(streamId) == queueMap_.end()) {
        THROW<InternalException>(StringFormat("MirrorTaskManager::GetQueue streamId(sqId)[%u] out of range", streamId));
    }
    return queueMap_.find(streamId)->second;
}

std::map<u32, TaskInfoQueue *>::iterator MirrorTaskManager::Begin()
{
    return queueMap_.begin();
}

std::map<u32, TaskInfoQueue *>::iterator MirrorTaskManager::End()
{
    return queueMap_.end();
}

MirrorTaskManager::~MirrorTaskManager()
{
}

} // namespace Hccl