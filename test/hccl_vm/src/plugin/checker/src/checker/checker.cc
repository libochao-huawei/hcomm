/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "checker.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ccu_task_common.h"
#include "check_rank_mem.h"
#include "check_utils.h"
#include "checker_def.h"
#include "dump/dump_graph.h"
#include "dump/validation_issue_recorder.h"
#include "dump_v3/dump_v3_dag.h"
#include "framework/task_graph_generator_v3/task_graph_generator_v3.h"
#include "framework/task_graph_generator_v3/task_graph_mem_conflict_v3.h"
#include "framework/task_graph_generator_v3/task_graph_semantic_check_v3.h"
#include "framework/task_graph_generator_v3/task_graph_single_task_check_v3.h"
#include "framework/task_graph_generator_v3/task_meta_translator_v3.h"
#include "sim_log.h"
#include "mem_conflict_check_utils.h"
#include "sim_task.h"
#include "singletask_check.h"
#include "storage_manager.h"
#include "task_ccu.h"
#include "task_check_op_semantics.h"
#include "task_def.h"
#include "task_graph_generator.h"
#include "task_graph_revamp.h"
#include "task_graph_revamp_bilateral_ccu.h"
#include "task_graph_revamp_parallel.h"
#include "task_utils.h"

using namespace std;

namespace HcclSim {
namespace {
using V3TaskNode = TaskGraphGeneratorV3::TaskNode;
using OldNodeStreamPosMap = std::map<TaskNodePtr, uint32_t>;
using V3NodeStreamPosMap = std::map<const V3TaskNode *, uint32_t>;
using V3StageClock = std::chrono::steady_clock;
constexpr size_t MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT = 20;
constexpr uint32_t INVALID_GRAPH_STREAM_POS = std::numeric_limits<uint32_t>::max();
constexpr uint64_t V3_NS_PER_MS = 1000000ULL;

uint64_t ElapsedV3Ns(V3StageClock::time_point start, V3StageClock::time_point end)
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

constexpr bool ENABLE_V3_MEM_CONFLICT_CHECK = true;
constexpr bool ENABLE_V3_SEMANTIC_CHECK = true;

class V3StageTimer {
public:
    explicit V3StageTimer(const char *stage) : stage_(stage), start_(V3StageClock::now()) {}

    ~V3StageTimer()
    {
        const uint64_t elapsedNs = ElapsedV3Ns(start_, V3StageClock::now());
        if (extraInfo_.empty()) {
            HCCL_VM_WARN("[TaskGraphGeneratorV3][StageTime] {} {}, costMs={}", stage_, status_,
                elapsedNs / V3_NS_PER_MS);
            return;
        }
        HCCL_VM_WARN("[TaskGraphGeneratorV3][StageTime] {} {}, costMs={}, {}", stage_, status_,
            elapsedNs / V3_NS_PER_MS, extraInfo_);
    }

    void SetStatus(const char *status)
    {
        status_ = status;
    }

    void SetExtraInfo(const std::string &extraInfo)
    {
        extraInfo_ = extraInfo;
    }

private:
    const char *stage_{nullptr};
    const V3StageClock::time_point start_;
    const char *status_{"failed"};
    std::string extraInfo_;
};

struct V3StreamKey {
    uint32_t rankId{TaskGraphGeneratorV3::INVALID_RANK_ID};
    uint32_t streamId{TaskGraphGeneratorV3::INVALID_STREAM_ID};

    bool operator<(const V3StreamKey &rhs) const
    {
        if (rankId != rhs.rankId) {
            return rankId < rhs.rankId;
        }
        return streamId < rhs.streamId;
    }
};

bool IsOldGraphCompareTransparent(TaskNodePtr node)
{
    if (node == nullptr || node->task == nullptr) {
        return false;
    }
    const TaskTypeStub taskType = node->task->GetType();
    return taskType == TaskTypeStub::GRAPH_SEPARATE || taskType == TaskTypeStub::SUB_GRAPH_END;
}

bool IsV3Boundary(const V3TaskNode *node, TaskGraphGeneratorV3::BoundaryType boundaryType)
{
    if (node == nullptr) {
        return false;
    }
    if (node->GetType() == TaskGraphGeneratorV3::TaskType::START) {
        auto *start = dynamic_cast<const TaskGraphGeneratorV3::TaskStart *>(node);
        return start != nullptr && start->GetBoundaryType() == boundaryType;
    }
    if (node->GetType() == TaskGraphGeneratorV3::TaskType::END) {
        auto *end = dynamic_cast<const TaskGraphGeneratorV3::TaskEnd *>(node);
        return end != nullptr && end->GetBoundaryType() == boundaryType;
    }
    return false;
}

bool IsV3GraphCompareTransparent(const V3TaskNode *node)
{
    if (node == nullptr) {
        return false;
    }
    if (node->GetPosition().queueId != TaskGraphGeneratorV3::INVALID_QUEUE_ID) {
        return true;
    }
    return IsV3Boundary(node, TaskGraphGeneratorV3::BoundaryType::CCU_SUB_GRAPH) ||
        IsV3Boundary(node, TaskGraphGeneratorV3::BoundaryType::AIV_SUB_GRAPH);
}

bool IsAivTaskType(TaskGraphGeneratorV3::TaskType type)
{
    switch (type) {
        case TaskGraphGeneratorV3::TaskType::AIV_GRAPH:
        case TaskGraphGeneratorV3::TaskType::AIV_SET_FLAG:
        case TaskGraphGeneratorV3::TaskType::AIV_WAIT_FLAG:
        case TaskGraphGeneratorV3::TaskType::AIV_PIPE_BARRIER:
        case TaskGraphGeneratorV3::TaskType::AIV_SYNC_ALL:
        case TaskGraphGeneratorV3::TaskType::AIV_SEND_FLAG:
        case TaskGraphGeneratorV3::TaskType::AIV_RECV_FLAG:
            return true;
        default:
            return false;
    }
}

bool IsAivDataTaskType(TaskGraphGeneratorV3::TaskType type)
{
    switch (type) {
        case TaskGraphGeneratorV3::TaskType::TRANS_MEM:
        case TaskGraphGeneratorV3::TaskType::BATCH_TRANS_MEM:
        case TaskGraphGeneratorV3::TaskType::REDUCE:
        case TaskGraphGeneratorV3::TaskType::BATCH_REDUCE:
            return true;
        default:
            return false;
    }
}

bool IsAivSyntheticDataNode(const V3TaskNode *node)
{
    if (node == nullptr || !IsAivDataTaskType(node->GetType()) || node->HasCcuTrace()) {
        return false;
    }
    const auto &loc = node->GetPosition();
    return loc.streamId == TaskGraphGeneratorV3::INVALID_STREAM_ID &&
        loc.queueId != TaskGraphGeneratorV3::INVALID_QUEUE_ID;
}

bool IsAivDagLogNode(const V3TaskNode *node)
{
    if (node == nullptr) {
        return false;
    }
    return IsAivTaskType(node->GetType()) || IsAivSyntheticDataNode(node) ||
        IsV3Boundary(node, TaskGraphGeneratorV3::BoundaryType::AIV_SUB_GRAPH);
}

std::vector<TaskNodePtr> BuildOldGraphCompareNodes(const std::vector<TaskNodePtr> &oldBfsNodes)
{
    std::vector<TaskNodePtr> result;
    result.reserve(oldBfsNodes.size());
    for (TaskNodePtr node : oldBfsNodes) {
        if (IsOldGraphCompareTransparent(node)) {
            continue;
        }
        result.push_back(node);
    }
    return result;
}

std::vector<const V3TaskNode *> BuildV3GraphCompareNodes(const std::vector<const V3TaskNode *> &newBfsNodes)
{
    std::vector<const V3TaskNode *> result;
    result.reserve(newBfsNodes.size());
    for (const V3TaskNode *node : newBfsNodes) {
        if (IsV3GraphCompareTransparent(node)) {
            continue;
        }
        result.push_back(node);
    }
    return result;
}

std::string EscapeGraphvizLabel(const std::string &label)
{
    std::string escaped;
    escaped.reserve(label.size());
    for (char ch : label) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

std::string FormatGraphvizLabelText(const std::string &text)
{
    std::string label = text;
    std::replace(label.begin(), label.end(), '|', '\n');
    return label;
}

std::unique_ptr<TaskGraphGeneratorV3::TaskGraphGeneratorV3> GenGraphV3FromTaskMeta()
{
    V3StageTimer timer("GenGraphV3FromTaskMeta");
    StorageManager &storage = StorageManager::GetInstance();
    TaskGraphGeneratorV3::TaskMetaTranslatorV3 taskMetaTranslatorV3;
    // V3 主流程入口：
    // 1. 先把 task meta 翻译成中间节点；
    // 2. 做一次从流合法性检查；
    // 3. 再生成 V3 图并在日志里记录成图/CCU 展开耗时。
    HcclResult ret = taskMetaTranslatorV3.Translate(storage);
    if (ret != HCCL_SUCCESS) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Translate task meta failed, skip V3 graph generation, ret={}",
            static_cast<uint32_t>(ret));
        return nullptr;
    }

    auto translatedNodes = taskMetaTranslatorV3.TakeNodes();
    auto translatedTaskQueues = taskMetaTranslatorV3.TakeTaskQueues();
    ret = TaskGraphGeneratorV3::CheckSlaveTaskQueue(translatedNodes, translatedTaskQueues);
    if (ret != HCCL_SUCCESS) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Check slave task queue failed, skip V3 graph generation, ret={}",
            static_cast<uint32_t>(ret));
        return nullptr;
    }
    std::unique_ptr<TaskGraphGeneratorV3::TaskGraphGeneratorV3> graphGeneratorV3(
        new (std::nothrow) TaskGraphGeneratorV3::TaskGraphGeneratorV3);
    if (graphGeneratorV3 == nullptr) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Create graph generator failed.");
        return nullptr;
    }

    ret = graphGeneratorV3->GenGraph(std::move(translatedNodes), std::move(translatedTaskQueues));
    if (ret != HCCL_SUCCESS) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Generate graph from translated task nodes failed, ret={}",
            static_cast<uint32_t>(ret));
        return nullptr;
    }
    const auto &ccuExpandStats = graphGeneratorV3->GetCcuExpandStats();
    const auto &aivExpandStats = graphGeneratorV3->GetAivExpandStats();
    std::ostringstream timerExtraInfo;
    timerExtraInfo << "taskNodeCount=" << graphGeneratorV3->GetNodes().size()
                   << ", mainStartNodeId=" << graphGeneratorV3->GetMainStartNodeId()
                   << ", aivExpandTotalMs=" << (aivExpandStats.totalExpandNs / V3_NS_PER_MS)
                   << ", aivGraphCount=" << aivExpandStats.graphCount
                   << ", ccuExpandTotalMs=" << (ccuExpandStats.totalExpandNs / V3_NS_PER_MS)
                   << ", ccuGraphCount=" << ccuExpandStats.graphCount;
    timer.SetExtraInfo(timerExtraInfo.str());
    timer.SetStatus("success");
    return graphGeneratorV3;
}

std::vector<TaskNodePtr> BfsOldGraph(TaskNodePtr start)
{
    std::vector<TaskNodePtr> result;
    if (start == nullptr) {
        return result;
    }

    std::queue<TaskNodePtr> nodeQue;
    std::set<TaskNodePtr> visited;
    nodeQue.push(start);
    visited.insert(start);

    while (!nodeQue.empty()) {
        TaskNodePtr currNode = nodeQue.front();
        nodeQue.pop();
        result.push_back(currNode);

        for (TaskNodePtr child : currNode->children) {
            if (child == nullptr || visited.count(child) != 0) {
                continue;
            }
            visited.insert(child);
            nodeQue.push(child);
        }
    }

    return result;
}

std::vector<const V3TaskNode *> BfsGraphV3(const V3TaskNode *start)
{
    std::vector<const V3TaskNode *> result;
    if (start == nullptr) {
        return result;
    }

    std::queue<const V3TaskNode *> nodeQue;
    std::set<const V3TaskNode *> visited;
    nodeQue.push(start);
    visited.insert(start);

    while (!nodeQue.empty()) {
        const V3TaskNode *currNode = nodeQue.front();
        nodeQue.pop();
        result.push_back(currNode);

        for (const V3TaskNode *childNode : currNode->GetChildren()) {
            if (childNode == nullptr || visited.count(childNode) != 0) {
                continue;
            }
            visited.insert(childNode);
            nodeQue.push(childNode);
        }
    }

    return result;
}

OldNodeStreamPosMap BuildOldStreamPosMap(const std::vector<TaskNodePtr> &bfsNodes)
{
    std::map<V3StreamKey, std::vector<TaskNodePtr>> nodesByStream;
    for (TaskNodePtr node : bfsNodes) {
        if (node == nullptr || node->task == nullptr || IsOldGraphCompareTransparent(node)) {
            continue;
        }
        V3StreamKey key;
        key.rankId = node->rankIdx;
        key.streamId = node->queIdx;
        nodesByStream[key].push_back(node);
    }

    OldNodeStreamPosMap streamPosByNode;
    for (auto &streamItem : nodesByStream) {
        auto &streamNodes = streamItem.second;
        std::sort(streamNodes.begin(), streamNodes.end(), [](TaskNodePtr lhs, TaskNodePtr rhs) {
            if (lhs == rhs) {
                return false;
            }
            if (lhs == nullptr || rhs == nullptr) {
                return lhs < rhs;
            }
            if (lhs->pos != rhs->pos) {
                return lhs->pos < rhs->pos;
            }
            return lhs < rhs;
        });

        for (size_t pos = 0; pos < streamNodes.size(); ++pos) {
            streamPosByNode[streamNodes[pos]] = static_cast<uint32_t>(pos);
        }
    }
    return streamPosByNode;
}

V3NodeStreamPosMap BuildV3StreamPosMap(const std::vector<const V3TaskNode *> &bfsNodes)
{
    std::map<V3StreamKey, std::vector<const V3TaskNode *>> nodesByStream;
    for (const V3TaskNode *node : bfsNodes) {
        if (node == nullptr || node->GetType() == TaskGraphGeneratorV3::TaskType::START ||
            IsV3GraphCompareTransparent(node)) {
            continue;
        }
        const auto &loc = node->GetPosition();
        V3StreamKey key;
        key.rankId = loc.rankId;
        key.streamId = loc.streamId;
        nodesByStream[key].push_back(node);
    }

    V3NodeStreamPosMap streamPosByNode;
    for (auto &streamItem : nodesByStream) {
        auto &streamNodes = streamItem.second;
        std::sort(streamNodes.begin(), streamNodes.end(), [](const V3TaskNode *lhs, const V3TaskNode *rhs) {
            if (lhs == rhs) {
                return false;
            }
            if (lhs->GetNodeId() != rhs->GetNodeId()) {
                return lhs->GetNodeId() < rhs->GetNodeId();
            }
            return std::less<const V3TaskNode *>()(lhs, rhs);
        });

        for (size_t pos = 0; pos < streamNodes.size(); ++pos) {
            streamPosByNode[streamNodes[pos]] = static_cast<uint32_t>(pos);
        }
    }
    return streamPosByNode;
}

const char *TaskTypeKeyName(TaskGraphGeneratorV3::TaskType type)
{
    switch (type) {
        case TaskGraphGeneratorV3::TaskType::TRANS_MEM:
            return "TRANS_MEM";
        case TaskGraphGeneratorV3::TaskType::BATCH_TRANS_MEM:
            return "BATCH_TRANS_MEM";
        case TaskGraphGeneratorV3::TaskType::REDUCE:
            return "REDUCE";
        case TaskGraphGeneratorV3::TaskType::BATCH_REDUCE:
            return "BATCH_REDUCE";
        case TaskGraphGeneratorV3::TaskType::RECORD:
            return "RECORD";
        case TaskGraphGeneratorV3::TaskType::WAIT:
            return "WAIT";
        case TaskGraphGeneratorV3::TaskType::CCU_GRAPH:
            return "CCU_GRAPH";
        case TaskGraphGeneratorV3::TaskType::AIV_GRAPH:
            return "AIV_GRAPH";
        case TaskGraphGeneratorV3::TaskType::AIV_SET_FLAG:
            return "AIV_SET_FLAG";
        case TaskGraphGeneratorV3::TaskType::AIV_WAIT_FLAG:
            return "AIV_WAIT_FLAG";
        case TaskGraphGeneratorV3::TaskType::AIV_PIPE_BARRIER:
            return "AIV_PIPE_BARRIER";
        case TaskGraphGeneratorV3::TaskType::AIV_SYNC_ALL:
            return "AIV_SYNC_ALL";
        case TaskGraphGeneratorV3::TaskType::AIV_SEND_FLAG:
            return "AIV_SEND_FLAG";
        case TaskGraphGeneratorV3::TaskType::AIV_RECV_FLAG:
            return "AIV_RECV_FLAG";
        case TaskGraphGeneratorV3::TaskType::START:
            return "START";
        case TaskGraphGeneratorV3::TaskType::END:
            return "END";
        default:
            return "INVALID";
    }
}

TaskGraphGeneratorV3::MemType ConvertOldMemTypeToV3(BufferType bufferType)
{
    switch (bufferType) {
        case ::INPUT:
            return TaskGraphGeneratorV3::MemType::INPUT;
        case ::OUTPUT:
            return TaskGraphGeneratorV3::MemType::OUTPUT;
        case ::CCL:
        case ::SCRATCH:
            return TaskGraphGeneratorV3::MemType::CCL;
        case ::MS:
            return TaskGraphGeneratorV3::MemType::MS_CCU;
        case ::INPUT_AIV:
        case ::OUTPUT_AIV:
        case ::AIV_COMMINFO:
        case ::USERBUF_AIV:
            return TaskGraphGeneratorV3::MemType::UB_AIV;
        default:
            return TaskGraphGeneratorV3::MemType::INVALID;
    }
}

TaskGraphGeneratorV3::ProtocolType ConvertOldProtocolToV3(LinkProtoStub protocol)
{
    if (protocol == LinkProtoStub::RDMA) {
        return TaskGraphGeneratorV3::ProtocolType::RDMA;
    }
    if (protocol == LinkProtoStub::SDMA) {
        return TaskGraphGeneratorV3::ProtocolType::SDMA;
    }
    if (protocol == LinkProtoStub::CCU) {
        return TaskGraphGeneratorV3::ProtocolType::CCU;
    }
    return TaskGraphGeneratorV3::ProtocolType::INVALID;
}

TaskGraphGeneratorV3::TaskType ConvertOldTaskTypeToV3(TaskTypeStub oldType)
{
    switch (oldType) {
        case TaskTypeStub::LOCAL_COPY:
        case TaskTypeStub::READ:
        case TaskTypeStub::WRITE:
            return TaskGraphGeneratorV3::TaskType::TRANS_MEM;
        case TaskTypeStub::LOCAL_REDUCE:
        case TaskTypeStub::LOCAL_BATCH_REDUCE:
        case TaskTypeStub::READ_REDUCE:
        case TaskTypeStub::WRITE_REDUCE:
            return TaskGraphGeneratorV3::TaskType::REDUCE;
        case TaskTypeStub::LOCAL_POST_TO:
        case TaskTypeStub::POST:
            return TaskGraphGeneratorV3::TaskType::RECORD;
        case TaskTypeStub::LOCAL_WAIT_FROM:
        case TaskTypeStub::WAIT:
            return TaskGraphGeneratorV3::TaskType::WAIT;
        case TaskTypeStub::CCU_GRAPH:
            return TaskGraphGeneratorV3::TaskType::CCU_GRAPH;
        default:
            return TaskGraphGeneratorV3::TaskType::INVALID;
    }
}

std::string MakePositionKey(uint32_t rankId, uint32_t streamId, uint32_t streamPos)
{
    std::ostringstream os;
    os << "pos={rank=" << rankId << ",stream=" << streamId << ",index=" << streamPos << "}";
    return os.str();
}

std::string MakeProtocolKey(TaskGraphGeneratorV3::ProtocolType protocol)
{
    std::ostringstream os;
    os << "protocol=" << static_cast<uint32_t>(protocol);
    return os.str();
}

std::string MakeMemSliceKey(RankId rankId, TaskGraphGeneratorV3::MemType memType, uint64_t offset, uint64_t len)
{
    std::ostringstream os;
    os << "{rank=" << rankId << ",mem=" << static_cast<uint32_t>(memType)
       << ",offset=0x" << std::hex << offset << ",len=0x" << len << std::dec << "}";
    return os.str();
}

std::string MakeMemSliceKey(RankId rankId, const DataSlice &slice)
{
    return MakeMemSliceKey(rankId, ConvertOldMemTypeToV3(slice.GetType()), slice.GetOffset(), slice.GetSize());
}

std::string MakeMemSliceKey(const TaskGraphGeneratorV3::MemSlice &slice)
{
    return MakeMemSliceKey(slice.rankId, slice.memType, slice.offset, slice.len);
}

std::string MakeNotifyKey(const TaskGraphGeneratorV3::AicpuNotify &notify)
{
    std::ostringstream os;
    os << "{recordRank=" << notify.recordRankId << ",waitRank=" << notify.waitRankId
       << ",notifyId=" << notify.notifyId  << "}";
    return os.str();
}

std::string MakeNotifyKey(RankId recordRankId, RankId waitRankId, uint64_t notifyId)
{
    TaskGraphGeneratorV3::AicpuNotify notify;
    notify.recordRankId = recordRankId;
    notify.waitRankId = waitRankId;
    notify.notifyId = static_cast<uint32_t>(notifyId);
    return MakeNotifyKey(notify);
}

bool MakeOldNodeSemanticKey(TaskNodePtr oldNode, const OldNodeStreamPosMap *streamPosByNode, std::string &key)
{
    if (oldNode == nullptr) {
        return false;
    }

    if (oldNode->task == nullptr) {
        key = "type=START|boundary=MAIN_GRAPH";
        return true;
    }

    const TaskGraphGeneratorV3::TaskType newType = ConvertOldTaskTypeToV3(oldNode->task->GetType());
    if (newType == TaskGraphGeneratorV3::TaskType::INVALID) {
        return false;
    }

    uint32_t streamPos = oldNode->pos;
    if (streamPosByNode != nullptr) {
        const auto posIter = streamPosByNode->find(oldNode);
        streamPos = (posIter == streamPosByNode->end()) ? INVALID_GRAPH_STREAM_POS : posIter->second;
    }

    std::ostringstream os;
    os << "type=" << TaskTypeKeyName(newType) << "|" << MakePositionKey(oldNode->rankIdx, oldNode->queIdx,
        streamPos);

    switch (oldNode->task->GetType()) {
        case TaskTypeStub::LOCAL_COPY: {
            auto *copy = dynamic_cast<TaskStubLocalCopy *>(oldNode->task);
            if (copy == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(oldNode->rankIdx, copy->GetSrcSlice())
               << "|dst=" << MakeMemSliceKey(oldNode->rankIdx, copy->GetDstSlice())
               << "|" << MakeProtocolKey(TaskGraphGeneratorV3::ProtocolType::SDMA);
            break;
        }
        case TaskTypeStub::READ: {
            auto *read = dynamic_cast<TaskStubRead *>(oldNode->task);
            if (read == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(read->GetRemoteRank(), read->GetRemoteSlice())
               << "|dst=" << MakeMemSliceKey(oldNode->rankIdx, read->GetLocalSlice())
               << "|" << MakeProtocolKey(ConvertOldProtocolToV3(read->GetLinkType()));
            break;
        }
        case TaskTypeStub::WRITE: {
            auto *write = dynamic_cast<TaskStubWrite *>(oldNode->task);
            if (write == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(oldNode->rankIdx, write->GetLocalSlice())
               << "|dst=" << MakeMemSliceKey(write->GetRemoteRank(), write->GetRemoteSlice())
               << "|" << MakeProtocolKey(ConvertOldProtocolToV3(write->GetLinkType()));
            break;
        }
        case TaskTypeStub::LOCAL_REDUCE: {
            auto *reduce = dynamic_cast<TaskStubLocalReduce *>(oldNode->task);
            if (reduce == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(oldNode->rankIdx, reduce->GetSrcSlice())
               << "|dst=" << MakeMemSliceKey(oldNode->rankIdx, reduce->GetDstSlice())
               << "|dataType=" << static_cast<uint32_t>(reduce->GetDataType())
               << "|reduceOp=" << static_cast<uint32_t>(reduce->GetReduceOp())
               << "|" << MakeProtocolKey(TaskGraphGeneratorV3::ProtocolType::SDMA);
            break;
        }
        case TaskTypeStub::LOCAL_BATCH_REDUCE: {
            auto *reduce = dynamic_cast<TaskStubLocalBatchReduce *>(oldNode->task);
            if (reduce == nullptr || reduce->GetSrcSlices().empty()) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(oldNode->rankIdx, reduce->GetSrcSlice(0))
               << "|dst=" << MakeMemSliceKey(oldNode->rankIdx, reduce->GetDstSlice())
               << "|dataType=" << static_cast<uint32_t>(reduce->GetDataType())
               << "|reduceOp=" << static_cast<uint32_t>(reduce->GetReduceOp())
               << "|" << MakeProtocolKey(TaskGraphGeneratorV3::ProtocolType::SDMA);
            break;
        }
        case TaskTypeStub::READ_REDUCE: {
            auto *reduce = dynamic_cast<TaskStubReadReduce *>(oldNode->task);
            if (reduce == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(reduce->GetRemoteRank(), reduce->GetRemoteSlice())
               << "|dst=" << MakeMemSliceKey(oldNode->rankIdx, reduce->GetLocalSlice())
               << "|dataType=" << static_cast<uint32_t>(reduce->GetDataType())
               << "|reduceOp=" << static_cast<uint32_t>(reduce->GetReduceOp())
               << "|" << MakeProtocolKey(ConvertOldProtocolToV3(reduce->GetLinkType()));
            break;
        }
        case TaskTypeStub::WRITE_REDUCE: {
            auto *reduce = dynamic_cast<TaskStubWriteReduce *>(oldNode->task);
            if (reduce == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(oldNode->rankIdx, reduce->GetLocalSlice())
               << "|dst=" << MakeMemSliceKey(reduce->GetRemoteRank(), reduce->GetRemoteSlice())
               << "|dataType=" << static_cast<uint32_t>(reduce->GetDataType())
               << "|reduceOp=" << static_cast<uint32_t>(reduce->GetReduceOp())
               << "|" << MakeProtocolKey(ConvertOldProtocolToV3(reduce->GetLinkType()));
            break;
        }
        case TaskTypeStub::LOCAL_POST_TO: {
            auto *post = dynamic_cast<TaskStubLocalPostTo *>(oldNode->task);
            if (post == nullptr) {
                return false;
            }
            os << "|notify=" << MakeNotifyKey(oldNode->rankIdx, oldNode->rankIdx, post->GetNotifyId())
               << "|" << MakeProtocolKey(TaskGraphGeneratorV3::ProtocolType::INVALID);
            break;
        }
        case TaskTypeStub::LOCAL_WAIT_FROM: {
            auto *wait = dynamic_cast<TaskStubLocalWaitFrom *>(oldNode->task);
            if (wait == nullptr) {
                return false;
            }
            os << "|notify=" << MakeNotifyKey(oldNode->rankIdx, oldNode->rankIdx, wait->GetNotifyId())
               << "|" << MakeProtocolKey(TaskGraphGeneratorV3::ProtocolType::INVALID);
            break;
        }
        case TaskTypeStub::POST: {
            auto *post = dynamic_cast<TaskStubPost *>(oldNode->task);
            if (post == nullptr) {
                return false;
            }
            os << "|notify=" << MakeNotifyKey(oldNode->rankIdx, post->GetRemoteRank(), post->GetNotifyId())
               << "|" << MakeProtocolKey(ConvertOldProtocolToV3(post->GetLinkType()));
            break;
        }
        case TaskTypeStub::WAIT: {
            auto *wait = dynamic_cast<TaskStubWait *>(oldNode->task);
            if (wait == nullptr) {
                return false;
            }
            os << "|notify=" << MakeNotifyKey(wait->GetRemoteRank(), oldNode->rankIdx, wait->GetNotifyId())
               << "|" << MakeProtocolKey(ConvertOldProtocolToV3(wait->GetLinkType()));
            break;
        }
        case TaskTypeStub::CCU_GRAPH:
            break;
        default:
            return false;
    }

    key = os.str();
    return true;
}

bool MakeOldNodeSemanticKey(TaskNodePtr oldNode, std::string &key)
{
    return MakeOldNodeSemanticKey(oldNode, nullptr, key);
}

bool MakeV3NodeSemanticKey(const V3TaskNode *newNode, const V3NodeStreamPosMap &streamPosByNode,
    std::string &key)
{
    if (newNode == nullptr) {
        return false;
    }

    if (newNode->GetType() == TaskGraphGeneratorV3::TaskType::START) {
        auto *start = dynamic_cast<const TaskGraphGeneratorV3::TaskStart *>(newNode);
        if (start == nullptr || start->GetBoundaryType() != TaskGraphGeneratorV3::BoundaryType::MAIN_GRAPH) {
            return false;
        }
        key = "type=START|boundary=MAIN_GRAPH";
        return true;
    }

    const auto &loc = newNode->GetPosition();
    const auto posIter = streamPosByNode.find(newNode);
    const uint32_t streamPos = (posIter == streamPosByNode.end()) ? INVALID_GRAPH_STREAM_POS : posIter->second;
    if (IsAivSyntheticDataNode(newNode)) {
        return false;
    }

    std::ostringstream os;
    os << "type=" << TaskTypeKeyName(newNode->GetType()) << "|"
       << MakePositionKey(loc.rankId, loc.streamId, streamPos);
    if (newNode->HasCcuTrace()) {
        const auto &trace = newNode->GetCcuTrace();
        os << "|instrId=" << trace.instrId
           << "|queueId=" << trace.queueId
           << "|op=" << trace.opName
           << "|role=" << trace.nodeRole;
    }

    switch (newNode->GetType()) {
        case TaskGraphGeneratorV3::TaskType::TRANS_MEM: {
            auto *transMem = dynamic_cast<const TaskGraphGeneratorV3::TaskTransMem *>(newNode);
            if (transMem == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(transMem->GetSrc())
               << "|dst=" << MakeMemSliceKey(transMem->GetDst())
               << "|" << MakeProtocolKey(transMem->GetProtocol());
            break;
        }
        case TaskGraphGeneratorV3::TaskType::BATCH_TRANS_MEM: {
            auto *batchTransMem = dynamic_cast<const TaskGraphGeneratorV3::TaskBatchTransMem *>(newNode);
            if (batchTransMem == nullptr) {
                return false;
            }
            const auto &srcs = batchTransMem->GetSrcs();
            const auto &dsts = batchTransMem->GetDsts();
            os << "|pairCount=" << std::min(srcs.size(), dsts.size());
            if (!srcs.empty()) {
                os << "|src0=" << MakeMemSliceKey(srcs.front());
            }
            if (!dsts.empty()) {
                os << "|dst0=" << MakeMemSliceKey(dsts.front());
            }
            os << "|mergedPairCount=" << std::min(batchTransMem->GetMergedSrcs().size(),
                batchTransMem->GetMergedDsts().size())
               << "|" << MakeProtocolKey(batchTransMem->GetProtocol());
            break;
        }
        case TaskGraphGeneratorV3::TaskType::REDUCE: {
            auto *reduce = dynamic_cast<const TaskGraphGeneratorV3::TaskReduce *>(newNode);
            if (reduce == nullptr) {
                return false;
            }
            os << "|src=" << MakeMemSliceKey(reduce->GetSrc())
               << "|dst=" << MakeMemSliceKey(reduce->GetDst())
               << "|dataType=" << static_cast<uint32_t>(reduce->GetDataType())
               << "|reduceOp=" << static_cast<uint32_t>(reduce->GetReduceOp())
               << "|" << MakeProtocolKey(reduce->GetProtocol());
            break;
        }
        case TaskGraphGeneratorV3::TaskType::BATCH_REDUCE: {
            auto *batchReduce = dynamic_cast<const TaskGraphGeneratorV3::TaskBatchReduce *>(newNode);
            if (batchReduce == nullptr) {
                return false;
            }
            const auto &srcGroups = batchReduce->GetSrcs();
            const auto &dsts = batchReduce->GetDsts();
            os << "|groupCount=" << std::min(srcGroups.size(), dsts.size())
               << "|dataType=" << static_cast<uint32_t>(batchReduce->GetDataType())
               << "|reduceOp=" << static_cast<uint32_t>(batchReduce->GetReduceOp());
            if (!srcGroups.empty() && !srcGroups.front().empty()) {
                os << "|src0=" << MakeMemSliceKey(srcGroups.front().front());
            }
            if (!dsts.empty()) {
                os << "|dst0=" << MakeMemSliceKey(dsts.front());
            }
            os << "|mergedGroupCount=" << std::min(batchReduce->GetMergedSrcs().size(),
                batchReduce->GetMergedDsts().size())
               << "|" << MakeProtocolKey(batchReduce->GetProtocol());
            break;
        }
        case TaskGraphGeneratorV3::TaskType::RECORD: {
            if (auto *record = dynamic_cast<const TaskGraphGeneratorV3::TaskRecordAICPU *>(newNode)) {
                os << "|notify=" << MakeNotifyKey(record->GetNotify())
                   << "|" << MakeProtocolKey(record->GetProtocol());
                break;
            }
            if (auto *record = dynamic_cast<const TaskGraphGeneratorV3::TaskRecordCCU *>(newNode)) {
                os << "|notify=" << MakeNotifyKey(record->GetNotify().recordRankId,
                    record->GetNotify().waitRankId, record->GetNotify().ckeId)
                   << "|ckeMask=" << record->GetNotify().ckeMask
                   << "|" << MakeProtocolKey(record->GetProtocol());
                break;
            }
            return false;
        }
        case TaskGraphGeneratorV3::TaskType::WAIT: {
            if (auto *wait = dynamic_cast<const TaskGraphGeneratorV3::TaskWaitAICPU *>(newNode)) {
                os << "|notify=" << MakeNotifyKey(wait->GetNotify())
                   << "|" << MakeProtocolKey(wait->GetProtocol());
                break;
            }
            if (auto *wait = dynamic_cast<const TaskGraphGeneratorV3::TaskWaitCCU *>(newNode)) {
                os << "|notify=" << MakeNotifyKey(wait->GetNotify().recordRankId,
                    wait->GetNotify().waitRankId, wait->GetNotify().ckeId)
                   << "|ckeMask=" << wait->GetNotify().ckeMask
                   << "|" << MakeProtocolKey(wait->GetProtocol());
                break;
            }
            return false;
        }
        case TaskGraphGeneratorV3::TaskType::CCU_GRAPH:
            break;
        default:
            return false;
    }

    key = os.str();
    return true;
}

std::string MakeOldGraphvizNodeLabel(TaskNodePtr oldNode, size_t bfsIndex)
{
    std::ostringstream os;
    os << "old_" << bfsIndex << "\nbfs=" << bfsIndex;
    if (oldNode == nullptr) {
        os << "\n<null old node>";
        return os.str();
    }

    os << "\nrank=" << oldNode->rankIdx << ", stream=" << oldNode->queIdx << ", index=" << oldNode->pos;
    std::string semanticKey;
    if (MakeOldNodeSemanticKey(oldNode, semanticKey)) {
        os << "\n" << FormatGraphvizLabelText(semanticKey);
    } else if (oldNode->task != nullptr) {
        os << "\n" << FormatGraphvizLabelText(oldNode->task->Describe());
    } else {
        os << "\n<invalid old node>";
    }
    return os.str();
}

std::string MakeV3GraphvizNodeLabel(const V3TaskNode *node, const V3NodeStreamPosMap &streamPosByNode,
    size_t bfsIndex)
{
    std::ostringstream os;
    const TaskGraphGeneratorV3::NodeId nodeId =
        (node == nullptr) ? TaskGraphGeneratorV3::INVALID_NODE_ID : node->GetNodeId();
    os << "v3_" << bfsIndex << "\nbfs=" << bfsIndex << "\nnodeId=" << nodeId;

    if (node != nullptr) {
        const auto &loc = node->GetPosition();
        const auto posIter = streamPosByNode.find(node);
        const uint32_t streamPos =
            (posIter == streamPosByNode.end()) ? INVALID_GRAPH_STREAM_POS : posIter->second;
        os << "\nrank=" << loc.rankId << ", stream=" << loc.streamId << ", index=" << streamPos;
    }

    std::string semanticKey;
    if (MakeV3NodeSemanticKey(node, streamPosByNode, semanticKey)) {
        os << "\n" << FormatGraphvizLabelText(semanticKey);
    } else if (node != nullptr) {
        os << "\n" << FormatGraphvizLabelText(node->Describe());
    } else {
        os << "\n<invalid v3 node>";
    }
    return os.str();
}

std::string BuildOldGraphvizDot(const std::vector<TaskNodePtr> &oldBfsNodes)
{
    std::map<TaskNodePtr, size_t> nodeIndexByPtr;
    for (size_t i = 0; i < oldBfsNodes.size(); ++i) {
        nodeIndexByPtr[oldBfsNodes[i]] = i;
    }

    std::set<std::pair<size_t, size_t>> edgeIndexes;
    for (TaskNodePtr oldNode : oldBfsNodes) {
        const auto parentIter = nodeIndexByPtr.find(oldNode);
        if (parentIter == nodeIndexByPtr.end() || oldNode == nullptr) {
            continue;
        }

        for (TaskNodePtr child : oldNode->children) {
            const auto childIter = nodeIndexByPtr.find(child);
            if (childIter == nodeIndexByPtr.end()) {
                continue;
            }
            edgeIndexes.insert(std::make_pair(parentIter->second, childIter->second));
        }
    }

    std::ostringstream os;
    os << "digraph old_task_graph {\n";
    os << "  graph [rankdir=LR, label=\"old graph BFS DAG\", labelloc=t, fontsize=20];\n";
    os << "  node [shape=box, style=\"rounded,filled\", fillcolor=\"#fff7e6\", fontname=\"Courier\"];\n";
    os << "  edge [fontname=\"Courier\"];\n\n";
    for (size_t i = 0; i < oldBfsNodes.size(); ++i) {
        os << "  old_" << i << " [label=\"" << EscapeGraphvizLabel(MakeOldGraphvizNodeLabel(oldBfsNodes[i], i))
           << "\"];\n";
    }

    os << "\n";
    for (const auto &edge : edgeIndexes) {
        os << "  old_" << edge.first << " -> old_" << edge.second << ";\n";
    }
    os << "}\n";
    return os.str();
}

std::string BuildV3GraphvizDot(const std::vector<const V3TaskNode *> &newBfsNodes)
{
    const V3NodeStreamPosMap streamPosByNode = BuildV3StreamPosMap(newBfsNodes);
    std::map<const V3TaskNode *, size_t> nodeIndexByPtr;
    for (size_t i = 0; i < newBfsNodes.size(); ++i) {
        nodeIndexByPtr[newBfsNodes[i]] = i;
    }

    std::set<std::pair<size_t, size_t>> edgeIndexes;
    for (const V3TaskNode *node : newBfsNodes) {
        const auto parentIter = nodeIndexByPtr.find(node);
        if (parentIter == nodeIndexByPtr.end() || node == nullptr) {
            continue;
        }

        for (const V3TaskNode *childNode : node->GetChildren()) {
            const auto childIter = nodeIndexByPtr.find(childNode);
            if (childIter == nodeIndexByPtr.end()) {
                continue;
            }
            edgeIndexes.insert(std::make_pair(parentIter->second, childIter->second));
        }
    }

    std::ostringstream os;
    os << "digraph v3_task_graph {\n";
    os << "  graph [rankdir=LR, label=\"v3 graph BFS DAG\", labelloc=t, fontsize=20];\n";
    os << "  node [shape=box, style=\"rounded,filled\", fillcolor=\"#eef7ff\", fontname=\"Courier\"];\n";
    os << "  edge [fontname=\"Courier\"];\n\n";
    for (size_t i = 0; i < newBfsNodes.size(); ++i) {
        os << "  v3_" << i << " [label=\""
           << EscapeGraphvizLabel(MakeV3GraphvizNodeLabel(newBfsNodes[i], streamPosByNode, i))
           << "\"];\n";
    }

    os << "\n";
    for (const auto &edge : edgeIndexes) {
        os << "  v3_" << edge.first << " -> v3_" << edge.second << ";\n";
    }
    os << "}\n";
    return os.str();
}

void LogGraphvizDot(const std::string &graphName, const std::string &dot)
{
    HCCL_VM_INFO("[TaskGraphGeneratorV3][GraphvizDot][{}] GRAPHVIZ_DOT_BEGIN\n{}", graphName, dot);
    HCCL_VM_INFO("[TaskGraphGeneratorV3][GraphvizDot][{}] GRAPHVIZ_DOT_END", graphName);
}

std::string FormatV3NodeIdList(const std::vector<V3TaskNode *> &nodes)
{
    std::ostringstream os;
    os << "[";
    bool firstNode = true;
    for (const V3TaskNode *node : nodes) {
        if (node == nullptr) {
            continue;
        }
        if (!firstNode) {
            os << ",";
        }
        firstNode = false;
        os << node->GetNodeId();
    }
    os << "]";
    return os.str();
}

std::string MakeAivDagTaskLogLine(const V3TaskNode *node, size_t bfsIndex)
{
    if (node == nullptr) {
        return "<null v3 node>";
    }
    const auto &loc = node->GetPosition();
    std::ostringstream os;
    os << "bfs=" << bfsIndex
       << ", nodeId=" << node->GetNodeId()
       << ", type=" << TaskTypeKeyName(node->GetType())
       << ", rank=" << loc.rankId
       << ", stream=" << loc.streamId
       << ", queue=" << loc.queueId
       << ", parents=" << FormatV3NodeIdList(node->GetParents())
       << ", children=" << FormatV3NodeIdList(node->GetChildren())
       << ", desc=" << node->Describe();
    return os.str();
}

size_t LogAivExpandedDagTasks(const std::vector<const V3TaskNode *> &v3BfsNodes)
{
    size_t aivTaskCount = 0;
    for (size_t index = 0; index < v3BfsNodes.size(); ++index) {
        const V3TaskNode *node = v3BfsNodes[index];
        if (!IsAivDagLogNode(node)) {
            continue;
        }
        HCCL_VM_DEBUG("[TaskGraphGeneratorV3][AivDagTask] {}", MakeAivDagTaskLogLine(node, index));
        ++aivTaskCount;
    }
    HCCL_VM_DEBUG("[TaskGraphGeneratorV3][AivDagTask] Dump AIV expanded DAG task detail, taskCount={}",
        aivTaskCount);
    return aivTaskCount;
}

void LogGraphV3Dag(const V3TaskNode *start)
{
    if (start == nullptr) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3][GraphvizDot][v3] Skip DAG dump, start is null.");
        return;
    }

    const auto v3BfsNodes = BfsGraphV3(start);
    HCCL_VM_INFO("[TaskGraphGeneratorV3][GraphvizDot][v3] Dump V3 DAG, nodeCount={}", v3BfsNodes.size());
    LogGraphvizDot("v3", BuildV3GraphvizDot(v3BfsNodes));
    LogAivExpandedDagTasks(v3BfsNodes);
}

void DumpGraphV3DotToFile(const V3TaskNode *start, const std::string &path)
{
    if (start == nullptr) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3][GraphvizDot][v3] Skip file dump, start is null.");
        return;
    }
    const auto v3BfsNodes = BfsGraphV3(start);
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3][GraphvizDot][v3] Failed to open graph file: {}", path);
        return;
    }
    out << BuildV3GraphvizDot(v3BfsNodes);
    out.close();
    HCCL_VM_WARN("[TaskGraphGeneratorV3][GraphvizDot][v3] Graph dumped to {}, nodeCount={}",
        path, v3BfsNodes.size());
}

struct SemanticGraphSnapshot {
    std::map<std::string, uint32_t> nodeCounts;
    std::set<std::string> edgeKeys;
    size_t invalidNodeCount{0};
    size_t duplicateNodeCount{0};
};

void CollectOldVisibleChildren(TaskNodePtr node, std::set<TaskNodePtr> &visibleChildren)
{
    if (node == nullptr) {
        return;
    }

    std::vector<TaskNodePtr> nodeStack(node->children.begin(), node->children.end());
    std::set<TaskNodePtr> visited;
    while (!nodeStack.empty()) {
        TaskNodePtr currNode = nodeStack.back();
        nodeStack.pop_back();
        if (currNode == nullptr || visited.count(currNode) != 0) {
            continue;
        }
        visited.insert(currNode);
        if (!IsOldGraphCompareTransparent(currNode)) {
            visibleChildren.insert(currNode);
            continue;
        }
        for (TaskNodePtr childNode : currNode->children) {
            nodeStack.push_back(childNode);
        }
    }
}

void CollectV3VisibleChildren(const V3TaskNode *node, std::set<const V3TaskNode *> &visibleChildren)
{
    if (node == nullptr) {
        return;
    }

    std::vector<const V3TaskNode *> nodeStack(node->GetChildren().begin(), node->GetChildren().end());
    std::set<const V3TaskNode *> visited;
    while (!nodeStack.empty()) {
        const V3TaskNode *currNode = nodeStack.back();
        nodeStack.pop_back();
        if (currNode == nullptr || visited.count(currNode) != 0) {
            continue;
        }
        visited.insert(currNode);
        if (!IsV3GraphCompareTransparent(currNode)) {
            visibleChildren.insert(currNode);
            continue;
        }
        for (const V3TaskNode *childNode : currNode->GetChildren()) {
            nodeStack.push_back(childNode);
        }
    }
}

void AddNodeKey(SemanticGraphSnapshot &snapshot, const std::string &nodeKey)
{
    uint32_t &count = snapshot.nodeCounts[nodeKey];
    ++count;
    if (count > 1) {
        ++snapshot.duplicateNodeCount;
    }
}

SemanticGraphSnapshot BuildOldSemanticGraph(const std::vector<TaskNodePtr> &oldBfsNodes)
{
    SemanticGraphSnapshot snapshot;
    const OldNodeStreamPosMap streamPosByNode = BuildOldStreamPosMap(oldBfsNodes);
    std::map<TaskNodePtr, std::string> nodeKeyByPtr;

    for (TaskNodePtr oldNode : oldBfsNodes) {
        if (IsOldGraphCompareTransparent(oldNode)) {
            continue;
        }
        std::string nodeKey;
        if (!MakeOldNodeSemanticKey(oldNode, &streamPosByNode, nodeKey)) {
            ++snapshot.invalidNodeCount;
            continue;
        }
        AddNodeKey(snapshot, nodeKey);
        nodeKeyByPtr[oldNode] = nodeKey;
    }

    for (TaskNodePtr oldNode : oldBfsNodes) {
        const auto parentIter = nodeKeyByPtr.find(oldNode);
        if (parentIter == nodeKeyByPtr.end()) {
            continue;
        }

        std::set<TaskNodePtr> visibleChildren;
        CollectOldVisibleChildren(oldNode, visibleChildren);
        for (TaskNodePtr child : visibleChildren) {
            const auto childIter = nodeKeyByPtr.find(child);
            if (childIter == nodeKeyByPtr.end()) {
                continue;
            }
            snapshot.edgeKeys.insert(parentIter->second + " -> " + childIter->second);
        }
    }

    return snapshot;
}

SemanticGraphSnapshot BuildV3SemanticGraph(const std::vector<const V3TaskNode *> &newBfsNodes)
{
    SemanticGraphSnapshot snapshot;
    const V3NodeStreamPosMap streamPosByNode = BuildV3StreamPosMap(newBfsNodes);
    std::map<const V3TaskNode *, std::string> nodeKeyByPtr;

    for (const V3TaskNode *node : newBfsNodes) {
        if (IsV3GraphCompareTransparent(node)) {
            continue;
        }
        std::string nodeKey;
        if (!MakeV3NodeSemanticKey(node, streamPosByNode, nodeKey)) {
            ++snapshot.invalidNodeCount;
            continue;
        }
        AddNodeKey(snapshot, nodeKey);
        nodeKeyByPtr[node] = nodeKey;
    }

    for (const V3TaskNode *node : newBfsNodes) {
        const auto parentIter = nodeKeyByPtr.find(node);
        if (parentIter == nodeKeyByPtr.end() || node == nullptr) {
            continue;
        }

        std::set<const V3TaskNode *> visibleChildren;
        CollectV3VisibleChildren(node, visibleChildren);
        for (const V3TaskNode *childNode : visibleChildren) {
            const auto childIter = nodeKeyByPtr.find(childNode);
            if (childIter == nodeKeyByPtr.end()) {
                continue;
            }
            snapshot.edgeKeys.insert(parentIter->second + " -> " + childIter->second);
        }
    }

    return snapshot;
}

size_t LogBfsOrderDiff(const std::vector<TaskNodePtr> &oldBfsNodes,
    const std::vector<const V3TaskNode *> &newBfsNodes)
{
    size_t mismatchCount = 0;
    size_t detailCount = 0;
    const OldNodeStreamPosMap oldStreamPosByNode = BuildOldStreamPosMap(oldBfsNodes);
    const V3NodeStreamPosMap streamPosByNode = BuildV3StreamPosMap(newBfsNodes);
    const size_t commonCount = std::min(oldBfsNodes.size(), newBfsNodes.size());

    for (size_t i = 0; i < commonCount; ++i) {
        std::string oldKey;
        if (!MakeOldNodeSemanticKey(oldBfsNodes[i], &oldStreamPosByNode, oldKey)) {
            oldKey = "<invalid old bfs node>";
        }

        std::string newKey;
        if (!MakeV3NodeSemanticKey(newBfsNodes[i], streamPosByNode, newKey)) {
            newKey = "<invalid v3 bfs node>";
        }

        if (oldKey == newKey) {
            continue;
        }

        ++mismatchCount;
        if (detailCount < MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
            HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph BFS order mismatch, bfsIndex={}, oldNode={}, v3Node={}",
                i, oldKey, newKey);
            ++detailCount;
        }
    }

    for (size_t i = commonCount; i < oldBfsNodes.size(); ++i) {
        ++mismatchCount;
        if (detailCount < MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
            std::string oldKey;
            if (!MakeOldNodeSemanticKey(oldBfsNodes[i], &oldStreamPosByNode, oldKey)) {
                oldKey = "<invalid old bfs node>";
            }
            HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph BFS order mismatch, bfsIndex={}, oldNode={}, v3Node={}",
                i, oldKey, "<missing v3 bfs node>");
            ++detailCount;
        }
    }

    for (size_t i = commonCount; i < newBfsNodes.size(); ++i) {
        ++mismatchCount;
        if (detailCount < MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
            std::string newKey;
            if (!MakeV3NodeSemanticKey(newBfsNodes[i], streamPosByNode, newKey)) {
                newKey = "<invalid v3 bfs node>";
            }
            HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph BFS order mismatch, bfsIndex={}, oldNode={}, v3Node={}",
                i, "<missing old bfs node>", newKey);
            ++detailCount;
        }
    }

    if (detailCount >= MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Too many BFS order mismatches, stop detail logging.");
    }

    if (mismatchCount == 0) {
        HCCL_VM_INFO("[TaskGraphGeneratorV3] Graph BFS order matched, count={}", oldBfsNodes.size());
    } else {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph BFS order mismatch, mismatchCount={}, oldCount={}, v3Count={}",
            mismatchCount, oldBfsNodes.size(), newBfsNodes.size());
    }
    return mismatchCount;
}

size_t LogNodeCountDiff(const SemanticGraphSnapshot &oldSnapshot, const SemanticGraphSnapshot &newSnapshot)
{
    size_t mismatchCount = 0;
    size_t detailCount = 0;

    for (const auto &oldEntry : oldSnapshot.nodeCounts) {
        const auto newIter = newSnapshot.nodeCounts.find(oldEntry.first);
        const uint32_t newCount = (newIter == newSnapshot.nodeCounts.end()) ? 0 : newIter->second;
        if (newCount >= oldEntry.second) {
            continue;
        }

        const uint32_t missingCount = oldEntry.second - newCount;
        mismatchCount += missingCount;
        if (detailCount < MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
            HCCL_VM_WARN("[TaskGraphGeneratorV3] V3 graph missing semantic node, count={}, key={}",
                missingCount, oldEntry.first);
            ++detailCount;
        }
    }

    for (const auto &newEntry : newSnapshot.nodeCounts) {
        const auto oldIter = oldSnapshot.nodeCounts.find(newEntry.first);
        const uint32_t oldCount = (oldIter == oldSnapshot.nodeCounts.end()) ? 0 : oldIter->second;
        if (oldCount >= newEntry.second) {
            continue;
        }

        const uint32_t extraCount = newEntry.second - oldCount;
        mismatchCount += extraCount;
        if (detailCount < MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
            HCCL_VM_WARN("[TaskGraphGeneratorV3] V3 graph extra semantic node, count={}, key={}",
                extraCount, newEntry.first);
            ++detailCount;
        }
    }

    if (detailCount >= MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Too many semantic node mismatches, stop detail logging.");
    }
    return mismatchCount;
}

bool IsCcuGraphSemanticKey(const std::string &key)
{
    return key.find("type=CCU_GRAPH") != std::string::npos;
}

bool HasCcuGraphNode(const SemanticGraphSnapshot &snapshot)
{
    for (const auto &nodeEntry : snapshot.nodeCounts) {
        if (IsCcuGraphSemanticKey(nodeEntry.first)) {
            return true;
        }
    }
    return false;
}

bool IsCcuBoundaryEdge(const std::string &edgeKey)
{
    return IsCcuGraphSemanticKey(edgeKey);
}

size_t LogEdgeDiff(const std::set<std::string> &oldEdgeKeys, const std::set<std::string> &newEdgeKeys,
    bool ignoreCcuBoundaryEdges = false)
{
    size_t mismatchCount = 0;
    size_t detailCount = 0;

    for (const auto &edgeKey : oldEdgeKeys) {
        if (ignoreCcuBoundaryEdges && IsCcuBoundaryEdge(edgeKey)) {
            continue;
        }
        if (newEdgeKeys.count(edgeKey) != 0) {
            continue;
        }

        ++mismatchCount;
        if (detailCount < MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
            HCCL_VM_WARN("[TaskGraphGeneratorV3] V3 graph missing edge, edge={}", edgeKey);
            ++detailCount;
        }
    }

    for (const auto &edgeKey : newEdgeKeys) {
        if (ignoreCcuBoundaryEdges && IsCcuBoundaryEdge(edgeKey)) {
            continue;
        }
        if (oldEdgeKeys.count(edgeKey) != 0) {
            continue;
        }

        ++mismatchCount;
        if (detailCount < MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
            HCCL_VM_WARN("[TaskGraphGeneratorV3] V3 graph extra edge, edge={}", edgeKey);
            ++detailCount;
        }
    }

    if (detailCount >= MAX_GRAPH_COMPARE_DETAIL_LOG_COUNT) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Too many semantic edge mismatches, stop detail logging.");
    }
    return mismatchCount;
}

void CompareGraphV3BfsOrder(const std::vector<TaskNodePtr> &oldBfsNodes,
    const std::vector<const V3TaskNode *> &newBfsNodes)
{
    if (oldBfsNodes.size() != newBfsNodes.size()) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph BFS node count mismatch, old={}, v3={}",
            oldBfsNodes.size(), newBfsNodes.size());
    } else {
        HCCL_VM_INFO("[TaskGraphGeneratorV3] Graph BFS node count matched, count={}", oldBfsNodes.size());
    }

    const size_t bfsOrderMismatchCount = LogBfsOrderDiff(oldBfsNodes, newBfsNodes);
    if (bfsOrderMismatchCount == 0 && oldBfsNodes.size() == newBfsNodes.size()) {
        HCCL_VM_INFO("[TaskGraphGeneratorV3] Graph BFS compare matched, orderMatched=true, nodeCount={}",
            oldBfsNodes.size());
    } else {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph BFS compare mismatch, orderMismatch={}, oldNodeCount={}, "
            "v3NodeCount={}", bfsOrderMismatchCount, oldBfsNodes.size(), newBfsNodes.size());
    }
}

void CompareGraphV3Semantic(const std::vector<TaskNodePtr> &oldBfsNodes,
    const std::vector<const V3TaskNode *> &newBfsNodes)
{
    const SemanticGraphSnapshot oldSnapshot = BuildOldSemanticGraph(oldBfsNodes);
    const SemanticGraphSnapshot newSnapshot = BuildV3SemanticGraph(newBfsNodes);
    const bool hasCcuGraph = HasCcuGraphNode(oldSnapshot) || HasCcuGraphNode(newSnapshot);
    const size_t nodeMismatchCount = LogNodeCountDiff(oldSnapshot, newSnapshot);
    const size_t edgeMismatchCount = LogEdgeDiff(oldSnapshot.edgeKeys, newSnapshot.edgeKeys, hasCcuGraph);
    if (hasCcuGraph) {
        HCCL_VM_INFO("[TaskGraphGeneratorV3] Graph semantic compare uses CCU-compatible view: "
            "V3 CCU expanded nodes and old CCU graph separators are ignored; CCU boundary edges are skipped.");
    }

    if (oldSnapshot.invalidNodeCount != 0 || newSnapshot.invalidNodeCount != 0 ||
        oldSnapshot.duplicateNodeCount != 0 || newSnapshot.duplicateNodeCount != 0) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph semantic compare snapshot abnormal, oldInvalid={}, "
            "v3Invalid={}, oldDuplicate={}, v3Duplicate={}", oldSnapshot.invalidNodeCount,
            newSnapshot.invalidNodeCount, oldSnapshot.duplicateNodeCount, newSnapshot.duplicateNodeCount);
    }

    if (nodeMismatchCount == 0 && edgeMismatchCount == 0 && oldSnapshot.invalidNodeCount == 0 &&
        newSnapshot.invalidNodeCount == 0) {
        HCCL_VM_INFO("[TaskGraphGeneratorV3] Graph semantic compare matched, nodeKinds={}, edgeCount={}",
            oldSnapshot.nodeCounts.size(), oldSnapshot.edgeKeys.size());
    } else {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Graph semantic compare mismatch, nodeMismatch={}, edgeMismatch={}, "
            "oldInvalid={}, v3Invalid={}, oldNodeKinds={}, v3NodeKinds={}, oldEdges={}, v3Edges={}",
            nodeMismatchCount, edgeMismatchCount, oldSnapshot.invalidNodeCount, newSnapshot.invalidNodeCount,
            oldSnapshot.nodeCounts.size(), newSnapshot.nodeCounts.size(), oldSnapshot.edgeKeys.size(),
            newSnapshot.edgeKeys.size());
    }
}

void CompareGraphV3WithOldGraph(TaskNodePtr oldStart, const V3TaskNode *newStart)
{
    if (oldStart == nullptr || newStart == nullptr) {
        HCCL_VM_WARN("[TaskGraphGeneratorV3] Skip graph compare, oldStart or newStart is null.");
        return;
    }

    const auto oldBfsNodes = BfsOldGraph(oldStart);
    const auto newBfsNodes = BfsGraphV3(newStart);
    const auto oldCompareNodes = BuildOldGraphCompareNodes(oldBfsNodes);
    const auto newCompareNodes = BuildV3GraphCompareNodes(newBfsNodes);
    // compare 统一使用 BFS 收集节点，随后再过滤掉透明节点（如队列占位节点、CCU 边界节点），
    // 这样既能保持视图稳定，也能让 old/v3 两侧在同一语义层面上比较。
    const bool useTransparentCompareView = oldCompareNodes.size() != oldBfsNodes.size() ||
        newCompareNodes.size() != newBfsNodes.size();
    if (useTransparentCompareView) {
        HCCL_VM_INFO("[TaskGraphGeneratorV3] Graph compare view filtered nodes, oldRaw={}, oldVisible={}, "
            "v3Raw={}, v3Visible={}", oldBfsNodes.size(), oldCompareNodes.size(), newBfsNodes.size(),
            newCompareNodes.size());
    }
    if (useTransparentCompareView) {
        HCCL_VM_INFO("[TaskGraphGeneratorV3] Skip BFS order compare in transparent graph compare view.");
    } else {
        CompareGraphV3BfsOrder(oldCompareNodes, newCompareNodes);
    }
    CompareGraphV3Semantic(oldCompareNodes, newCompareNodes);
}

HcclResult CheckGraphV3TaskMem(const V3TaskNode *start)
{
    V3StageTimer timer("CheckGraphV3TaskMem");
    if (start == nullptr) {
        HCCL_VM_WARN("[TaskGraphSingleTaskCheckV3] Skip V3 task memory check, start is null.");
        timer.SetStatus("skipped");
        return HCCL_SUCCESS;
    }

    // 单 task 校验只看每个节点自身的 src/dst 描述是否合法，不依赖跨节点语义。
    TaskGraphGeneratorV3::SingleTaskCheckStats stats;
    HcclResult ret = TaskGraphGeneratorV3::CheckTaskMem(start, &stats);
    std::ostringstream extraInfo;
    extraInfo << "nodeCount=" << stats.nodeCount
              << ", dataTaskNodeCount=" << stats.dataTaskNodeCount;
    if (TaskGraphGeneratorV3::g_checkerAivUbBufferSize != 0 ||
        TaskGraphGeneratorV3::g_checkerAivFlagBufferSize != 0) {
        extraInfo << ", aivUbBufferSize=" << TaskGraphGeneratorV3::g_checkerAivUbBufferSize
                  << ", aivFlagBufferSize=" << TaskGraphGeneratorV3::g_checkerAivFlagBufferSize;
    }
    timer.SetExtraInfo(extraInfo.str());
    timer.SetStatus(ret == HCCL_SUCCESS ? "success" : "failed");
    return ret;
}

HcclResult CheckGraphV3MemConflict(const V3TaskNode *start)
{
    V3StageTimer timer("CheckGraphV3MemConflict");
    if (start == nullptr) {
        HCCL_VM_WARN("[TaskGraphMemConflictV3] Skip V3 memory conflict check, start is null.");
        timer.SetStatus("skipped");
        return HCCL_SUCCESS;
    }

    // 内存冲突校验会先收集所有数据搬运节点的访问区间，再按地址桶扫描重叠候选。
    TaskGraphGeneratorV3::MemConflictCheckStats stats;
    HcclResult ret = TaskGraphGeneratorV3::CheckDataMoveTaskMemConflict(start, &stats);
    std::ostringstream extraInfo;
    extraInfo << "nodeCount=" << stats.nodeCount
              << ", dataTaskNodeCount=" << stats.dataTaskNodeCount
              << ", processedDataTaskNodeCount=" << stats.processedDataTaskNodeCount
              << ", accessIntervalCount=" << stats.accessIntervalCount
              << ", memoryBucketCount=" << stats.memoryBucketCount
              << ", overlapCandidatePairs=" << stats.overlapCandidatePairCount
              << ", orderedCandidatePairs=" << stats.orderedCandidatePairCount
              << ", parallelCandidatePairs=" << stats.parallelCandidatePairCount;
    timer.SetExtraInfo(extraInfo.str());
    timer.SetStatus(ret == HCCL_SUCCESS ? "success" : "failed");
    return ret;
}

HcclResult CheckGraphV3Semantic(const V3TaskNode *start)
{
    V3StageTimer timer("CheckGraphV3Semantic");
    if (start == nullptr) {
        HCCL_VM_WARN("[TaskGraphSemanticCheckV3] Skip V3 semantic check, start is null.");
        timer.SetStatus("skipped");
        return HCCL_SUCCESS;
    }

    // 语义校验按 DAG 依赖逐节点模拟内存来源，最后再校验各 rank 输出布局。
    TaskGraphGeneratorV3::SemanticCheckStats stats;
    HcclResult ret = TaskGraphGeneratorV3::CheckSemantics(start, &stats);
    std::ostringstream extraInfo;
    extraInfo << "handledNodeCount=" << stats.handledNodeCount
              << ", rankCount=" << stats.rankCount
              << ", normalSemanticCount=" << stats.normalSemanticCount
              << ", normalSemanticBytes=" << stats.normalSemanticBytes
              << ", msBucketCount=" << stats.msBucketCount;
    timer.SetExtraInfo(extraInfo.str());
    timer.SetStatus(ret == HCCL_SUCCESS ? "success" : "failed");
    return ret;
}
} // namespace

Checker::~Checker()
{
    for (auto &ele : toDeleteCopyTaskNodeResource_) {
        if (ele == nullptr) {
            continue;
        }
        delete ele;
    }

    for (auto &ele : toDeleteCopyTaskResource_) {
        if (ele == nullptr) {
            continue;
        }
        delete ele;
    }
}

void Checker::CloseRankMemCheck()
{
    closeRankMemCheck_ = true;
}

HcclResult GenAndCheckGraphV3()
{
    auto graphGeneratorV3 = GenGraphV3FromTaskMeta();
    if (graphGeneratorV3 == nullptr) {
        return HCCL_E_PTR;
    }
    const HcclResult dumpRet = DumpV3Dag(*graphGeneratorV3);
    if (dumpRet != HCCL_SUCCESS) {
        HCCL_VM_WARN("[GenAndCheckGraphV3] failed to dump V3 dag, ret={}", static_cast<uint32_t>(dumpRet));
    }
    const V3TaskNode *dummyStartV3 = graphGeneratorV3->GetMainStartNode();
    // 打印dag task任务
    // LogGraphV3Dag(dummyStartV3);

    // V3 检查顺序与日志阶段保持一致：
    // 成图 -> 单 task 内存校验 -> 内存冲突校验 -> 语义校验。
    CHK_RET(CheckGraphV3TaskMem(dummyStartV3));
    if (ENABLE_V3_MEM_CONFLICT_CHECK) {
        CHK_RET(CheckGraphV3MemConflict(dummyStartV3));
    }
    if (ENABLE_V3_SEMANTIC_CHECK) {
        CHK_RET(CheckGraphV3Semantic(dummyStartV3));
    }
    return HCCL_SUCCESS;
}

void PrintSQEGraph(TaskNodePtr dummyStart);
HcclResult Checker::GenAndCheckGraph(AllRankTaskQueues &allRankTaskQueues, TaskCheckOpSemantics &opSemanticsChecker)
{
    ValidationIssueRecorder::GetInstance().Reset();

    u32 rankIdx = 0;
    uint32_t rankNum = 0;
    for (auto& iter : allRankTaskQueues) {
        rankIdx = iter.first;
        rankNum++;
        HCCL_VM_INFO("=======================================================");
        HCCL_VM_INFO("rankId is : {:d}", rankIdx);
        const SingleTaskQueue& taskQueue = iter.second;
        for (int i = 0; i < taskQueue.size(); i++) {
            if (taskQueue[i].size() == 0) {
                continue;
            }

            u32 taskIdx = 0;
            HCCL_VM_INFO("streamIdx : {:d}, taskNum : {:d}", i, taskQueue[i].size());
            HCCL_VM_INFO("-------------------------------------------------------");
            for (auto& task : taskQueue[i]) {
                std::string tempStr = task->Describe();
                HCCL_VM_INFO("rankIdx:{:d}, taskIdx:{:d}, {:s}, pointer: {:p}", rankIdx, taskIdx, tempStr, static_cast<void*>(task.get()));
                taskIdx++;
            }
        }
    }
    {
        const HcclResult dumpRet = DumpInputTaskQueues(allRankTaskQueues);
        if (dumpRet != HcclResult::HCCL_SUCCESS) {
            HCCL_VM_WARN("[Checker::GenAndCheckGraph] failed to dump input task queues, ret[%u].",
                static_cast<u32>(dumpRet));
        }
    }

    // 1. 检查从流
    HCCL_INFO("1. 检查从流");
    SingleTaskCheck taskChecker;
    CHK_RET(taskChecker.CheckSlaveTaskQueue(allRankTaskQueues));

    // 2. 成图
    HCCL_INFO("2. 成图");
    TaskNode dummyStart = TaskNode(nullptr, -1, 0, 0);
    TaskNode dummyStartCopy = TaskNode(nullptr, -1, 0, 0);
    TaskGraphGenerator graphGenerator;
    CHK_RET(graphGenerator.GenGraph(allRankTaskQueues, &dummyStart));

    // 3. Task内存校验
    // 是否可以复用 taskChecker
    HCCL_INFO("3. Task内存校验");
    CHK_RET(taskChecker.CheckTaskMem(&dummyStart));
    HCCL_INFO("3. Task内存校验 END: %d", closeRankMemCheck_);

    if (!closeRankMemCheck_) {
        // 4. 图复制
        HCCL_INFO("4. 图复制");
        if (dummyStart.hasCcuTask) {
            CHK_RET(CopyCcuTaskGraph(&dummyStart, &dummyStartCopy, rankNum));
            dummyStartCopy.hasCcuTask = true;
        } else {
            CopyTaskGraph(&dummyStart, &dummyStartCopy);
        }
        
         // 5. 图改造
        HCCL_INFO("5. 图改造");
        GraphRevampBilateralSemantics graphRevamp;

        // ccu图改造
        GraphRevampParallel parallelRevamp;
        GraphRevampBilateralCcu ccuBilateralRevamp;
        if(dummyStartCopy.hasCcuTask) {
            HcclResult ret = HcclResult::HCCL_SUCCESS;
            // 并行化改造：异步节点、Loop指令块
            ret = parallelRevamp.Revamp(&dummyStartCopy);
            if (ret != HcclResult::HCCL_SUCCESS) {
                return ret;
            }

            // 单边->双边语义: CCU子图
            ret = ccuBilateralRevamp.Revamp(&dummyStartCopy);
            if (ret != HcclResult::HCCL_SUCCESS) {
                return ret;
            }
        }
        // 主图改造
        CHK_RET(graphRevamp.Revamp(&dummyStartCopy));

        // 6. Rank内存校验
        HCCL_INFO("6. Rank内存校验");
        CheckRankMem checkRankmem(&dummyStartCopy);
        CHK_RET(checkRankmem.Execute());
    }

    // 7. 语义校验
    HCCL_INFO("7. 语义校验");
    // PrintCcuGraph(&dummyStart);
    opSemanticsChecker.SetGraphHead(&dummyStart);
    CHK_RET(opSemanticsChecker.Execute());
    HCCL_INFO("8. 语义校验 END");

    // 成图及校验成功
    return HCCL_SUCCESS;
}

void Checker::CopyTaskGraph(TaskNodePtr originNode, TaskNodePtr copyNode)
{
    // 该函数无修改
    // 遍历两遍，先将所有节点拷贝出来，再建立父子关系
    std::map<TaskNodePtr, TaskNodePtr> originNode2copyNode;  // 用来收录原节点到新节点的映射
    std::vector<TaskNodePtr> candTaskNodePtr;
    std::set<TaskNodePtr> isVisited;

    originNode2copyNode[originNode] = copyNode;
    for (int i = 0; i < originNode->children.size(); i++) {
        candTaskNodePtr.push_back(originNode->children[i]);
        isVisited.insert(originNode->children[i]);
    }

    while (!candTaskNodePtr.empty()) {
        TaskNodePtr curNode = candTaskNodePtr[0];
        candTaskNodePtr.erase(candTaskNodePtr.begin());

        TaskNodePtr newNodePtr = new TaskNode(curNode->task, curNode->rankIdx, curNode->queIdx, curNode->pos);
        toDeleteCopyTaskNodeResource_.push_back(newNodePtr);
        originNode2copyNode[curNode] = newNodePtr;

        for (auto &child : curNode->children) {
            if (isVisited.find(child) == isVisited.end()) {
                isVisited.insert(child);
                candTaskNodePtr.push_back(child);
            }
        }
    }

    isVisited.clear();
    for (int i = 0; i < originNode->children.size(); i++) {
        candTaskNodePtr.push_back(originNode->children[i]);
        isVisited.insert(originNode->children[i]);
        copyNode->children.push_back(originNode2copyNode[originNode->children[i]]);
    }
    while (!candTaskNodePtr.empty()) {
        TaskNodePtr curNode = candTaskNodePtr[0];
        candTaskNodePtr.erase(candTaskNodePtr.begin());
        for (auto &parent : curNode->parents) {
            originNode2copyNode[curNode]->parents.push_back(originNode2copyNode[parent]);
        }
        for (auto &child : curNode->children) {
            originNode2copyNode[curNode]->children.push_back(originNode2copyNode[child]);
            if (isVisited.count(child) == 0) {
                isVisited.insert(child);
                candTaskNodePtr.push_back(child);
            }
        }
    }
}

HcclResult Checker::CopyCcuTaskGraph(TaskNodePtr originNode, TaskNodePtr copyNode, uint32_t rankNum)
{
    // 遍历两遍，先将所有节点拷贝出来，再建立父子关系
    std::map<TaskNodePtr, TaskNodePtr> originNode2copyNode; // 用来收录原节点到新节点的映射
    std::vector<TaskNodePtr> candTaskNodePtr;
    std::set<TaskNodePtr> isVisited;

    originNode2copyNode[originNode] = copyNode;
    for (int i = 0; i < originNode->children.size(); i++) {
        candTaskNodePtr.push_back(originNode->children[i]);
        isVisited.insert(originNode->children[i]);
    }

    // 记录ccu子图旧->新节点映射关系
    // 复用taskStub资源，新建taskNode资源
    std::vector<CcuOri2NewNodeMap> ccuOrigin2CopyNodes;
    std::vector<std::unordered_map<TaskStubPtr, TaskStubPtr>> ccuGraphs;
    ccuOrigin2CopyNodes.resize(rankNum);
    ccuGraphs.resize(rankNum);

    while (!candTaskNodePtr.empty()) {
        TaskNodePtr curNode = candTaskNodePtr[0];
        candTaskNodePtr.erase(candTaskNodePtr.begin());

        TaskStub* newNode = curNode->task;
        if (newNode->GetType() == TaskTypeStub::CCU_GRAPH) {
            TaskStub* newCcu = nullptr;
            CHK_RET(CopyCcuSubGraphNode(newNode, &newCcu, ccuGraphs, ccuOrigin2CopyNodes));
            newNode = newCcu;
            toDeleteCopyTaskResource_.push_back(newNode);
        }
        TaskNodePtr newNodePtr = new TaskNode(newNode, curNode->rankIdx, curNode->queIdx, curNode->pos);
        toDeleteCopyTaskNodeResource_.push_back(newNodePtr);
        originNode2copyNode[curNode] = newNodePtr;

        for (auto &child : curNode->children) {
            if (isVisited.find(child) == isVisited.end()) {
                isVisited.insert(child);
                candTaskNodePtr.push_back(child);
            }
        }
    }

    // 恢复ccu子图内部的连接关系
    CHK_RET(CopyCcuSubGraphConnection(ccuGraphs, ccuOrigin2CopyNodes));
    // 恢复外层拓扑图的连接关系
    isVisited.clear();
    for (int i = 0; i < originNode->children.size(); i++) {
        candTaskNodePtr.push_back(originNode->children[i]);
        isVisited.insert(originNode->children[i]);
        copyNode->children.push_back(originNode2copyNode[originNode->children[i]]);
    }
    while(!candTaskNodePtr.empty()) {
        TaskNodePtr curNode = candTaskNodePtr[0];
        candTaskNodePtr.erase(candTaskNodePtr.begin());
        for (auto &parent : curNode->parents) {
            originNode2copyNode[curNode]->parents.push_back(originNode2copyNode[parent]);
        }
        for (auto &child : curNode->children) {
            originNode2copyNode[curNode]->children.push_back(originNode2copyNode[child]);
            if (isVisited.count(child) == 0) {
                isVisited.insert(child);
                candTaskNodePtr.push_back(child);
            }
        }
    }

    for(auto &ccuGraph : ccuGraphs) {
        for (auto ccuPair : ccuGraph) {
            g_ccuGraphTaskOri2New.insert(ccuPair);
        }
    }

    return HcclResult::HCCL_SUCCESS;
}
}
