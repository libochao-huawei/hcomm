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
    currDfxOpInfo_ = std::make_shared<Hccl::DfxOpInfo>();
}

void MirrorTaskManager::RegFullyCallBack(std::function<void(const std::string&, u32)> callBack)
{
    fullyNewCallBack_ = callBack;
    return;
}

void MirrorTaskManager::RegFullyCallBack(std::function<void()> callBack)
{
    fullyCallBack_ = callBack;
    return;
}

QueueType MirrorTaskManager::GetQueueType() const
{
    if (currDfxOpInfo_ == nullptr) {
        THROW<InternalException>(
            StringFormat("MirrorTaskManager::GetQueueType currDfxOpInfo_ is nullptr!"));
    }
    QueueType queueType = QueueType::Vector_Queue;

    if (devUsed_ || isStaticGraphMode_ || (opMode_ == OpMode::OPBASE)) {
        queueType = QueueType::Circular_Queue;
    }
    return queueType;
}

void MirrorTaskManager::AddTaskInfo(std::unique_ptr<TaskInfo> &taskInfo)
{
    if (UNLIKELY(taskInfo == nullptr)) {
        THROW<InternalException>(
            StringFormat("MirrorTaskManager::AddTaskInfo taskInfo is nullptr"));
    }
    bool needCallback = false;
    {
        std::unique_lock<std::mutex> lock(profMutex);
        if (taskInfo->dfxOpInfo_ == nullptr) {
            taskInfo->dfxOpInfo_ = currDfxOpInfo_;
        }

        auto queueIt = queueMap_.find(taskInfo->streamId_);
        if (UNLIKELY(queueIt == queueMap_.end())) {
            QueueType queueType = GetQueueType();
            queueMap_[taskInfo->streamId_] = &(globalMirrorTasks_->CreateQueue(devId_, taskInfo->streamId_, queueType));
            queueTaskNum[taskInfo->streamId_] = 0;
            queueIt = queueMap_.find(taskInfo->streamId_);
        }

        auto taskNumIt = queueTaskNum.find(taskInfo->streamId_);
        if (UNLIKELY(taskNumIt->second == queueIt->second->Capacity())) {
            needCallback = true;
            taskNumIt->second = 0;
        }

        if (needCallback && fullyCallBack_ != nullptr) {
            lock.unlock();
            fullyCallBack_();
            lock.lock();
            queueIt = queueMap_.find(taskInfo->streamId_);
            taskNumIt = queueTaskNum.find(taskInfo->streamId_);
        }
        queueIt->second->Append(taskInfo);
        taskNumIt->second++;
    }
    HCCL_INFO("[MirrorTaskManager][AddTaskInfo]add devId[%u] streamId(sqId)[%u] taskId(sqeId)[%u] queueMapsize[%u]",
        devId_, taskInfo->streamId_, taskInfo->taskId_, queueMap_.size());
    return;
}

bool MirrorTaskManager::IsStaticGraphMode(const CollOperator &collOperator) const
{
    return (collOperator.staticAddr == false) && (collOperator.staticShape == false);
}

void MirrorTaskManager::SetCurrDfxOpInfo(std::shared_ptr<DfxOpInfo> dfxOpInfo)
{
    if (dfxOpInfo == nullptr) {
        HCCL_ERROR("[MirrorTaskManager][SetCurrDfxOpInfo]fail, dfxOpInfo is nullptr");
        return;
    }
    currDfxOpInfo_     = dfxOpInfo;
    isStaticGraphMode_ = IsStaticGraphMode(dfxOpInfo->op_);
    opMode_            = dfxOpInfo->op_.opMode;
    HCCL_INFO("[MirrorTaskManager][SetCurrDfxOpInfo] Succeed, currDfxOpInfo_[%p], this[%p] !", currDfxOpInfo_.get(), this);
    return;
}

std::shared_ptr<DfxOpInfo> MirrorTaskManager::GetCurrDfxOpInfo() const
{
    HCCL_INFO("[MirrorTaskManager][GetCurrDfxOpInfo] Succeed, currDfxOpInfo_[%p], this[%p] !", currDfxOpInfo_.get(), this);
    return currDfxOpInfo_;
}

TaskInfoQueue *MirrorTaskManager::GetQueue(u32 streamId) const
{
    if (queueMap_.find(streamId) == queueMap_.end()) {
        THROW<InternalException>(StringFormat("MirrorTaskManager::GetQueue streamId(sqId)[%u] out of range", streamId));
    }
    return queueMap_.find(streamId)->second;
}

std::unordered_map<u32, TaskInfoQueue *>::iterator MirrorTaskManager::Begin()
{
    return queueMap_.begin();
}

std::unordered_map<u32, TaskInfoQueue *>::iterator MirrorTaskManager::End()
{
    return queueMap_.end();
}

MirrorTaskManager::~MirrorTaskManager()
{
}

} // namespace Hccl