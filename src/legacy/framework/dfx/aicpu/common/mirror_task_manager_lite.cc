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
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::MirrorTaskManagerLite] this[%p], currDfxOpInfo[%p]",
        this, currDfxOpInfo_.get());
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
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] begin, this[%p], taskInfo[%p], "
        "currDfxOpInfo[%p]", this, taskInfo.get(), currDfxOpInfo_.get());
    HCCL_INFO("[MirrorTaskManagerLite][AddTaskInfo]AddTaskInfo begin");
    if (UNLIKELY(taskInfo == nullptr)) {
        THROW<InternalException>(
            StringFormat("MirrorTaskManagerLite::AddTaskInfo taskInfo is nullptr"));
    }

    if (taskInfo->dfxOpInfo_ == nullptr) {
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] task dfxOpInfo null, use currDfxOpInfo[%p]",
            currDfxOpInfo_.get());
        taskInfo->dfxOpInfo_ = currDfxOpInfo_;
    }

    if (queueMap_.find(taskInfo->streamId_) == queueMap_.end()) {
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] create queue, streamId[%u]",
            taskInfo->streamId_);
        queueMap_[taskInfo->streamId_] = std::make_unique<CircularQueue<std::shared_ptr<TaskInfo>>>(MAX_CIRCULAR_QUEUE_LENGTH);
        queueTaskNum[taskInfo->streamId_] = 0;
    }

    if (queueTaskNum[taskInfo->streamId_] == static_cast<u32>(queueMap_[taskInfo->streamId_]->Capacity())) {
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] queue full callback, streamId[%u], "
            "queueTaskNum[%u]", taskInfo->streamId_, queueTaskNum[taskInfo->streamId_]);
        fullyCallBack_();
        queueTaskNum[taskInfo->streamId_] = 0;
    }

    queueMap_[taskInfo->streamId_]->Append(taskInfo);
    queueTaskNum[taskInfo->streamId_]++;

    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] end, streamId(sqId)[%u], "
        "taskId(sqeId)[%u], queueMapSize[%llu], queueTaskNum[%u], dfxOpInfo[%p]", taskInfo->streamId_,
        taskInfo->taskId_, static_cast<unsigned long long>(queueMap_.size()), queueTaskNum[taskInfo->streamId_],
        taskInfo->dfxOpInfo_.get());
    HCCL_INFO("[MirrorTaskManagerLite][AddTaskInfo]add streamId(sqId)[%u] taskId(sqeId)[%u] queueMapsize[%u]", taskInfo->streamId_, taskInfo->taskId_, queueMap_.size());

    return;
}

bool MirrorTaskManagerLite::IsStaticGraphMode(const CollOperator &collOperator) const
{
    return (collOperator.staticAddr == false) && (collOperator.staticShape == false);
}

void MirrorTaskManagerLite::SetCurrDfxOpInfo(std::shared_ptr<DfxOpInfo> dfxOpInfo)
{
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::SetCurrDfxOpInfo] start, this[%p], oldDfxOpInfo[%p], "
        "newDfxOpInfo[%p]", this, currDfxOpInfo_.get(), dfxOpInfo.get());
    if (dfxOpInfo == nullptr) {
        HCCL_ERROR("YYYYYY hcomm dfx [MirrorTaskManagerLite::SetCurrDfxOpInfo] fail, dfxOpInfo is nullptr");
        HCCL_ERROR("[MirrorTaskManagerLite][SetCurrDfxOpInfo]fail, dfxOpInfo is nullptr");
        return;
    }
    currDfxOpInfo_     = dfxOpInfo;
    isStaticGraphMode_ = IsStaticGraphMode(dfxOpInfo->op_);
    opMode_            = dfxOpInfo->op_.opMode;
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::SetCurrDfxOpInfo] end, currDfxOpInfo[%p], "
        "isStaticGraphMode[%d], opMode[%d], opIndex[%u], groupName[%s]", currDfxOpInfo_.get(),
        isStaticGraphMode_, opMode_, currDfxOpInfo_->opIndex_, currDfxOpInfo_->groupName_.c_str());
    HCCL_INFO("[MirrorTaskManagerLite][SetCurrDfxOpInfo] Succeed, currDfxOpInfo_[%p], this[%p] !", currDfxOpInfo_.get(), this);
    return;
}

std::shared_ptr<DfxOpInfo> MirrorTaskManagerLite::GetCurrDfxOpInfo() const
{
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::GetCurrDfxOpInfo] this[%p], currDfxOpInfo[%p]",
        this, currDfxOpInfo_.get());
    HCCL_INFO("[MirrorTaskManagerLite][GetCurrDfxOpInfo] Succeed, currDfxOpInfo_[%p], this[%p] !", currDfxOpInfo_.get(), this);
    return currDfxOpInfo_;
}

TaskInfoQueue *MirrorTaskManagerLite::GetQueue(u32 streamId) const
{
    if (queueMap_.find(streamId) == queueMap_.end()) {
        HCCL_ERROR("YYYYYY hcomm dfx [MirrorTaskManagerLite::GetQueue] streamId(sqId)[%u] out of range, "
            "queueMapSize[%llu]", streamId, static_cast<unsigned long long>(queueMap_.size()));
        HCCL_ERROR("MirrorTaskManagerLite::GetQueue streamId(sqId)[%u] out of range", streamId);
        return nullptr;
    }
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::GetQueue] success, streamId(sqId)[%u], queue[%p], "
        "queueMapSize[%llu]", streamId, queueMap_.find(streamId)->second.get(),
        static_cast<unsigned long long>(queueMap_.size()));
    return queueMap_.find(streamId)->second.get();
}

std::shared_ptr<TaskInfo>  MirrorTaskManagerLite::GetTaskInfo(u32 streamId, u32 taskId) const
{
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::GetTaskInfo] start, this[%p], streamId(sqId)[%u], "
        "taskId(sqeId)[%u]", this, streamId, taskId);
    TaskInfoQueue *queue = nullptr;
    try {
        queue = GetQueue(streamId);
    } catch (HcclException &e) {
        HCCL_ERROR("YYYYYY hcomm dfx [MirrorTaskManagerLite::GetTaskInfo] Hccl exception %s was caught.", e.what());
        HCCL_ERROR("Hccl exception %s was caught.", e.what());
        return nullptr;
    }

    auto FindTask = [taskId](const std::shared_ptr<TaskInfo> &taskInfo) {
        return taskInfo->taskId_ == taskId;
    };

    auto task = *queue->Find(FindTask);
    if (task == *queue->End()) {
        HCCL_ERROR("YYYYYY hcomm dfx [MirrorTaskManagerLite::GetTaskInfo] not found, streamId(sqId)[%u], "
            "taskId(sqeId)[%u], queue[%p]", streamId, taskId, queue);
        return nullptr;
    };

    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::GetTaskInfo] success, streamId(sqId)[%u], "
        "taskId(sqeId)[%u], taskInfo[%p]", streamId, taskId, (*task).get());
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
