/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "task_exception_handler_lite.h"
#include "task_exception_func.h"
#include "communicator_impl_lite.h"

namespace Hccl {

using namespace std;

constexpr uint32_t TASK_CONTEXT_SIZE = 50;
constexpr uint32_t TASK_CONTEXT_INFO_SIZE = LOG_TMPBUF_SIZE - 50; // task 执行失败时打印前序task信息的长度限制

TaskExceptionHandlerLite &TaskExceptionHandlerLite::GetInstance()
{
    static TaskExceptionHandlerLite instance;
    return instance;
}

TaskExceptionHandlerLite::TaskExceptionHandlerLite()
{
    Register();
}

TaskExceptionHandlerLite::~TaskExceptionHandlerLite()
{
}

void TaskExceptionHandlerLite::Register() const
{
    TaskExceptionFunc::GetInstance().RegisterCallback(Process);
}

void TaskExceptionHandlerLite::Process(rtLogicCqReport_t* exceptionInfo)
{
    if (exceptionInfo == nullptr) {
        HCCL_ERROR("Exception process failed, rtExceptionInfo is nullptr.");
        return;
    }
    // exceptionInfo->taskId和exceptionInfo->streamId拼成sqeId
    const u32 sqeId = static_cast<uint32_t>(exceptionInfo->taskId << 16) | static_cast<uint32_t>(exceptionInfo->streamId);
    const auto curTask = GlobalMirrorTasks::Instance().GetTaskInfo(
            0, exceptionInfo->sqId, sqeId);
    if (curTask == nullptr) {
        // 未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId(sqId)[%u], taskId(sqeId)[%u].",
                   0, exceptionInfo->sqId, sqeId);
        return;
    }

    HCCL_ERROR("[TaskExceptionHandlerLite][%s]Task from HCCL run failed.", __func__);
    if (curTask->taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT) {
        PrintTaskContextInfo(exceptionInfo->sqId, sqeId);
    }
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, base information is %s.", curTask->GetBaseInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, para information is %s.", curTask->GetParaInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, groupRank information is %s.",
        GetGroupRankInfo(*curTask).c_str());
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, opData information is %s.", curTask->GetOpInfo().c_str());
}

string TaskExceptionHandlerLite::GetGroupRankInfo(const TaskInfo& taskInfo)
{
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]TaskInfo communicator is nullptr.", __func__);
        return "";
    }
    const CommunicatorImplLite* commImplLite = static_cast<CommunicatorImplLite*>(taskInfo.dfxOpInfo_->comm_);
    return StringFormat("group:[%s], rankSize[%u], rankId[%d]",
        commImplLite->GetId().c_str(), commImplLite->GetRankSize(), commImplLite->GetMyRank());
}

void TaskExceptionHandlerLite::PrintTaskContextInfo(uint32_t sqId, uint32_t taskId)
{
    auto queue = GlobalMirrorTasks::Instance().GetQueue(0, sqId);
    if (queue == nullptr) {
        // 未找到异常对应的TaskQueue
        HCCL_ERROR("Exception task queue not found. deviceId[%u], streamId(sqId)[%u].", 0, sqId);
        return;
    }

    auto func = [taskId] (const shared_ptr<TaskInfo>& task) { return task->taskId_ == taskId; };
    auto taskItorPtr = queue->Find(func);
    if (taskItorPtr == nullptr || *taskItorPtr == *queue->End()) {
        // 在队列中未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId(sqId)[%u], taskId(sqeId)[%u].", 0, sqId, taskId);
        return;
    }

    // 找到当前异常task的前50个task(至多)
    vector<shared_ptr<TaskInfo>> taskContext {};
    for (uint32_t i = 0; i < TASK_CONTEXT_SIZE && *taskItorPtr != *queue->Begin(); ++i, --(*taskItorPtr)) {
        if ((**taskItorPtr)->taskId_ > taskId) {
            break;
        }
        if ((**taskItorPtr)->taskId_ != taskId) {
            taskContext.emplace_back(**taskItorPtr);
        }
    }

    if (taskContext.empty()) {
        return;
    }

    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, context sequence before error task is "
        "[SDMA:M(rank), RDMA:RS(rank,id), SendPayload:SP(rank), InlineReduce:IR(rank), Reduce:R(rank), "
        "NotifyRecord:NR(rank,id), NotifyWait:NW(rank,id), SendNotify:SN(rank,id), "
        "WriteWithNotify:WN(rank,id), WriteReduceWithNotify:WRN(rank,id)]:");

    string taskContextInfo = "";
    for (auto it = taskContext.rbegin(); it != taskContext.rend(); ++it) {
        string conciseInfo = (*it)->GetConciseBaseInfo();
        conciseInfo += ",";

        if (taskContextInfo.size() + conciseInfo.size() >= TASK_CONTEXT_INFO_SIZE) {
            HCCL_ERROR("[TaskExceptionHandlerLite]%s", taskContextInfo.c_str());
            taskContextInfo = "";
        }

        taskContextInfo += conciseInfo;
    }
    HCCL_ERROR("[TaskExceptionHandlerLite]%s end.", taskContextInfo.c_str());
}

} // namespace Hccl