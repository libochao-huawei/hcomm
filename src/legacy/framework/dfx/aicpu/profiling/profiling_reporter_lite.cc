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
ProfilingReporterLite::ProfilingReporterLite(MirrorTaskManager    *mirrorTaskMgr,
                                             ProfilingHandlerLite *profilingHandlerLite, bool isIndop)
{
    if (UNLIKELY(mirrorTaskMgr == nullptr || profilingHandlerLite == nullptr)) {
        THROW<InternalException>("[ProfilingHandler] ProfilingReporterLite is nullptr.");
    }
    mirrorTaskMgr_        = mirrorTaskMgr;
    profilingHandlerLite_ = profilingHandlerLite;
    if (isIndop == false) {
        mirrorTaskMgr_->RegFullyCallBack([this]() {
            ReportAllTasks();
        });
        return;
    }
    mirrorTaskMgr_->RegFullyCallBack([this]() {
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

/*
*  (*currQueue) == Queue<std::shared_ptr<TaskInfo>> = QUEUE
*  QUEUE.Begin() =std::shared_ptr<Iterator<shared_ptr<taskInfo>>
*  *QUEUE.Begin() = Iterator<shared_ptr<taskInfo>
*  *(*QUEUE.Begin()) = shared_ptr<taskInfo>
*  *(*(*QUEUE.Begin())) = taskInfo;
*  taskInfo.push_back((*(*((*currQueue).Begin())));
*/

void ProfilingReporterLite::ReportAllTasks()
{
    std::vector<TaskInfo> taskInfo;
    for (auto it = mirrorTaskMgr_->Begin(); it != mirrorTaskMgr_->End(); ++it) {
        u32                               streamId  = it->first;
        Queue<std::shared_ptr<TaskInfo>> *currQueue = it->second;
        if (currQueue == nullptr || currQueue->Begin() == nullptr || (*(*(currQueue->Begin()))) == nullptr) {
            HCCL_WARNING("[ProfilingReporterLite][ReportAllTasks] currQueue is nullptr, continue to next task.");
            continue;
        }
        // 不论首次是否打印，都手动将首个task打印一遍
        if (lastPoses_.find(streamId) == lastPoses_.end()) {
            TaskInfo task = (*(*(*currQueue->Begin())));
            taskInfo.push_back(task);
            lastPoses_[streamId] = currQueue->Begin();
        }
        if (currQueue->Tail() == nullptr) {
            continue;
        }
        auto endPos = currQueue->Tail();
        auto iter = lastPoses_[streamId];
        ++(*iter);
        for (; (*(iter)) != (*(currQueue->End())); ++(*(iter))) {
            TaskInfo task = (*(*(*iter)));
            HCCL_INFO("taskParam_task.type %s", task.taskParam_.Describe().c_str());
            taskInfo.push_back(task);
        }
        lastPoses_[streamId] = endPos;
    }
    ProfilingHandlerLite::GetInstance().ReportHcclTaskDetails(taskInfo);
}

void ProfilingReporterLite::UpdateProfStat(void) const
{
    ProfilingHandlerLite::GetInstance().UpdateProfSwitch();
}
 
} // namespace Hccl