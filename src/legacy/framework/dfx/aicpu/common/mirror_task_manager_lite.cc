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
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] begin, this[%p], taskInfo[%p], "
        "currDfxOpInfo[%p], queueMapSize[%llu]", this, taskInfo.get(), currDfxOpInfo_.get(),
        static_cast<unsigned long long>(queueMap_.size()));
    if (UNLIKELY(taskInfo == nullptr)) {
        HCCL_ERROR("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] taskInfo is nullptr, this[%p]", this);
        THROW<InternalException>(
            StringFormat("MirrorTaskManagerLite::AddTaskInfo taskInfo is nullptr"));
    }
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] task decoded, streamId[%u], "
        "taskId[%u], taskDfxOpInfo[%p]", taskInfo->streamId_, taskInfo->taskId_, taskInfo->dfxOpInfo_.get());

    if (taskInfo->dfxOpInfo_ == nullptr) {
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] task dfxOpInfo null, use currDfxOpInfo[%p]",
            currDfxOpInfo_.get());
        taskInfo->dfxOpInfo_ = currDfxOpInfo_;
    }

    if (queueMap_.find(taskInfo->streamId_) == queueMap_.end()) {
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] create queue begin, streamId[%u], "
            "queueMapSize[%llu]", taskInfo->streamId_, static_cast<unsigned long long>(queueMap_.size()));
        queueMap_[taskInfo->streamId_] = std::make_unique<CircularQueue<std::shared_ptr<TaskInfo>>>(MAX_CIRCULAR_QUEUE_LENGTH);
        queueTaskNum[taskInfo->streamId_] = 0;
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] create queue end, streamId[%u], "
            "queue[%p], queueMapSize[%llu]", taskInfo->streamId_, queueMap_[taskInfo->streamId_].get(),
            static_cast<unsigned long long>(queueMap_.size()));
    }

    if (queueTaskNum[taskInfo->streamId_] == static_cast<u32>(queueMap_[taskInfo->streamId_]->Capacity())) {
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] queue full callback begin, "
            "streamId[%u], queueTaskNum[%u], capacity[%llu], hasCallback[%d]", taskInfo->streamId_,
            queueTaskNum[taskInfo->streamId_],
            static_cast<unsigned long long>(queueMap_[taskInfo->streamId_]->Capacity()),
            static_cast<int>(static_cast<bool>(fullyCallBack_)));
        fullyCallBack_();
        queueTaskNum[taskInfo->streamId_] = 0;
        HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] queue full callback end, streamId[%u]",
            taskInfo->streamId_);
    }

    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] append begin, streamId[%u], taskId[%u], "
        "queue[%p], queueTaskNum[%u]", taskInfo->streamId_, taskInfo->taskId_,
        queueMap_[taskInfo->streamId_].get(), queueTaskNum[taskInfo->streamId_]);
    queueMap_[taskInfo->streamId_]->Append(taskInfo);
    queueTaskNum[taskInfo->streamId_]++;

    HCCL_INFO("[MirrorTaskManagerLite][AddTaskInfo]add streamId(sqId)[%u] taskId(sqeId)[%u] queueMapsize[%llu]",
        taskInfo->streamId_, taskInfo->taskId_, static_cast<unsigned long long>(queueMap_.size()));
    HCCL_INFO("YYYYYY hcomm dfx [MirrorTaskManagerLite::AddTaskInfo] end, streamId[%u], taskId[%u], "
        "queueTaskNum[%u], taskDfxOpInfo[%p]", taskInfo->streamId_, taskInfo->taskId_,
        queueTaskNum[taskInfo->streamId_], taskInfo->dfxOpInfo_.get());

    return;
}

bool MirrorTaskManagerLite::IsStaticGraphMode(const CollOperator &collOperator) const
{
    return (collOperator.staticAddr == false) && (collOperator.staticShape == false);
}

void MirrorTaskManagerLite::SetCurrDfxOpInfo(std::shared_ptr<DfxOpInfo> dfxOpInfo)
{
    if (dfxOpInfo == nullptr) {
        HCCL_ERROR("[MirrorTaskManagerLite][SetCurrDfxOpInfo]fail, dfxOpInfo is nullptr");
        return;
    }
    currDfxOpInfo_     = dfxOpInfo;
    isStaticGraphMode_ = IsStaticGraphMode(dfxOpInfo->op_);
    opMode_            = dfxOpInfo->op_.opMode;
    HCCL_INFO("[MirrorTaskManagerLite][SetCurrDfxOpInfo] Succeed, currDfxOpInfo_[%p], this[%p] !", currDfxOpInfo_.get(), this);
    return;
}

std::shared_ptr<DfxOpInfo> MirrorTaskManagerLite::GetCurrDfxOpInfo() const
{
    HCCL_INFO("[MirrorTaskManagerLite][GetCurrDfxOpInfo] Succeed, currDfxOpInfo_[%p], this[%p] !", currDfxOpInfo_.get(), this);
    return currDfxOpInfo_;
}

TaskInfoQueue *MirrorTaskManagerLite::GetQueue(u32 streamId) const
{
    if (queueMap_.find(streamId) == queueMap_.end()) {
        HCCL_ERROR("MirrorTaskManagerLite::GetQueue streamId(sqId)[%u] out of range", streamId);
        return nullptr;
    }
    return queueMap_.find(streamId)->second.get();
}

std::shared_ptr<TaskInfo>  MirrorTaskManagerLite::GetTaskInfo(u32 streamId, u32 taskId) const
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
