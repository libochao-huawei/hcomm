/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_task_transform_v3.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <utility>

#include "aiv_snapshot_json_loader_v3.h"
#include "sim_log.h"

namespace HcclSim {
namespace TaskGraphGeneratorV3 {
namespace {
constexpr uint32_t PIPE_S = 0U;
constexpr uint32_t PIPE_MTE2 = 1U;
constexpr uint32_t PIPE_MTE3 = 2U;
constexpr uint32_t PIPE_ALL = 3U;
constexpr uint32_t AIV_PIPE_NUM = 3U;

using AivExpandClock = std::chrono::steady_clock;

struct AivNodeRecord {
    NodeId nodeId{INVALID_NODE_ID};
    const AivRuntimeTaskV3 *task{nullptr};
    uint64_t order{0};
};

struct AivLaunchContext {
    TaskGraphGeneratorV3 *graph{nullptr};
    StorageManager *storage{nullptr};
    TaskAivGraph *placeholder{nullptr};
    AivRuntimeTaskSnapshotV3 snapshot;
    NodeId headNodeId{INVALID_NODE_ID};
    NodeId endNodeId{INVALID_NODE_ID};
    std::map<uint32_t, AivNodeRecord> taskIdToNode;
    std::vector<NodeId> internalNodeIds;
    size_t setWaitEdgeCount{0};
    size_t pipeBarrierMergeCount{0};
    size_t syncAllMergeCount{0};
};

struct SetWaitKey {
    RankId rankId{INVALID_RANK_ID};
    uint64_t launchIdx{0};
    uint32_t blockId{0};
    uint32_t srcPipe{0};
    uint32_t dstPipe{0};
    int32_t eventId{0};

    bool operator<(const SetWaitKey &rhs) const
    {
        if (rankId != rhs.rankId) {
            return rankId < rhs.rankId;
        }
        if (launchIdx != rhs.launchIdx) {
            return launchIdx < rhs.launchIdx;
        }
        if (blockId != rhs.blockId) {
            return blockId < rhs.blockId;
        }
        if (srcPipe != rhs.srcPipe) {
            return srcPipe < rhs.srcPipe;
        }
        if (dstPipe != rhs.dstPipe) {
            return dstPipe < rhs.dstPipe;
        }
        return eventId < rhs.eventId;
    }
};

struct FlagCellKey {
    RankId flagOwnerRank{INVALID_RANK_ID};
    uint64_t launchIdx{0};
    uint64_t flagOffset{0};

    bool operator<(const FlagCellKey &rhs) const
    {
        if (flagOwnerRank != rhs.flagOwnerRank) {
            return flagOwnerRank < rhs.flagOwnerRank;
        }
        if (launchIdx != rhs.launchIdx) {
            return launchIdx < rhs.launchIdx;
        }
        return flagOffset < rhs.flagOffset;
    }
};

struct FlagCellState {
    int32_t currentValue{0};
    std::vector<NodeId> producerNodes;
};

struct MergeGroup {
    std::vector<NodeId> memberNodeIds;
    std::vector<uint32_t> memberTaskIds;
    std::vector<uint32_t> memberPipes;
    uint64_t topoOrder{0};
    uint32_t blockId{0};
    uint32_t pipeType{std::numeric_limits<uint32_t>::max()};
    uint32_t syncRound{std::numeric_limits<uint32_t>::max()};
};

uint64_t ElapsedNs(AivExpandClock::time_point start, AivExpandClock::time_point end)
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

bool IsValidPipe(uint32_t pipe)
{
    return pipe < AIV_PIPE_NUM;
}

TaskPosition MakeAivPosition(RankId rankId, uint64_t launchIdx, uint32_t blockId, uint32_t pipe,
    uint32_t taskId = std::numeric_limits<uint32_t>::max())
{
    TaskPosition position;
    position.rankId = rankId;
    position.streamId = INVALID_STREAM_ID;
    position.launchIdx = launchIdx;
    position.blockId = blockId;
    position.pipe = pipe;
    position.taskId = taskId;
    const uint32_t queueBlockId = blockId == std::numeric_limits<uint32_t>::max() ? 0U : blockId;
    const uint32_t queuePipe = IsValidPipe(pipe) ? pipe : 0U;
    position.queueId = static_cast<QueueId>((launchIdx & 0xFFFFULL) * AIV_PIPE_NUM * 1024ULL +
        static_cast<uint64_t>(queueBlockId) * AIV_PIPE_NUM + queuePipe);
    return position;
}

MemType ConvertAivMemType(AivBufferTypeV3 bufferType)
{
    switch (bufferType) {
        case AivBufferTypeV3::INPUT:
            return MemType::INPUT;
        case AivBufferTypeV3::OUTPUT:
            return MemType::OUTPUT;
        case AivBufferTypeV3::CCL:
            return MemType::CCL;
        case AivBufferTypeV3::UB:
            return MemType::UB_AIV;
        case AivBufferTypeV3::FLAG:
            return MemType::FLAG_AIV;
        default:
            return MemType::INVALID;
    }
}

MemSlice ConvertAivSlice(RankId rankId, const AivDataSliceV3 &slice)
{
    MemSlice result;
    result.rankId = rankId;
    result.memType = ConvertAivMemType(slice.type);
    result.offset = slice.offset;
    result.len = slice.size;
    return result;
}

TaskPosition MakeAivLocation(const AivRuntimeTaskV3 &task, uint64_t launchIdx)
{
    return MakeAivPosition(task.rankId, launchIdx, task.blockId, task.curPipe, task.taskId);
}

SetWaitKey MakeSetWaitKey(const AivPipeEvent &event)
{
    return SetWaitKey{event.rankId, event.launchIdx, event.blockId, event.srcPipe, event.dstPipe, event.eventId};
}

FlagCellKey MakeFlagCellKey(const AivFlagSync &flag)
{
    return FlagCellKey{flag.flagOwnerRank, flag.launchIdx, flag.flagOffset};
}

std::unique_ptr<TaskNode> TranslateAivRuntimeTask(const AivRuntimeTaskV3 &task, uint64_t launchIdx)
{
    switch (task.taskType) {
        case AivRuntimeTaskTypeV3::MEM_COPY: {
            const MemSlice src = ConvertAivSlice(task.srcRank, task.src);
            const MemSlice dst = ConvertAivSlice(task.dstRank, task.dst);
            return std::make_unique<TaskTransMem>(src, dst, ProtocolType::SDMA);
        }
        case AivRuntimeTaskTypeV3::REDUCE: {
            const MemSlice src = ConvertAivSlice(task.srcRank, task.src);
            const MemSlice dst = ConvertAivSlice(task.dstRank, task.dst);
            auto node = std::make_unique<TaskReduce>(src, dst, static_cast<uint8_t>(task.dataType),
                static_cast<uint8_t>(task.reduceOp), ProtocolType::SDMA);
            return node;
        }
        case AivRuntimeTaskTypeV3::SET_FLAG: {
            AivPipeEvent event;
            event.rankId = task.rankId;
            event.launchIdx = launchIdx;
            event.blockId = task.blockId;
            event.curPipe = task.curPipe;
            event.srcPipe = task.srcPipe;
            event.dstPipe = task.dstPipe;
            event.eventId = task.eventId;
            event.taskId = task.taskId;
            return std::make_unique<TaskAivSetFlag>(event);
        }
        case AivRuntimeTaskTypeV3::WAIT_FLAG: {
            AivPipeEvent event;
            event.rankId = task.rankId;
            event.launchIdx = launchIdx;
            event.blockId = task.blockId;
            event.curPipe = task.curPipe;
            event.srcPipe = task.srcPipe;
            event.dstPipe = task.dstPipe;
            event.eventId = task.eventId;
            event.taskId = task.taskId;
            return std::make_unique<TaskAivWaitFlag>(event);
        }
        case AivRuntimeTaskTypeV3::PIPE_BARRIER: {
            AivBarrierInfo info;
            info.taskLoc = MakeAivLocation(task, launchIdx);
            info.pipeType = task.pipeType;
            info.memberTaskIds = task.barrierGroupTaskIds;
            return std::make_unique<TaskAivPipeBarrier>(std::move(info));
        }
        case AivRuntimeTaskTypeV3::SYNC_ALL: {
            AivSyncAllInfo info;
            info.taskLoc = MakeAivLocation(task, launchIdx);
            info.syncRound = task.syncRound;
            return std::make_unique<TaskAivSyncAll>(std::move(info));
        }
        case AivRuntimeTaskTypeV3::SEND_FLAG: {
            AivFlagSync flag;
            flag.currentRank = task.rankId;
            flag.flagOwnerRank = task.flagOwnerRank;
            flag.launchIdx = launchIdx;
            flag.blockId = task.blockId;
            flag.curPipe = task.curPipe;
            flag.taskId = task.taskId;
            flag.flagOffset = task.flagOffset;
            flag.value = task.flagValue;
            return std::make_unique<TaskAivSendFlag>(flag);
        }
        case AivRuntimeTaskTypeV3::RECV_FLAG: {
            AivFlagSync flag;
            flag.currentRank = task.rankId;
            flag.flagOwnerRank = task.flagOwnerRank;
            flag.launchIdx = launchIdx;
            flag.blockId = task.blockId;
            flag.curPipe = task.curPipe;
            flag.taskId = task.taskId;
            flag.flagOffset = task.flagOffset;
            flag.value = task.flagValue;
            return std::make_unique<TaskAivRecvFlag>(flag);
        }
        default:
            return nullptr;
    }
}

HcclResult AddEdge(TaskGraphGeneratorV3 *graph, NodeId parent, NodeId child)
{
    if (graph == nullptr) {
        return HCCL_E_PTR;
    }
    if (parent == child) {
        return HCCL_SUCCESS;
    }
    return graph->AddEdge(parent, child);
}

HcclResult RewireGroupMember(TaskGraphGeneratorV3 *graph, NodeId memberNodeId, NodeId mergeNodeId)
{
    if (graph == nullptr) {
        return HCCL_E_PTR;
    }
    TaskNode *member = graph->GetNode(memberNodeId);
    if (member == nullptr) {
        return HCCL_E_PTR;
    }

    std::vector<TaskNode *> parents = member->GetParents();
    std::vector<TaskNode *> children = member->GetChildren();
    for (TaskNode *parent : parents) {
        if (parent == nullptr || parent->GetNodeId() == mergeNodeId) {
            continue;
        }
        HcclResult ret = graph->RemoveEdge(parent->GetNodeId(), memberNodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = AddEdge(graph, parent->GetNodeId(), mergeNodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }

    for (TaskNode *child : children) {
        if (child == nullptr || child->GetNodeId() == mergeNodeId) {
            continue;
        }
        HcclResult ret = graph->RemoveEdge(memberNodeId, child->GetNodeId());
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = AddEdge(graph, mergeNodeId, child->GetNodeId());
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ValidateSnapshot(const AivLaunchContext &ctx)
{
    if (ctx.storage == nullptr || ctx.placeholder == nullptr) {
        return HCCL_E_PTR;
    }
    const CheckerParam param = ctx.storage->GetCheckerParam();
    if (ctx.snapshot.rankSize != 0 && param.rankSize != 0 && ctx.snapshot.rankSize != param.rankSize) {
        HCCL_VM_ERROR("[AivTaskTransformV3] AIV snapshot rankSize mismatch, rank={}, launch={}, "
            "snapshotRankSize={}, checkerRankSize={}, file={}", ctx.placeholder->GetRankId(),
            ctx.placeholder->GetLaunchIdx(), ctx.snapshot.rankSize, param.rankSize, ctx.snapshot.filePath);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult UpdateAivBufferSize(uint64_t snapshotSize, const char *fieldName, const AivLaunchContext &ctx,
    uint64_t &bufferSize)
{
    if (snapshotSize == 0) {
        return HCCL_SUCCESS;
    }
    if (bufferSize == 0) {
        bufferSize = snapshotSize;
        return HCCL_SUCCESS;
    }
    if (bufferSize == snapshotSize) {
        return HCCL_SUCCESS;
    }

    HCCL_VM_ERROR("[AivTaskTransformV3] AIV {} mismatch, rank={}, launch={}, expected={}, actual={}, file={}",
        fieldName, ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(), bufferSize, snapshotSize,
        ctx.snapshot.filePath);
    return HCCL_E_PARA;
}

HcclResult AppendRuntimeTask(AivLaunchContext &ctx, const AivRuntimeTaskV3 &task, uint64_t order, NodeId &nodeId)
{
    std::unique_ptr<TaskNode> node = TranslateAivRuntimeTask(task, ctx.placeholder->GetLaunchIdx());
    if (node == nullptr) {
        HCCL_VM_ERROR("[AivTaskTransformV3] Translate AIV task failed, rank={}, launch={}, taskId={}, taskType={}, "
            "file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(), task.taskId,
            static_cast<uint32_t>(task.taskType), ctx.snapshot.filePath);
        return HCCL_E_NOT_SUPPORT;
    }

    const TaskPosition position = MakeAivPosition(task.rankId, ctx.placeholder->GetLaunchIdx(), task.blockId,
        task.curPipe, task.taskId);
    HcclResult ret = ctx.graph->AppendGeneratedNode(std::move(node), position, nodeId);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }

    ctx.internalNodeIds.push_back(nodeId);
    ctx.taskIdToNode[task.taskId] = AivNodeRecord{nodeId, &task, order};
    return HCCL_SUCCESS;
}

HcclResult AppendPipeTasks(AivLaunchContext &ctx, const std::vector<AivRuntimeTaskV3> &tasks, NodeId headNodeId,
    uint64_t &order, std::vector<NodeId> &tailNodes)
{
    NodeId prevNodeId = INVALID_NODE_ID;
    for (const auto &task : tasks) {
        NodeId currNodeId = INVALID_NODE_ID;
        HcclResult ret = AppendRuntimeTask(ctx, task, order++, currNodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = (prevNodeId == INVALID_NODE_ID) ? AddEdge(ctx.graph, headNodeId, currNodeId) :
            AddEdge(ctx.graph, prevNodeId, currNodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        prevNodeId = currNodeId;
    }
    if (prevNodeId != INVALID_NODE_ID) {
        tailNodes.push_back(prevNodeId);
    }
    return HCCL_SUCCESS;
}

HcclResult BuildPipeSkeleton(AivLaunchContext &ctx, std::vector<NodeId> &tailNodes)
{
    uint64_t order = 0;
    for (const auto &block : ctx.snapshot.blocks) {
        HcclResult ret = AppendPipeTasks(ctx, block.scalarTasks, ctx.headNodeId, order, tailNodes);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = AppendPipeTasks(ctx, block.mte2Tasks, ctx.headNodeId, order, tailNodes);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = AppendPipeTasks(ctx, block.mte3Tasks, ctx.headNodeId, order, tailNodes);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult AddSetWaitFlagEdges(AivLaunchContext &ctx)
{
    std::set<NodeId> internalSet(ctx.internalNodeIds.begin(), ctx.internalNodeIds.end());
    std::deque<NodeId> readyQueue;
    std::set<NodeId> queued;
    std::set<NodeId> executed;
    std::map<SetWaitKey, std::deque<NodeId>> seenSetFlags;

    TaskNode *head = ctx.graph->GetNode(ctx.headNodeId);
    if (head == nullptr) {
        return HCCL_E_PTR;
    }
    for (TaskNode *child : head->GetChildren()) {
        if (child == nullptr) {
            return HCCL_E_PTR;
        }
        if (internalSet.count(child->GetNodeId()) != 0) {
            queued.insert(child->GetNodeId());
            readyQueue.push_back(child->GetNodeId());
        }
    }

    auto isExecutable = [&executed](const TaskNode *node) {
        if (node == nullptr) {
            return false;
        }
        for (const TaskNode *parent : node->GetParents()) {
            if (parent == nullptr) {
                return false;
            }
            if (parent->GetType() == TaskType::START) {
                continue;
            }
            if (executed.count(parent->GetNodeId()) == 0) {
                return false;
            }
        }
        return true;
    };
    auto enqueueChildren = [&ctx, &internalSet, &queued, &readyQueue](const TaskNode *node) -> HcclResult {
        if (node == nullptr) {
            return HCCL_E_PTR;
        }
        for (TaskNode *child : node->GetChildren()) {
            if (child == nullptr) {
                return HCCL_E_PTR;
            }
            const NodeId childId = child->GetNodeId();
            if (internalSet.count(childId) == 0 || queued.count(childId) != 0) {
                continue;
            }
            readyQueue.push_back(childId);
            queued.insert(childId);
        }
        return HCCL_SUCCESS;
    };

    size_t unmatchedCnt = 0;
    while (!readyQueue.empty()) {
        if (unmatchedCnt >= readyQueue.size()) {
            TaskNode *node = ctx.graph->GetNode(readyQueue.front());
            HCCL_VM_ERROR("[AivTaskTransformV3] SetFlag/WaitFlag matching deadlock, firstUnmatched={}, file={}",
                node == nullptr ? "null" : node->Describe(), ctx.snapshot.filePath);
            return HCCL_E_INTERNAL;
        }

        const NodeId nodeId = readyQueue.front();
        readyQueue.pop_front();
        queued.erase(nodeId);
        if (executed.count(nodeId) != 0) {
            unmatchedCnt = 0;
            continue;
        }

        TaskNode *node = ctx.graph->GetNode(nodeId);
        if (node == nullptr) {
            return HCCL_E_PTR;
        }
        if (!isExecutable(node)) {
            readyQueue.push_back(nodeId);
            queued.insert(nodeId);
            ++unmatchedCnt;
            continue;
        }

        if (node->GetType() == TaskType::AIV_SET_FLAG) {
            const auto *setFlag = dynamic_cast<const TaskAivSetFlag *>(node);
            if (setFlag == nullptr) {
                return HCCL_E_PTR;
            }
            seenSetFlags[MakeSetWaitKey(setFlag->GetEvent())].push_back(nodeId);
            executed.insert(nodeId);
            HcclResult ret = enqueueChildren(node);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
            unmatchedCnt = 0;
            continue;
        }
        if (node->GetType() != TaskType::AIV_WAIT_FLAG) {
            executed.insert(nodeId);
            HcclResult ret = enqueueChildren(node);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
            unmatchedCnt = 0;
            continue;
        }
        const auto *waitFlag = dynamic_cast<const TaskAivWaitFlag *>(node);
        if (waitFlag == nullptr) {
            return HCCL_E_PTR;
        }
        const SetWaitKey key = MakeSetWaitKey(waitFlag->GetEvent());
        auto iter = seenSetFlags.find(key);
        if (iter == seenSetFlags.end() || iter->second.empty()) {
            readyQueue.push_back(nodeId);
            queued.insert(nodeId);
            ++unmatchedCnt;
            continue;
        }
        const NodeId setNodeId = iter->second.front();
        iter->second.pop_front();
        HcclResult ret = AddEdge(ctx.graph, setNodeId, nodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ++ctx.setWaitEdgeCount;
        executed.insert(nodeId);
        ret = enqueueChildren(node);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        unmatchedCnt = 0;
    }

    for (const auto &entry : seenSetFlags) {
        if (!entry.second.empty()) {
            TaskNode *node = ctx.graph->GetNode(entry.second.front());
            HCCL_VM_WARN("[AivTaskTransformV3] Unconsumed SetFlag, node={}, file={}",
                node == nullptr ? "null" : node->Describe(), ctx.snapshot.filePath);
        }
    }
    return HCCL_SUCCESS;
}

class DisjointSet {
public:
    explicit DisjointSet(size_t size) : parent_(size), rank_(size, 0)
    {
        for (size_t i = 0; i < size; ++i) {
            parent_[i] = i;
        }
    }

    size_t Find(size_t index)
    {
        if (parent_[index] != index) {
            parent_[index] = Find(parent_[index]);
        }
        return parent_[index];
    }

    void Union(size_t lhs, size_t rhs)
    {
        size_t lhsRoot = Find(lhs);
        size_t rhsRoot = Find(rhs);
        if (lhsRoot == rhsRoot) {
            return;
        }
        if (rank_[lhsRoot] < rank_[rhsRoot]) {
            parent_[lhsRoot] = rhsRoot;
        } else if (rank_[lhsRoot] > rank_[rhsRoot]) {
            parent_[rhsRoot] = lhsRoot;
        } else {
            parent_[rhsRoot] = lhsRoot;
            ++rank_[lhsRoot];
        }
    }

private:
    std::vector<size_t> parent_;
    std::vector<uint8_t> rank_;
};

std::vector<MergeGroup> BuildPipeBarrierGroups(const AivLaunchContext &ctx)
{
    std::vector<AivNodeRecord> barriers;
    for (const auto &entry : ctx.taskIdToNode) {
        if (entry.second.task != nullptr && entry.second.task->taskType == AivRuntimeTaskTypeV3::PIPE_BARRIER) {
            barriers.push_back(entry.second);
        }
    }
    if (barriers.empty()) {
        return {};
    }

    std::map<uint32_t, size_t> taskIdToIndex;
    for (size_t i = 0; i < barriers.size(); ++i) {
        taskIdToIndex[barriers[i].task->taskId] = i;
    }
    DisjointSet dsu(barriers.size());
    for (size_t i = 0; i < barriers.size(); ++i) {
        dsu.Union(i, i);
        for (uint32_t peerTaskId : barriers[i].task->barrierGroupTaskIds) {
            auto peerIter = taskIdToIndex.find(peerTaskId);
            if (peerIter != taskIdToIndex.end()) {
                dsu.Union(i, peerIter->second);
            }
        }
    }

    std::map<size_t, MergeGroup> groupsByRoot;
    for (size_t i = 0; i < barriers.size(); ++i) {
        MergeGroup &group = groupsByRoot[dsu.Find(i)];
        group.memberNodeIds.push_back(barriers[i].nodeId);
        group.memberTaskIds.push_back(barriers[i].task->taskId);
        group.memberPipes.push_back(barriers[i].task->curPipe);
        group.topoOrder = group.memberNodeIds.size() == 1 ? barriers[i].order : std::min(group.topoOrder,
            barriers[i].order);
        group.blockId = barriers[i].task->blockId;
        group.pipeType = barriers[i].task->pipeType;
    }

    std::vector<MergeGroup> groups;
    groups.reserve(groupsByRoot.size());
    for (auto &entry : groupsByRoot) {
        std::sort(entry.second.memberTaskIds.begin(), entry.second.memberTaskIds.end());
        groups.push_back(std::move(entry.second));
    }
    std::sort(groups.begin(), groups.end(), [](const MergeGroup &lhs, const MergeGroup &rhs) {
        return lhs.topoOrder < rhs.topoOrder;
    });
    return groups;
}

std::vector<MergeGroup> BuildSyncAllGroups(const AivLaunchContext &ctx)
{
    std::map<uint32_t, MergeGroup> groupsByRound;
    for (const auto &entry : ctx.taskIdToNode) {
        const AivNodeRecord &record = entry.second;
        if (record.task == nullptr || record.task->taskType != AivRuntimeTaskTypeV3::SYNC_ALL) {
            continue;
        }
        MergeGroup &group = groupsByRound[record.task->syncRound];
        group.memberNodeIds.push_back(record.nodeId);
        group.memberTaskIds.push_back(record.task->taskId);
        group.memberPipes.push_back(record.task->curPipe);
        group.topoOrder = group.memberNodeIds.size() == 1 ? record.order : std::min(group.topoOrder, record.order);
        group.syncRound = record.task->syncRound;
    }

    std::vector<MergeGroup> groups;
    groups.reserve(groupsByRound.size());
    for (auto &entry : groupsByRound) {
        std::sort(entry.second.memberTaskIds.begin(), entry.second.memberTaskIds.end());
        groups.push_back(std::move(entry.second));
    }
    std::sort(groups.begin(), groups.end(), [](const MergeGroup &lhs, const MergeGroup &rhs) {
        return lhs.topoOrder < rhs.topoOrder;
    });
    return groups;
}

HcclResult MergePipeBarrierGroups(AivLaunchContext &ctx)
{
    const std::vector<MergeGroup> groups = BuildPipeBarrierGroups(ctx);
    for (const MergeGroup &group : groups) {
        if (group.memberNodeIds.empty()) {
            continue;
        }
        std::set<uint32_t> memberPipes;
        for (NodeId memberNodeId : group.memberNodeIds) {
            TaskNode *member = ctx.graph->GetNode(memberNodeId);
            const auto *barrier = dynamic_cast<const TaskAivPipeBarrier *>(member);
            if (barrier == nullptr) {
                HCCL_VM_ERROR("[AivTaskTransformV3] Invalid PipeBarrier member node, rank={}, launch={}, "
                    "nodeId={}, file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
                    memberNodeId, ctx.snapshot.filePath);
                return HCCL_E_INTERNAL;
            }
            const AivBarrierInfo &memberInfo = barrier->GetInfo();
            if (memberInfo.taskLoc.blockId != group.blockId) {
                HCCL_VM_ERROR("[AivTaskTransformV3] PipeBarrier group crosses block, rank={}, launch={}, "
                    "groupBlock={}, memberBlock={}, memberTaskId={}, file={}", ctx.placeholder->GetRankId(),
                    ctx.placeholder->GetLaunchIdx(), group.blockId, memberInfo.taskLoc.blockId,
                    memberInfo.taskLoc.taskId, ctx.snapshot.filePath);
                return HCCL_E_INTERNAL;
            }
            if (!memberPipes.insert(memberInfo.taskLoc.pipe).second) {
                HCCL_VM_ERROR("[AivTaskTransformV3] Duplicate PipeBarrier member pipe, rank={}, launch={}, "
                    "block={}, pipe={}, file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
                    group.blockId, memberInfo.taskLoc.pipe, ctx.snapshot.filePath);
                return HCCL_E_INTERNAL;
            }
        }
        if (group.pipeType == PIPE_ALL) {
            for (uint32_t pipe = 0; pipe < AIV_PIPE_NUM; ++pipe) {
                if (memberPipes.count(pipe) != 0) {
                    continue;
                }
                HCCL_VM_ERROR("[AivTaskTransformV3] Missing PIPE_ALL PipeBarrier member, rank={}, launch={}, "
                    "block={}, pipe={}, file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
                    group.blockId, pipe, ctx.snapshot.filePath);
                return HCCL_E_INTERNAL;
            }
        }
        AivBarrierInfo info;
        const uint32_t mergedTaskId = group.memberTaskIds.empty() ? std::numeric_limits<uint32_t>::max() :
            group.memberTaskIds.front();
        info.taskLoc = MakeAivPosition(ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(), group.blockId,
            std::numeric_limits<uint32_t>::max(), mergedTaskId);
        info.pipeType = group.pipeType;
        info.merged = true;
        info.memberNodeIds = group.memberNodeIds;
        info.memberTaskIds = group.memberTaskIds;
        info.memberPipes = group.memberPipes;

        NodeId mergeNodeId = INVALID_NODE_ID;
        const TaskPosition position = MakeAivPosition(ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
            group.blockId, std::numeric_limits<uint32_t>::max(), mergedTaskId);
        HcclResult ret = ctx.graph->AppendGeneratedNode(std::make_unique<TaskAivPipeBarrier>(std::move(info)),
            position, mergeNodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ctx.internalNodeIds.push_back(mergeNodeId);
        for (NodeId memberNodeId : group.memberNodeIds) {
            ret = RewireGroupMember(ctx.graph, memberNodeId, mergeNodeId);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
        }
        ++ctx.pipeBarrierMergeCount;
    }
    return HCCL_SUCCESS;
}

HcclResult MergeSyncAllGroups(AivLaunchContext &ctx)
{
    const std::vector<MergeGroup> groups = BuildSyncAllGroups(ctx);
    const size_t expectedMemberCount = ctx.snapshot.blocks.size() * AIV_PIPE_NUM;
    for (const MergeGroup &group : groups) {
        if (group.memberNodeIds.empty()) {
            continue;
        }
        if (expectedMemberCount != 0 && group.memberNodeIds.size() != expectedMemberCount) {
            HCCL_VM_ERROR("[AivTaskTransformV3] SyncAll member count mismatch, rank={}, launch={}, syncRound={}, "
                "actual={}, expected={}, file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
                group.syncRound, group.memberNodeIds.size(), expectedMemberCount, ctx.snapshot.filePath);
            return HCCL_E_INTERNAL;
        }
        std::set<std::pair<uint32_t, uint32_t>> members;
        for (NodeId memberNodeId : group.memberNodeIds) {
            TaskNode *member = ctx.graph->GetNode(memberNodeId);
            const auto *syncAll = dynamic_cast<const TaskAivSyncAll *>(member);
            if (syncAll == nullptr) {
                HCCL_VM_ERROR("[AivTaskTransformV3] Invalid SyncAll member node, rank={}, launch={}, syncRound={}, "
                    "nodeId={}, file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
                    group.syncRound, memberNodeId, ctx.snapshot.filePath);
                return HCCL_E_INTERNAL;
            }
            const TaskPosition &location = syncAll->GetInfo().taskLoc;
            if (!members.insert({location.blockId, location.pipe}).second) {
                HCCL_VM_ERROR("[AivTaskTransformV3] Duplicate SyncAll member, rank={}, launch={}, syncRound={}, "
                    "block={}, pipe={}, file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
                    group.syncRound, location.blockId, location.pipe, ctx.snapshot.filePath);
                return HCCL_E_INTERNAL;
            }
        }
        for (const auto &block : ctx.snapshot.blocks) {
            for (uint32_t pipe = 0; pipe < AIV_PIPE_NUM; ++pipe) {
                if (members.count({block.blockIdx, pipe}) != 0) {
                    continue;
                }
                HCCL_VM_ERROR("[AivTaskTransformV3] Missing SyncAll member, rank={}, launch={}, syncRound={}, "
                    "block={}, pipe={}, file={}", ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
                    group.syncRound, block.blockIdx, pipe, ctx.snapshot.filePath);
                return HCCL_E_INTERNAL;
            }
        }
        AivSyncAllInfo info;
        const uint32_t mergedTaskId = group.memberTaskIds.empty() ? std::numeric_limits<uint32_t>::max() :
            group.memberTaskIds.front();
        info.taskLoc = MakeAivPosition(ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
            std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max(), mergedTaskId);
        info.syncRound = group.syncRound;
        info.merged = true;
        info.memberNodeIds = group.memberNodeIds;
        info.memberTaskIds = group.memberTaskIds;
        info.memberPipes = group.memberPipes;

        NodeId mergeNodeId = INVALID_NODE_ID;
        const TaskPosition position = MakeAivPosition(ctx.placeholder->GetRankId(), ctx.placeholder->GetLaunchIdx(),
            std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max(), mergedTaskId);
        HcclResult ret = ctx.graph->AppendGeneratedNode(std::make_unique<TaskAivSyncAll>(std::move(info)), position,
            mergeNodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ctx.internalNodeIds.push_back(mergeNodeId);
        for (NodeId memberNodeId : group.memberNodeIds) {
            ret = RewireGroupMember(ctx.graph, memberNodeId, mergeNodeId);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
        }
        ++ctx.syncAllMergeCount;
    }
    return HCCL_SUCCESS;
}

HcclResult ConnectCurrentTailsToEnd(AivLaunchContext &ctx)
{
    if (ctx.headNodeId == INVALID_NODE_ID || ctx.endNodeId == INVALID_NODE_ID) {
        return HCCL_E_PARA;
    }

    std::set<NodeId> internalSet(ctx.internalNodeIds.begin(), ctx.internalNodeIds.end());
    std::set<NodeId> reachableInternalNodes;
    std::queue<NodeId> nodeQueue;
    nodeQueue.push(ctx.headNodeId);
    reachableInternalNodes.insert(ctx.headNodeId);
    while (!nodeQueue.empty()) {
        const NodeId nodeId = nodeQueue.front();
        nodeQueue.pop();
        TaskNode *node = ctx.graph->GetNode(nodeId);
        if (node == nullptr) {
            return HCCL_E_PTR;
        }
        for (TaskNode *child : node->GetChildren()) {
            if (child == nullptr) {
                return HCCL_E_PTR;
            }
            const NodeId childId = child->GetNodeId();
            if (internalSet.count(childId) == 0 || reachableInternalNodes.count(childId) != 0) {
                continue;
            }
            reachableInternalNodes.insert(childId);
            nodeQueue.push(childId);
        }
    }

    std::set<NodeId> tails;
    for (NodeId nodeId : reachableInternalNodes) {
        TaskNode *node = ctx.graph->GetNode(nodeId);
        if (node == nullptr) {
            return HCCL_E_PTR;
        }
        if (node->GetType() == TaskType::START || node->GetType() == TaskType::END) {
            continue;
        }
        bool hasInternalChild = false;
        for (TaskNode *child : node->GetChildren()) {
            if (child != nullptr && internalSet.count(child->GetNodeId()) != 0) {
                hasInternalChild = true;
                break;
            }
        }
        if (!hasInternalChild) {
            tails.insert(nodeId);
        }
    }

    if (tails.empty()) {
        return AddEdge(ctx.graph, ctx.headNodeId, ctx.endNodeId);
    }
    for (NodeId tailNodeId : tails) {
        HcclResult ret = AddEdge(ctx.graph, tailNodeId, ctx.endNodeId);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ReconnectPlaceholder(AivLaunchContext &ctx)
{
    TaskNode *placeholder = ctx.graph->GetNode(ctx.placeholder->GetNodeId());
    if (placeholder == nullptr) {
        return HCCL_E_PTR;
    }
    std::vector<TaskNode *> originalChildren = placeholder->GetChildren();
    HcclResult ret = AddEdge(ctx.graph, placeholder->GetNodeId(), ctx.headNodeId);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }

    for (TaskNode *child : originalChildren) {
        if (child == nullptr || child->GetNodeId() == ctx.headNodeId) {
            continue;
        }
        ret = ctx.graph->RemoveEdge(placeholder->GetNodeId(), child->GetNodeId());
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = AddEdge(ctx.graph, ctx.endNodeId, child->GetNodeId());
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ExpandOneAivGraph(TaskGraphGeneratorV3 *graph, StorageManager *storage, TaskAivGraph *aivGraph,
    AivGraphGenerateOutputV3 &output)
{
    if (graph == nullptr || storage == nullptr || aivGraph == nullptr) {
        return HCCL_E_PTR;
    }

    AivSnapshotJsonLoaderV3 loader;
    AivLaunchContext ctx;
    ctx.graph = graph;
    ctx.storage = storage;
    ctx.placeholder = aivGraph;
    std::string errorMessage;
    HcclResult ret = loader.LoadByRankAndLaunch(aivGraph->GetRankId(), aivGraph->GetLaunchIdx(), ctx.snapshot,
        errorMessage);
    if (ret != HCCL_SUCCESS) {
        HCCL_VM_ERROR("[AivTaskTransformV3] Load AIV snapshot failed, rank={}, launch={}, ret={}, err={}",
            aivGraph->GetRankId(), aivGraph->GetLaunchIdx(), static_cast<uint32_t>(ret), errorMessage);
        return ret;
    }
    ret = ValidateSnapshot(ctx);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ret = UpdateAivBufferSize(ctx.snapshot.ubBufferSize, "ubBufferSize", ctx, g_checkerAivUbBufferSize);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ret = UpdateAivBufferSize(ctx.snapshot.flagBufferSize, "flagBufferSize", ctx, g_checkerAivFlagBufferSize);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }

    const TaskPosition basePosition = aivGraph->GetPosition();
    ret = graph->AppendGeneratedNode(std::make_unique<TaskStart>(BoundaryType::AIV_SUB_GRAPH), basePosition,
        ctx.headNodeId);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ctx.internalNodeIds.push_back(ctx.headNodeId);

    std::vector<NodeId> tailNodes;
    ret = BuildPipeSkeleton(ctx, tailNodes);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ret = AddSetWaitFlagEdges(ctx);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ret = MergePipeBarrierGroups(ctx);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ret = MergeSyncAllGroups(ctx);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }

    ret = graph->AppendGeneratedNode(std::make_unique<TaskEnd>(BoundaryType::AIV_SUB_GRAPH), basePosition,
        ctx.endNodeId);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ctx.internalNodeIds.push_back(ctx.endNodeId);
    ret = ConnectCurrentTailsToEnd(ctx);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ret = ReconnectPlaceholder(ctx);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }

    output.internalNodeCount += ctx.internalNodeIds.size();
    output.setWaitEdgeCount += ctx.setWaitEdgeCount;
    output.pipeBarrierMergeCount += ctx.pipeBarrierMergeCount;
    output.syncAllMergeCount += ctx.syncAllMergeCount;
    return HCCL_SUCCESS;
}

bool IsExecutableForFlagMatch(const TaskGraphGeneratorV3 *graph, NodeId nodeId, const std::set<NodeId> &executed)
{
    if (graph == nullptr) {
        return false;
    }
    const TaskNode *node = graph->GetNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    for (const TaskNode *parent : node->GetParents()) {
        if (parent == nullptr) {
            return false;
        }
        const NodeId parentId = parent->GetNodeId();
        if (parentId == MAIN_START_NODE_ID) {
            continue;
        }
        if (executed.count(parentId) == 0) {
            return false;
        }
    }
    return true;
}

HcclResult EnqueueChildrenForFlagMatch(const TaskGraphGeneratorV3 *graph, NodeId nodeId,
    std::deque<NodeId> &readyQueue, std::set<NodeId> &queued)
{
    const TaskNode *node = graph->GetNode(nodeId);
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    for (const TaskNode *child : node->GetChildren()) {
        if (child == nullptr) {
            return HCCL_E_PTR;
        }
        const NodeId childId = child->GetNodeId();
        if (queued.insert(childId).second) {
            readyQueue.push_back(childId);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult MatchSendRecvFlagEdges(TaskGraphGeneratorV3 *graph, size_t &matchedEdgeCount)
{
    matchedEdgeCount = 0;
    if (graph == nullptr || graph->GetMainStartNode() == nullptr) {
        return HCCL_E_PTR;
    }

    std::map<FlagCellKey, FlagCellState> flagStates;
    std::deque<NodeId> readyQueue;
    std::set<NodeId> queued;
    std::set<NodeId> executed;

    for (const TaskNode *child : graph->GetMainStartNode()->GetChildren()) {
        if (child == nullptr) {
            return HCCL_E_PTR;
        }
        const NodeId childId = child->GetNodeId();
        queued.insert(childId);
        readyQueue.push_back(childId);
    }

    size_t unmatchedCnt = 0;
    while (!readyQueue.empty()) {
        if (unmatchedCnt >= readyQueue.size()) {
            const TaskNode *node = graph->GetNode(readyQueue.front());
            HCCL_VM_ERROR("[AivTaskTransformV3] Send/RecvFlag matching deadlock, firstUnmatched={}",
                node == nullptr ? "null" : node->Describe());
            return HCCL_E_INTERNAL;
        }

        const NodeId nodeId = readyQueue.front();
        readyQueue.pop_front();
        queued.erase(nodeId);
        if (executed.count(nodeId) != 0) {
            unmatchedCnt = 0;
            continue;
        }
        if (!IsExecutableForFlagMatch(graph, nodeId, executed)) {
            readyQueue.push_back(nodeId);
            queued.insert(nodeId);
            ++unmatchedCnt;
            continue;
        }

        TaskNode *node = graph->GetNode(nodeId);
        if (node == nullptr) {
            return HCCL_E_PTR;
        }
        if (node->GetType() == TaskType::AIV_SEND_FLAG) {
            const auto *sendFlag = dynamic_cast<const TaskAivSendFlag *>(node);
            if (sendFlag == nullptr) {
                return HCCL_E_PTR;
            }
            FlagCellState &state = flagStates[MakeFlagCellKey(sendFlag->GetFlag())];
            if (state.currentValue == sendFlag->GetFlag().value && !state.producerNodes.empty()) {
                HCCL_VM_WARN("[AivTaskTransformV3] Repeated SendFlag value on same flag cell, node={}",
                    node->Describe());
            }
            state.currentValue = sendFlag->GetFlag().value;
            state.producerNodes.clear();
            state.producerNodes.push_back(nodeId);
        } else if (node->GetType() == TaskType::AIV_RECV_FLAG) {
            const auto *recvFlag = dynamic_cast<const TaskAivRecvFlag *>(node);
            if (recvFlag == nullptr) {
                return HCCL_E_PTR;
            }
            FlagCellState &state = flagStates[MakeFlagCellKey(recvFlag->GetFlag())];
            if (state.currentValue != recvFlag->GetFlag().value) {
                readyQueue.push_back(nodeId);
                queued.insert(nodeId);
                ++unmatchedCnt;
                continue;
            }
            for (NodeId producerNodeId : state.producerNodes) {
                HcclResult ret = AddEdge(graph, producerNodeId, nodeId);
                if (ret != HCCL_SUCCESS) {
                    return ret;
                }
                ++matchedEdgeCount;
            }
        }

        executed.insert(nodeId);
        HcclResult ret = EnqueueChildrenForFlagMatch(graph, nodeId, readyQueue, queued);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        unmatchedCnt = 0;
    }
    return HCCL_SUCCESS;
}
} // namespace

HcclResult ExpandAivGraphsV3(const AivGraphsGenerateInputV3 &input, AivGraphGenerateOutputV3 &output)
{
    const auto start = AivExpandClock::now();
    output = AivGraphGenerateOutputV3 {};
    if (input.graph == nullptr || input.storage == nullptr) {
        return HCCL_E_PTR;
    }

    for (TaskAivGraph *aivGraph : input.aivGraphs) {
        HcclResult ret = ExpandOneAivGraph(input.graph, input.storage, aivGraph, output);
        if (ret != HCCL_SUCCESS) {
            output.expandNs = ElapsedNs(start, AivExpandClock::now());
            return ret;
        }
    }

    HcclResult ret = MatchSendRecvFlagEdges(input.graph, output.sendRecvEdgeCount);
    output.expandNs = ElapsedNs(start, AivExpandClock::now());
    return ret;
}

} // namespace TaskGraphGeneratorV3
} // namespace HcclSim
