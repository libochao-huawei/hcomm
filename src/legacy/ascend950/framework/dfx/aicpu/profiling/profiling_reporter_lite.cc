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

/*
*  (*currQueue) == Queue<std::unique_ptr<TaskInfo>> = QUEUE
*  QUEUE.Begin() =std::shared_ptr<Iterator<unique_ptr<taskInfo>>
*  *QUEUE.Begin() = Iterator<unique_ptr<taskInfo>
*  *(*QUEUE.Begin()) = unique_ptr<taskInfo>
*  *(*(*QUEUE.Begin())) = taskInfo;
*  taskInfo.push_back((*(*((*currQueue).Begin())));
*/
// 所有的迭代器都是make_shared 永不为空  底层修改之后 需求再看下
void ProfilingReporterLite::ReportAllTasksLog()
{
    if (HcclCheckLogLevel(HCCL_LOG_INFO) == 0) {
        return;
    }
    for (auto it = mirrorTaskMgrLite_->Begin(); it != mirrorTaskMgrLite_->End(); ++it) {
        u32 streamId = it->first;
        Queue<std::unique_ptr<TaskInfo>> *currQueue = it->second.get();
        if (currQueue == nullptr || currQueue->Begin() == nullptr || currQueue->Tail() == nullptr) {
            continue;
        }
        bool logAll = (lastPoses_.find(streamId) == lastPoses_.end());
        for (auto logIter = currQueue->Begin(); *logIter != *currQueue->End(); ++(*logIter)) {
            if (*(*logIter) == nullptr) {
                continue;
            }
            if (!logAll && *logIter == *lastPoses_[streamId]) { //  找到旧 Tail 位置后设为 logAll=true，continue 跳过该位置，后续全打印
                logAll = true;
                continue;
            }
            if (!logAll) {
                continue;
            }
            TaskInfo task = (*(*(*logIter)));
            HCCL_INFO("[ProfilingReporterLite][ReportAllTasks] %s", task.Describe().c_str());
        }
    }
}

void ProfilingReporterLite::ReportAllTasks()
{
    ReportAllTasksLog();
    if (ProfilingHandlerLite::GetInstance().GetProfL1State() == false) {
        HCCL_DEBUG("[ProfilingReporterLite][ReportAllTasks] GetProfL1State is false, UpdateAllLastPos and skip report");
        UpdateAllLastPos();
        return;
    }

    std::vector<TaskInfo> taskInfo;
    taskInfo.reserve(8192);
    for (auto it = mirrorTaskMgrLite_->Begin(); it != mirrorTaskMgrLite_->End(); ++it) {
        u32                               streamId  = it->first;
        Queue<std::unique_ptr<TaskInfo>> *currQueue = it->second.get();
        if (currQueue == nullptr || (*(*(currQueue->Begin()))) == nullptr
            || (*(*(currQueue->Tail()))) == nullptr) {
            HCCL_WARNING("[ProfilingReporterLite][ReportAllTasks] currQueue is nullptr, continue to next task.");
            continue;
        }
        // 不论首次是否打印，都手动将首个task打印一遍
        if (lastPoses_.find(streamId) == lastPoses_.end()) {
            TaskInfo task = (*(*(*currQueue->Begin())));
            taskInfo.push_back(task);
            lastPoses_[streamId] = currQueue->Begin();
        }
        auto endPos = currQueue->Tail();
        auto iter = lastPoses_[streamId];
        ++(*iter);// 上一次的最后一个元素++后 到这一次的最新的元素
        for (; (*(iter)) != (*(currQueue->End())); ++(*(iter))) {
            TaskInfo task = (*(*(*iter)));
            taskInfo.push_back(task);
        }
        lastPoses_[streamId] = endPos;// 记录每个 stream 上次 ReportAllTasks 上报结束时已处理到的位置（即当时的 Tail）
    }
    ProfilingHandlerLite::GetInstance().ReportHcclTaskDetails(taskInfo);
}

void ProfilingReporterLite::UpdateProfStat(void) const
{
    ProfilingHandlerLite::GetInstance().UpdateProfSwitch();
}

void ProfilingReporterLite::UpdateAllLastPos()
{
    for (auto it = mirrorTaskMgrLite_->Begin(); it != mirrorTaskMgrLite_->End(); ++it) {
        u32                               streamId  = it->first;
        Queue<std::unique_ptr<TaskInfo>> *currQueue = it->second.get();// 一旦有streamid 必有queue 必不为空，只是元素可能会为0

        auto endPos = currQueue->Tail();
        lastPoses_[streamId] = endPos;
    }
}
 
} // namespace Hccl