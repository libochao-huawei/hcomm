/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 #include "profiling_reporter_lite.h"
 
namespace Hccl {
ProfilingReporterLite::ProfilingReporterLite(MirrorTaskManagerLite *mirrorTaskMgrLite,
                                             ProfilingHandlerLite *profilingHandlerLite, bool isIndop)
{
    if (UNLIKELY(mirrorTaskMgrLite == nullptr || profilingHandlerLite == nullptr)) {
        THROW<InternalException>("[ProfilingHandler] ProfilingReporterLite is nullptr.");
    }
    mirrorTaskMgrLite_        = mirrorTaskMgrLite;
    profilingHandlerLite_ = profilingHandlerLite;
    mirrorTaskMgrLite_->RegFullyCallBack([this]() {
        ReportAllTasks();
    });
}

ProfilingReporterLite::~ProfilingReporterLite()
{
}

void ProfilingReporterLite::Init() const
{
    ProfilingHandlerLite::GetInstance().Init();
}

void ProfilingReporterLite::ForEachQueue(
    const std::function<void(u32 streamId, Queue<std::shared_ptr<TaskInfo>> *queue)> &callback) const
{
    for (auto it = mirrorTaskMgrLite_->Begin(); it != mirrorTaskMgrLite_->End(); ++it) {
        u32                               streamId  = it->first;
        Queue<std::shared_ptr<TaskInfo>> *currQueue = it->second.get();
        if (currQueue == nullptr) {
            HCCL_WARNING("[ProfilingReporterLite][ForEachQueue] currQueue is nullptr, continue to next task.");
            continue;
        }
        callback(streamId, currQueue);
    }
}

void ProfilingReporterLite::ReportAllTasks()
{
    if (ProfilingHandlerLite::GetInstance().GetProfL1State() == false) {
        HCCL_DEBUG("[ProfilingReporterLite][ReportAllTasks] GetProfL1State is false, UpdateAllLastPos and skip report");
        UpdateAllLastPos();
        return;
    }

    std::vector<TaskInfo> taskInfo;
    ForEachQueue([&](u32 streamId, Queue<std::shared_ptr<TaskInfo>> *currQueue) {
        if (currQueue->Begin() == nullptr || currQueue->Tail() == nullptr) {
            HCCL_WARNING("[ProfilingReporterLite][ReportAllTasks] currQueue Begin or Tail is nullptr, "
                         "continue to next task.");
            return;
        }
        auto &beginRef = *(*(currQueue->Begin()));
        auto &tailRef = *(*(currQueue->Tail()));
        if (beginRef == nullptr || tailRef == nullptr) {
            HCCL_WARNING("[ProfilingReporterLite][ReportAllTasks] task in queue is nullptr, continue to next task.");
            return;
        }

        // 首次处理该 stream，手动将首个 task 打印一遍
        auto &lastPos = lastPoses_[streamId];
        if (lastPos == nullptr) {
            const TaskInfo &task = *beginRef;  // 使用 const 引用避免深拷贝
            HCCL_INFO("[ProfilingReporterLite][ReportAllTasks] streamId = %u, taskId = %u", task.streamId_, task.taskId_);
            HCCL_INFO("taskParam_task.type %s", task.taskParam_.Describe().c_str());
            taskInfo.push_back(task);
            lastPos = currQueue->Begin();
        }

        auto endPos = currQueue->Tail();
        auto iter = lastPos;
        ++(*iter);
        for (; (*(iter)) != (*(currQueue->End())); ++(*(iter))) {
            const TaskInfo &task = *(*(*iter));  // 使用 const 引用避免深拷贝
            HCCL_INFO("[ProfilingReporterLite][ReportAllTasks] streamId = %u, taskId = %u", task.streamId_, task.taskId_);
            HCCL_INFO("taskParam_task.type %s", task.taskParam_.Describe().c_str());
            taskInfo.push_back(task);
        }
        lastPos = endPos;
    });

    if (!taskInfo.empty()) {
        ProfilingHandlerLite::GetInstance().ReportHcclTaskDetails(taskInfo);
    }
}

void ProfilingReporterLite::UpdateProfStat(void) const
{
    ProfilingHandlerLite::GetInstance().UpdateProfSwitch();
}

void ProfilingReporterLite::UpdateAllLastPos()
{
    ForEachQueue([&](u32 streamId, Queue<std::shared_ptr<TaskInfo>> *currQueue) {
        auto endPos = currQueue->Tail();
        if (endPos == nullptr) {
            HCCL_WARNING("[ProfilingReporterLite][%s] endPos is nullptr, continue to next task.", __func__);
            return;
        }
        lastPoses_[streamId] = endPos;
    });
}
 
} // namespace Hccl
