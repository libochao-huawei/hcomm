/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "task_graph_single_task_check_v3.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "sim_log.h"
#include "storage_manager.h"

namespace HcclSim {
namespace TaskGraphGeneratorV3 {
namespace {
constexpr uint64_t AIV_FLAG_SLOT_BYTES = 128ULL;

const TaskNode *GetQueueNode(const std::vector<std::unique_ptr<TaskNode>> &nodes, NodeId nodeId)
{
    if (nodeId < 0 || static_cast<size_t>(nodeId) >= nodes.size()) {
        return nullptr;
    }
    return nodes[static_cast<size_t>(nodeId)].get();
}

const char *TaskTypeName(TaskType taskType)
{
    switch (taskType) {
        case TaskType::TRANS_MEM:
            return "TRANS_MEM";
        case TaskType::BATCH_TRANS_MEM:
            return "BATCH_TRANS_MEM";
        case TaskType::REDUCE:
            return "REDUCE";
        case TaskType::BATCH_REDUCE:
            return "BATCH_REDUCE";
        case TaskType::RECORD:
            return "RECORD";
        case TaskType::WAIT:
            return "WAIT";
        case TaskType::CCU_GRAPH:
            return "CCU_GRAPH";
        case TaskType::AIV_GRAPH:
            return "AIV_GRAPH";
        case TaskType::AIV_SET_FLAG:
            return "AIV_SET_FLAG";
        case TaskType::AIV_WAIT_FLAG:
            return "AIV_WAIT_FLAG";
        case TaskType::AIV_PIPE_BARRIER:
            return "AIV_PIPE_BARRIER";
        case TaskType::AIV_SYNC_ALL:
            return "AIV_SYNC_ALL";
        case TaskType::AIV_SEND_FLAG:
            return "AIV_SEND_FLAG";
        case TaskType::AIV_RECV_FLAG:
            return "AIV_RECV_FLAG";
        case TaskType::START:
            return "START";
        case TaskType::END:
            return "END";
        default:
            return "INVALID";
    }
}

std::string DescribePosition(const TaskPosition &loc)
{
    std::ostringstream os;
    os << "rank=" << loc.rankId << ", stream=" << loc.streamId;
    return os.str();
}

std::string DescribeMemSlice(const MemSlice &slice)
{
    std::ostringstream os;
    os << "{rank=" << slice.rankId << ", mem=" << static_cast<uint32_t>(slice.memType)
       << ", offset=0x" << std::hex << slice.offset << ", len=0x" << slice.len << std::dec << "}";
    return os.str();
}

bool IsLocalAicpuRecord(const TaskNode *node)
{
    if (node == nullptr || node->GetType() != TaskType::RECORD) {
        return false;
    }
    const auto *record = dynamic_cast<const TaskRecordAICPU *>(node);
    if (record == nullptr) {
        return false;
    }
    const AicpuNotify &notify = record->GetNotify();
    return notify.recordRankId == notify.waitRankId;
}

bool IsLocalAicpuWait(const TaskNode *node)
{
    if (node == nullptr || node->GetType() != TaskType::WAIT) {
        return false;
    }
    const auto *wait = dynamic_cast<const TaskWaitAICPU *>(node);
    if (wait == nullptr) {
        return false;
    }
    const AicpuNotify &notify = wait->GetNotify();
    return notify.recordRankId == notify.waitRankId;
}

bool IsEmptyLocalCopy(const TaskNode *node)
{
    if (node == nullptr || node->GetType() != TaskType::TRANS_MEM) {
        return false;
    }
    const auto *transMem = dynamic_cast<const TaskTransMem *>(node);
    if (transMem == nullptr) {
        return false;
    }
    const MemSlice &src = transMem->GetSrc();
    const MemSlice &dst = transMem->GetDst();
    return src.rankId == dst.rankId && src.len == 0 && dst.len == 0;
}

bool IsDataMoveTaskNode(const TaskNode *node)
{
    return node != nullptr && (node->GetType() == TaskType::TRANS_MEM ||
        node->GetType() == TaskType::BATCH_TRANS_MEM || node->GetType() == TaskType::REDUCE ||
        node->GetType() == TaskType::BATCH_REDUCE);
}

bool StreamHasAivGraph(const std::vector<std::unique_ptr<TaskNode>> &nodes, const std::vector<NodeId> &stream)
{
    for (const NodeId nodeId : stream) {
        const TaskNode *node = GetQueueNode(nodes, nodeId);
        if (node != nullptr && node->GetType() == TaskType::AIV_GRAPH) {
            return true;
        }
    }
    return false;
}

BufferType ConvertMemTypeToBufferType(MemType memType)
{
    switch (memType) {
        case MemType::INPUT:
            return BufferType::INPUT;
        case MemType::OUTPUT:
            return BufferType::OUTPUT;
        case MemType::CCL:
            return BufferType::CCL;
        case MemType::MS_CCU:
            return BufferType::MS;
        default:
            return BufferType::RESERVED;
    }
}

bool IsSameMemoryType(const MemSlice &lhs, const MemSlice &rhs)
{
    return lhs.memType == rhs.memType;
}

uint64_t GetAivSliceBoundSize(MemType memType)
{
    if (memType == MemType::UB_AIV) {
        return g_checkerAivUbBufferSize;
    }
    if (memType == MemType::FLAG_AIV) {
        return g_checkerAivFlagBufferSize;
    }
    return 0;
}

HcclResult CheckSingleSlice(const TaskNode *node, const MemSlice &slice);
HcclResult CheckTwoSliceOverlap(const TaskNode *node, const MemSlice &lhs, const MemSlice &rhs);

struct MemoryKey {
    RankId rankId{INVALID_RANK_ID};
    MemType memType{MemType::INVALID};

    bool operator==(const MemoryKey &rhs) const
    {
        return rankId == rhs.rankId && memType == rhs.memType;
    }

    bool operator<(const MemoryKey &rhs) const
    {
        if (rankId != rhs.rankId) {
            return rankId < rhs.rankId;
        }
        return static_cast<uint32_t>(memType) < static_cast<uint32_t>(rhs.memType);
    }
};

struct SliceCoverageSummary {
    std::map<MemoryKey, uint64_t> coveredSizeByKey;
    uint64_t totalCoveredSize{0};
};

// 将一组 slice 归并成按 {rank, memType} 统计的覆盖大小。
// 普通内存按区间并集计算；MS 由于是复用内存，按出现次数累计。
HcclResult AccumulateCoveredSizeByKey(const TaskNode *node, const std::vector<MemSlice> &slices,
    SliceCoverageSummary &summary)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }

    std::map<MemoryKey, std::vector<std::pair<uint64_t, uint64_t>>> rangesByKey;
    std::map<MemoryKey, uint64_t> repeatedSizeByKey;
    for (const auto &slice : slices) {
        if (slice.len == 0) {
            continue;
        }
        if (slice.len > std::numeric_limits<uint64_t>::max() - slice.offset) {
            HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Memory slice overflow while accumulating coverage, "
                "node={}, slice={}", node->Describe(), DescribeMemSlice(slice));
            return HCCL_E_PARA;
        }
        const MemoryKey key{slice.rankId, slice.memType};
        if (slice.memType == MemType::MS_CCU) {
            repeatedSizeByKey[key] += slice.len;
            continue;
        }
        rangesByKey[key].push_back({slice.offset, slice.offset + slice.len});
    }

    for (auto &entry : rangesByKey) {
        auto &ranges = entry.second;
        std::sort(ranges.begin(), ranges.end(), [](const std::pair<uint64_t, uint64_t> &lhs,
            const std::pair<uint64_t, uint64_t> &rhs) {
            if (lhs.first != rhs.first) {
                return lhs.first < rhs.first;
            }
            return lhs.second < rhs.second;
        });
        uint64_t coveredSize = 0;
        uint64_t mergedStart = ranges.front().first;
        uint64_t mergedEnd = ranges.front().second;
        for (size_t index = 1; index < ranges.size(); ++index) {
            const uint64_t curStart = ranges[index].first;
            const uint64_t curEnd = ranges[index].second;
            if (curStart <= mergedEnd) {
                mergedEnd = std::max(mergedEnd, curEnd);
                continue;
            }
            coveredSize += mergedEnd - mergedStart;
            mergedStart = curStart;
            mergedEnd = curEnd;
        }
        coveredSize += mergedEnd - mergedStart;
        summary.coveredSizeByKey[entry.first] += coveredSize;
        summary.totalCoveredSize += coveredSize;
    }

    for (const auto &entry : repeatedSizeByKey) {
        summary.coveredSizeByKey[entry.first] += entry.second;
        summary.totalCoveredSize += entry.second;
    }

    return HCCL_SUCCESS;
}

HcclResult BuildCoverageSummary(const TaskNode *node, const std::vector<MemSlice> &slices,
    SliceCoverageSummary &summary)
{
    summary = SliceCoverageSummary{};
    return AccumulateCoveredSizeByKey(node, slices, summary);
}

HcclResult BuildCoverageSummary(const TaskNode *node, const std::vector<std::vector<MemSlice>> &sliceGroups,
    SliceCoverageSummary &summary)
{
    summary = SliceCoverageSummary{};
    for (const auto &group : sliceGroups) {
        HcclResult ret = AccumulateCoveredSizeByKey(node, group, summary);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CheckCoverageMultiple(const TaskNode *node, uint64_t srcSize, uint64_t dstSize,
    const std::string &srcName, const std::string &dstName)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    if (dstSize == 0) {
        if (srcSize == 0) {
            return HCCL_SUCCESS;
        }
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Zero destination coverage with non-zero source coverage, "
            "node={}, {}={}, {}={}", node->Describe(), srcName, srcSize, dstName, dstSize);
        return HCCL_E_INTERNAL;
    }
    if (srcSize % dstSize == 0) {
        return HCCL_SUCCESS;
    }
    HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Source coverage is not a multiple of destination coverage, "
        "node={}, {}={}, {}={}", node->Describe(), srcName, srcSize, dstName, dstSize);
    return HCCL_E_INTERNAL;
}

HcclResult CheckSliceLenEqual(const TaskNode *node, const MemSlice &src, const MemSlice &dst,
    const std::string &label, size_t index)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    if (src.len == dst.len) {
        return HCCL_SUCCESS;
    }
    HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Batch trans slice length mismatch, node={}, label={}, index={}, "
        "src={}, dst={}", node->Describe(), label, index, DescribeMemSlice(src), DescribeMemSlice(dst));
    return HCCL_E_INTERNAL;
}

bool MemSliceLess(const MemSlice &lhs, const MemSlice &rhs)
{
    if (lhs.rankId != rhs.rankId) {
        return lhs.rankId < rhs.rankId;
    }
    if (lhs.memType != rhs.memType) {
        return static_cast<uint32_t>(lhs.memType) < static_cast<uint32_t>(rhs.memType);
    }
    if (lhs.offset != rhs.offset) {
        return lhs.offset < rhs.offset;
    }
    return lhs.len < rhs.len;
}

// merged slices 只用于更快地做 batch 节点内部重叠检查，因此这里先按完整四元组去重。
void DedupMemSlices(std::vector<MemSlice> &slices)
{
    if (slices.size() <= 1) {
        return;
    }
    std::sort(slices.begin(), slices.end(), MemSliceLess);
    slices.erase(std::unique(slices.begin(), slices.end(), [](const MemSlice &lhs, const MemSlice &rhs) {
        return lhs.rankId == rhs.rankId && lhs.memType == rhs.memType && lhs.offset == rhs.offset &&
            lhs.len == rhs.len;
    }), slices.end());
}

HcclResult CheckBatchTransSlices(const TaskNode *node, const std::vector<MemSlice> &srcs,
    const std::vector<MemSlice> &dsts, const std::vector<MemSlice> &mergedSrcs,
    const std::vector<MemSlice> &mergedDsts, const std::string &label)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    // 原始 src/dst 负责逐对检查 memcpy 语义是否满足“长度一一对应”；
    // merged src/dst 只用于更快地检查 batch 节点内部是否存在自冲突。
    if (srcs.size() != dsts.size()) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Batch trans pair size mismatch, node={}, label={}, "
            "srcCount={}, dstCount={}", node->Describe(), label, srcs.size(), dsts.size());
        return HCCL_E_PARA;
    }
    for (size_t index = 0; index < srcs.size(); ++index) {
        HcclResult ret = CheckSingleSlice(node, srcs[index]);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = CheckSingleSlice(node, dsts[index]);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = CheckSliceLenEqual(node, srcs[index], dsts[index], label, index);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }

    std::vector<MemSlice> dedupMergedSrcs = mergedSrcs;
    std::vector<MemSlice> dedupMergedDsts = mergedDsts;
    DedupMemSlices(dedupMergedSrcs);
    DedupMemSlices(dedupMergedDsts);

    for (const auto &src : dedupMergedSrcs) {
        HcclResult ret = CheckSingleSlice(node, src);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    for (const auto &dst : dedupMergedDsts) {
        HcclResult ret = CheckSingleSlice(node, dst);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    for (const auto &src : dedupMergedSrcs) {
        for (const auto &dst : dedupMergedDsts) {
            HcclResult ret = CheckTwoSliceOverlap(node, src, dst);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
        }
    }
    HCCL_VM_DEBUG("[TaskGraphSingleTaskCheckV3] Batch trans merged slices checked, "
        "nodeId={}, label={}, originalSrcCount={}, originalDstCount={}, mergedSrcCount={}, mergedDstCount={}, "
        "dedupMergedSrcCount={}, dedupMergedDstCount={}", node->GetNodeId(), label, srcs.size(), dsts.size(),
        mergedSrcs.size(), mergedDsts.size(), dedupMergedSrcs.size(), dedupMergedDsts.size());
    return HCCL_SUCCESS;
}

HcclResult CheckBatchReduceSlices(const TaskNode *node, const std::vector<std::vector<MemSlice>> &srcGroups,
    const std::vector<MemSlice> &dsts, const std::string &label, SliceCoverageSummary &srcSummary,
    SliceCoverageSummary &dstSummary)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    // batch reduce 仍然按原始 group 校验：每组 src 必须能落到同一个 dst，
    // 且所有 src 总覆盖量应当是 dst 总覆盖量的整倍数。
    if (srcGroups.empty() || srcGroups.size() != dsts.size()) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Batch reduce group size mismatch, node={}, label={}, "
            "srcGroupCount={}, dstCount={}", node->Describe(), label, srcGroups.size(), dsts.size());
        return HCCL_E_PARA;
    }
    for (size_t groupIndex = 0; groupIndex < srcGroups.size(); ++groupIndex) {
        HcclResult ret = CheckSingleSlice(node, dsts[groupIndex]);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        if (srcGroups[groupIndex].empty()) {
            HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Batch reduce source list is empty, node={}, label={}, "
                "groupIndex={}", node->Describe(), label, groupIndex);
            return HCCL_E_PARA;
        }
        for (const auto &src : srcGroups[groupIndex]) {
            ret = CheckSingleSlice(node, src);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
            ret = CheckTwoSliceOverlap(node, src, dsts[groupIndex]);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
        }
    }

    HcclResult ret = BuildCoverageSummary(node, srcGroups, srcSummary);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    ret = BuildCoverageSummary(node, dsts, dstSummary);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    return CheckCoverageMultiple(node, srcSummary.totalCoveredSize, dstSummary.totalCoveredSize,
        label + " srcCoveredSize", label + " dstCoveredSize");
}

HcclResult CheckSingleSlice(const TaskNode *node, const MemSlice &slice)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    if (slice.memType == MemType::MS_CCU) {
        return HCCL_SUCCESS;
    }
    if (slice.rankId == INVALID_RANK_ID || slice.memType == MemType::INVALID) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Invalid memory slice, node={}, slice={}",
            node->Describe(), DescribeMemSlice(slice));
        return HCCL_E_PARA;
    }
    if (slice.len > std::numeric_limits<uint64_t>::max() - slice.offset) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Memory slice overflow, node={}, slice={}",
            node->Describe(), DescribeMemSlice(slice));
        return HCCL_E_PARA;
    }

    if (slice.memType == MemType::UB_AIV || slice.memType == MemType::FLAG_AIV) {
        const uint64_t boundSize = GetAivSliceBoundSize(slice.memType);
        if (boundSize != 0 && slice.offset + slice.len > boundSize) {
            HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] AIV slice out-of-bounds, node={}, slice={}, boundSize={}",
                node->Describe(), DescribeMemSlice(slice), boundSize);
            return HCCL_E_INTERNAL;
        }
        return HCCL_SUCCESS;
    }

    const BufferType storageType = ConvertMemTypeToBufferType(slice.memType);
    if (storageType == BufferType::RESERVED) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Unsupported memory type, node={}, slice={}",
            node->Describe(), DescribeMemSlice(slice));
        return HCCL_E_NOT_SUPPORT;
    }

    const uint64_t blockSize = StorageManager::GetInstance().GetBlockSize(slice.rankId, storageType);
    if (slice.offset + slice.len > blockSize) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Failed to check slice, {}, node={}, slice={}, blockSize={}",
            DescribePosition(node->GetPosition()), node->Describe(), DescribeMemSlice(slice), blockSize);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult CheckTwoSliceOverlap(const TaskNode *node, const MemSlice &lhs, const MemSlice &rhs)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    if (lhs.rankId != rhs.rankId || !IsSameMemoryType(lhs, rhs) || lhs.len == 0 || rhs.len == 0) {
        return HCCL_SUCCESS;
    }
    if (lhs.len > std::numeric_limits<uint64_t>::max() - lhs.offset ||
        rhs.len > std::numeric_limits<uint64_t>::max() - rhs.offset) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Memory slice overflow, node={}, sliceA={}, sliceB={}",
            node->Describe(), DescribeMemSlice(lhs), DescribeMemSlice(rhs));
        return HCCL_E_PARA;
    }

    if (node->HasCcuTrace()) {
        // CCU mode下，允许src == dst
        if (lhs.rankId == rhs.rankId && IsSameMemoryType(lhs, rhs) && lhs.offset == rhs.offset && lhs.len == rhs.len) {
            HCCL_VM_WARN("[TaskGraphSingleTaskCheckV3] MemSlice are same (src == dst), which may affect performance, {}, node={}, sliceA={}, sliceB={}",
                DescribePosition(node->GetPosition()), node->Describe(), DescribeMemSlice(lhs), DescribeMemSlice(rhs));
            return HCCL_SUCCESS;
        }
    }

    const bool conflictCase1 = lhs.offset >= rhs.offset && lhs.offset < (rhs.offset + rhs.len);
    const bool conflictCase2 = rhs.offset >= lhs.offset && rhs.offset < (lhs.offset + lhs.len);
    if (conflictCase1 || conflictCase2) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Slice conflict, {}, node={}, sliceA={}, sliceB={}",
            DescribePosition(node->GetPosition()), node->Describe(), DescribeMemSlice(lhs), DescribeMemSlice(rhs));
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult CheckAivFlagNodeMem(const TaskNode *node)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    const AivFlagSync *flag = nullptr;
    if (node->GetType() == TaskType::AIV_SEND_FLAG) {
        const auto *sendFlag = dynamic_cast<const TaskAivSendFlag *>(node);
        if (sendFlag == nullptr) {
            return HCCL_E_PTR;
        }
        flag = &sendFlag->GetFlag();
    } else if (node->GetType() == TaskType::AIV_RECV_FLAG) {
        const auto *recvFlag = dynamic_cast<const TaskAivRecvFlag *>(node);
        if (recvFlag == nullptr) {
            return HCCL_E_PTR;
        }
        flag = &recvFlag->GetFlag();
    }
    if (flag == nullptr) {
        return HCCL_SUCCESS;
    }
    if (flag->flagOffset > (std::numeric_limits<uint64_t>::max() - sizeof(int32_t)) / AIV_FLAG_SLOT_BYTES) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] AIV flag offset overflow, node={}", node->Describe());
        return HCCL_E_PARA;
    }
    const uint64_t flagByteOffset = flag->flagOffset * AIV_FLAG_SLOT_BYTES;
    const uint64_t flagBufferSize = GetAivSliceBoundSize(MemType::FLAG_AIV);
    if (flagBufferSize != 0 && flagByteOffset + sizeof(int32_t) > flagBufferSize) {
        HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] AIV flag offset out-of-bounds, node={}, "
            "flagByteOffset={}, flagBufferSize={}", node->Describe(), flagByteOffset, flagBufferSize);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult CheckDataMoveTaskMem(const TaskNode *node)
{
    if (node == nullptr) {
        return HCCL_E_PTR;
    }
    if (node->GetType() == TaskType::TRANS_MEM) {
        const auto *transMem = dynamic_cast<const TaskTransMem *>(node);
        if (transMem == nullptr) {
            return HCCL_E_PTR;
        }
        HcclResult ret = CheckSingleSlice(node, transMem->GetSrc());
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        ret = CheckSingleSlice(node, transMem->GetDst());
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        return CheckTwoSliceOverlap(node, transMem->GetSrc(), transMem->GetDst());
    }

    if (node->GetType() == TaskType::REDUCE) {
        const auto *reduce = dynamic_cast<const TaskReduce *>(node);
        if (reduce == nullptr) {
            return HCCL_E_PTR;
        }
        HcclResult ret = CheckSingleSlice(node, reduce->GetDst());
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        const std::vector<MemSlice> &srcs = reduce->GetSrcs();
        if (srcs.empty()) {
            HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Reduce source list is empty, node={}", node->Describe());
            return HCCL_E_PARA;
        }
        for (const auto &src : srcs) {
            ret = CheckSingleSlice(node, src);
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
            ret = CheckTwoSliceOverlap(node, src, reduce->GetDst());
            if (ret != HCCL_SUCCESS) {
                return ret;
            }
        }
        return HCCL_SUCCESS;
    }

    if (node->GetType() == TaskType::BATCH_TRANS_MEM) {
        const auto *transMem = dynamic_cast<const TaskBatchTransMem *>(node);
        if (transMem == nullptr) {
            return HCCL_E_PTR;
        }
        HcclResult ret = CheckBatchTransSlices(node, transMem->GetSrcs(), transMem->GetDsts(),
            transMem->GetMergedSrcs(), transMem->GetMergedDsts(), "original");
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        return HCCL_SUCCESS;
    }

    if (node->GetType() == TaskType::BATCH_REDUCE) {
        const auto *reduce = dynamic_cast<const TaskBatchReduce *>(node);
        if (reduce == nullptr) {
            return HCCL_E_PTR;
        }
        SliceCoverageSummary originalSrcSummary;
        SliceCoverageSummary originalDstSummary;
        HcclResult ret = CheckBatchReduceSlices(node, reduce->GetSrcs(), reduce->GetDsts(), "original",
            originalSrcSummary, originalDstSummary);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        return HCCL_SUCCESS;
    }

    HcclResult ret = HCCL_SUCCESS;
    if (node->GetType() == TaskType::AIV_SEND_FLAG || node->GetType() == TaskType::AIV_RECV_FLAG) {
        ret = CheckAivFlagNodeMem(node);
    }
    return ret;
}
} // namespace

HcclResult CheckSlaveTaskQueue(const std::vector<std::unique_ptr<TaskNode>> &nodes,
    const AllRankNodeQueues &allRankTaskQueues)
{
    for (const auto &rankEntry : allRankTaskQueues) {
        const RankId rankId = rankEntry.first;
        const RankNodeQueues &taskQueue = rankEntry.second;
        const size_t queueNum = taskQueue.size();
        for (size_t streamId = 1; streamId < queueNum; ++streamId) {
            const auto &stream = taskQueue[streamId];
            const size_t taskSize = stream.size();
            if (StreamHasAivGraph(nodes, stream)) {
                HCCL_VM_DEBUG("[TaskGraphSingleTaskCheckV3] Skip legacy slave stream shape check for AIV stream, "
                    "rank={}, stream={}", rankId, streamId);
                continue;
            }
            if (taskSize < 2) {
                return HCCL_SUCCESS;
            }

            const TaskNode *firstTask = GetQueueNode(nodes, stream.front());
            const TaskNode *lastTask = GetQueueNode(nodes, stream.back());
            if (firstTask == nullptr || lastTask == nullptr) {
                HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Invalid slave stream node, rank={}, stream={}",
                    rankId, streamId);
                return HCCL_E_PTR;
            }

            size_t backStep = 1;
            while (IsEmptyLocalCopy(lastTask) && backStep < taskSize) {
                lastTask = GetQueueNode(nodes, stream[taskSize - 1 - backStep]);
                if (lastTask == nullptr) {
                    HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] Invalid slave stream tail node, rank={}, stream={}",
                        rankId, streamId);
                    return HCCL_E_PTR;
                }
                ++backStep;
            }

            if (!IsLocalAicpuWait(firstTask)) {
                HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] RankId:{}, streamId:{} first task type should be "
                    "local WAIT, while is {}", rankId, streamId, TaskTypeName(firstTask->GetType()));
                return HCCL_E_INTERNAL;
            }

            if (!IsLocalAicpuRecord(lastTask)) {
                HCCL_VM_ERROR("[TaskGraphSingleTaskCheckV3] RankId:{}, streamId:{} last task type should be "
                    "local RECORD, while is {}", rankId, streamId, TaskTypeName(lastTask->GetType()));
                return HCCL_E_INTERNAL;
            }
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CheckTaskMem(const TaskNode *dummyStart, SingleTaskCheckStats *stats)
{
    if (dummyStart == nullptr) {
        return HCCL_E_PTR;
    }

    // 第一阶段先用 BFS 收集整张可达子图，避免同一个 DAG 节点被重复检查。
    std::queue<const TaskNode *> candNode;
    std::set<const TaskNode *> visitedNodes;
    std::set<const TaskNode *> simulatedNodes;
    for (const TaskNode *child : dummyStart->GetChildren()) {
        if (child == nullptr) {
            return HCCL_E_PTR;
        }
        visitedNodes.insert(child);
        candNode.push(child);
    }

    while (!candNode.empty()) {
        const TaskNode *curNode = candNode.front();
        candNode.pop();
        simulatedNodes.insert(curNode);
        for (const TaskNode *child : curNode->GetChildren()) {
            if (child == nullptr) {
                return HCCL_E_PTR;
            }
            if (visitedNodes.count(child) != 0) {
                continue;
            }
            visitedNodes.insert(child);
            candNode.push(child);
        }
    }

    // 第二阶段逐节点做单点内存合法性检查。
    // 这里不依赖拓扑序，因为该阶段只校验任务自身的 src/dst 描述是否自洽。
    if (stats != nullptr) {
        stats->nodeCount = simulatedNodes.size() + 1U;
        stats->dataTaskNodeCount = 0;
        for (const TaskNode *node : simulatedNodes) {
            if (IsDataMoveTaskNode(node)) {
                ++stats->dataTaskNodeCount;
            }
        }
    }

    for (const TaskNode *node : simulatedNodes) {
        HcclResult ret = CheckDataMoveTaskMem(node);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }

    return HCCL_SUCCESS;
}
} // namespace TaskGraphGeneratorV3
} // namespace HcclSim
