/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: task queue stub
 * Author: shenyutian
 * Create: 2024-07-27
 */

#include "task_queue_stub.h"
using namespace std;

namespace checker {

void SingleRankTaskQueues::AppendTask(Stream *stream, std::shared_ptr<TaskStub> task)
{
    if (taskQueues.size() < (stream->id() + 1)) {
        taskQueues.resize(stream->id() + 1);
    }

    taskQueues[stream->id()].push_back(task);
    return;
}

void AllRankTaskQueues::AppendTask(RankId rankId, Stream* stream, std::shared_ptr<TaskStub> task)
{
    if (rank2TaskQueues.count(rankId) == 0) {
        rank2TaskQueues[rankId] = new SingleRankTaskQueues();
        rankSize += 1;
    }
    rank2TaskQueues[rankId]->AppendTask(stream, task);
    return;
}

void TaskQueueStub::AppendTask(RankId rankId, Stream* stream, std::shared_ptr<TaskStub> task)
{
    Global()->GetAllRankTasks().AppendTask(rankId, stream, task);
    return;
}

std::shared_ptr<TaskStub> SingleRankTaskQueues::GetTask(QId queId, u32 pos) const
{
    return taskQueues.at(queId).at(pos);
}

u32 SingleRankTaskQueues::GetQueTaskNum(QId queId) const
{
    return taskQueues.at(queId).size();
}

std::vector<std::shared_ptr<TaskStub>> SingleRankTaskQueues::GetQueTasks(QId queId) const
{
    return taskQueues.at(queId);
}

std::vector<std::shared_ptr<TaskStub>> &SingleRankTaskQueues::operator[](QId queId)
{
    return taskQueues.at(queId);
}

void AllRankTaskQueues::Clear()
{
    for (RankId currRank = 0; currRank < rank2TaskQueues.size(); currRank++) {
        delete rank2TaskQueues[currRank];
        rank2TaskQueues[currRank] = nullptr;
    }
    rank2TaskQueues.clear();
    rankSize = 0;
    return;
}

SingleRankTaskQueues *AllRankTaskQueues::operator[](RankId rankId)
{
    return rank2TaskQueues.at(rankId);
}

SingleRankTaskQueues *AllRankTaskQueues::GetRankTaskQues(RankId rankId) const
{
    return rank2TaskQueues.at(rankId);
}

TaskQueueStub* TaskQueueStub::Global()
{
    static TaskQueueStub *taskQueue = new TaskQueueStub();
    return taskQueue;
}

void TaskQueueStub::Reset()
{
    allRankTaskQueues.Clear();
    return;
}

AllRankTaskQueues& TaskQueueStub::GetAllRankTasks()
{
    return allRankTaskQueues;
}

u32 TaskQueueStub::GetRankSize() const
{
    return allRankTaskQueues.rankSize;
}

SingleRankTaskQueues *TaskQueueStub::GetTaskQueueOfRank(RankId rankId) const
{
    return allRankTaskQueues.GetRankTaskQues(rankId);
}
}