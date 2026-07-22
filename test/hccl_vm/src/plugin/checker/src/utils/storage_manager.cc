/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "storage_manager.h"

#include <cstdint>
#include <cstdlib> // strtoull
#include <cstring>
#include <iostream>
#include <limits>
#include <nlohmann_json/json.hpp>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "binary_data_operator.h"
#include "ccu_all_rank_param_recorder.h"
#include "error_codes.h"
#include "sim_log.h"

extern std::map<RankId, std::map<u32, HcclSim::ChannelsPerDie>> g_allRankChannelInfo;

namespace HcclSim {
static const std::string PLUGIN_PATH = "/plugin";
static const std::string DATA_FILE_PATH = "/data";
static const std::string TASK_COLLECTION_FILE = "/%s_task.jsonl.gz";
static const std::string MEM_LAYOUT_FILE = "/%s_mem_layout.jsonl.gz";
static const std::string MODEL_FILE = "/%s_model.jsonl.gz";

static const std::string HCCLVM_TASK_DATA_FILE = "/%s_hcclvm_task_data.bin";
static const std::string HCCLVM_SYN_DATA_FILE = "/%s_hcclvm_syn_data.bin";
static const std::string HCCLVM_INSTR_DATA_FILE = "/%s_hcclvm_instr_data.bin";

namespace {
template <typename T>
bool ReadExtValue(const std::vector<uint8_t> &data, size_t &offset, T &value)
{
    if (offset > data.size() || data.size() - offset < sizeof(T)) {
        return false;
    }
    std::memcpy(&value, data.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

bool ReadVParamExtInfo(const std::vector<uint8_t> &data, uint32_t rankSize, VRankParam &param)
{
    if (rankSize == 0) {
        return false;
    }

    constexpr size_t fixedSize = sizeof(uint64_t);
    const uint64_t valueCount = static_cast<uint64_t>(rankSize) * 2U;
    if (valueCount > (std::numeric_limits<size_t>::max() - fixedSize) / sizeof(uint64_t)) {
        return false;
    }
    const size_t expectedSize = fixedSize + static_cast<size_t>(valueCount) * sizeof(uint64_t);
    if (data.size() != expectedSize) {
        return false;
    }

    VRankParam parsed;
    parsed.counts.resize(rankSize);
    parsed.displs.resize(rankSize);
    size_t offset = 0;
    if (!ReadExtValue(data, offset, parsed.localCount)) {
        return false;
    }
    for (uint32_t i = 0; i < rankSize; ++i) {
        if (!ReadExtValue(data, offset, parsed.counts[i])) {
            return false;
        }
    }
    for (uint32_t i = 0; i < rankSize; ++i) {
        if (!ReadExtValue(data, offset, parsed.displs[i])) {
            return false;
        }
    }
    param = std::move(parsed);
    return true;
}

HcclResult FinalizeVDataDes(CheckerParam &checkerParam, bool isAllGatherV)
{
    const uint32_t rankSize = checkerParam.rankSize;
    if (rankSize == 0 || checkerParam.vRankParams.size() < rankSize) {
        HCCL_VM_ERROR("Invalid V operator rank parameters, rankSize={}, reportedRanks={}", rankSize,
                      checkerParam.vRankParams.size());
        return HcclResult::HCCL_E_PARA;
    }

    std::vector<uint64_t> finalCounts(rankSize, 0);
    for (uint32_t rankId = 0; rankId < rankSize; ++rankId) {
        const VRankParam &param = checkerParam.vRankParams[rankId];
        if (param.counts.size() != rankSize || param.displs.size() != rankSize) {
            HCCL_VM_ERROR("Invalid V operator parameters reported by rank {}, countsSize={}, "
                          "displsSize={}, rankSize={}",
                          rankId, param.counts.size(), param.displs.size(), rankSize);
            return HcclResult::HCCL_E_PARA;
        }
        finalCounts[rankId] = param.localCount;
    }

    for (uint32_t sourceRank = 0; sourceRank < rankSize; ++sourceRank) {
        for (uint32_t targetRank = 0; targetRank < rankSize; ++targetRank) {
            const uint64_t reportedCount = isAllGatherV
                ? checkerParam.vRankParams[targetRank].counts[sourceRank]
                : checkerParam.vRankParams[sourceRank].counts[targetRank];
            if (reportedCount != finalCounts[isAllGatherV ? sourceRank : targetRank]) {
                HCCL_VM_ERROR("{} V count mismatch, sourceRank={}, targetRank={}, reportedCount={}, "
                              "expectedCount={}",
                              isAllGatherV ? "AllGather" : "ReduceScatter", sourceRank, targetRank,
                              reportedCount, finalCounts[isAllGatherV ? sourceRank : targetRank]);
                return HcclResult::HCCL_E_PARA;
            }
        }
    }

    std::vector<uint64_t> displs;
    displs.reserve(rankSize);
    uint64_t offset = 0;
    for (uint64_t count : finalCounts) {
        displs.push_back(offset);
        if (count > std::numeric_limits<uint64_t>::max() - offset) {
            HCCL_VM_ERROR("{} V count prefix sum overflows uint64", isAllGatherV ? "AllGather" : "ReduceScatter");
            return HcclResult::HCCL_E_PARA;
        }
        offset += count;
    }

    checkerParam.vDataDes = VDataDesTagInner{};
    checkerParam.vDataDes.dataType = static_cast<uint16_t>(checkerParam.dataType);
    checkerParam.vDataDes.count = rankSize;
    checkerParam.vDataDes.counts = std::move(finalCounts);
    checkerParam.vDataDes.displs = std::move(displs);
    return HcclResult::HCCL_SUCCESS;
}
} // namespace

void StorageManager::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mem_layout.clear();
    m_allRankChannelInfo.clear();
    m_checker_param = CheckerParam{};
    m_all2AllvSendMatrices.clear();
    m_synData = HcclVmSynData{};
    m_instrData = HcclVmInstrData{};
    m_taskMeataData = HcclVmTaskMetaData{};
    devType_ = DevType::DEV_TYPE_COUNT;
    g_allRankChannelInfo.clear();
}

void StorageManager::BeginOpGroup()
{
    m_checker_param = CheckerParam{};
    m_all2AllvSendMatrices.clear();
}

HcclResult StorageManager::Trans2CheckerParam(sim::OpDetailTab& detailTab, ::OpDetails& detail)
{
    devType_ = static_cast<DevType>(detailTab.devType);
    AllRankParamRecorder::Global()->devType_ = devType_;
    auto vRankParams = std::move(m_checker_param.vRankParams);
    m_checker_param = CheckerParam{};
    m_checker_param.vRankParams = std::move(vRankParams);
    m_checker_param.cmdType = static_cast<HcclCMDType>(detail.opType);
    m_checker_param.rankSize = detailTab.rankSize;
    m_checker_param.dataType = static_cast<HcclDataType>(detail.dataType);
    m_checker_param.dataCount = detail.opV1.count;
    m_checker_param.reduceType = static_cast<HcclReduceOp>(detail.reduceType);
    m_checker_param.srcRank  = detailTab.srcRank;
    m_checker_param.dstRank  = detailTab.dstRank;
    m_checker_param.root     = detailTab.root;
    m_checker_param.all2AllDataDes.sendType = detail.opV2.sendDataType;
    m_checker_param.all2AllDataDes.recvType = detail.opV2.recvDataType;
    m_checker_param.all2AllDataDes.sendCount = detail.opV2.sendCount;
    m_checker_param.all2AllDataDes.recvCount = detail.opV2.recvCount;
    m_checker_param.all2AllDataDes.count = 0;

    HcclCMDType curCmdType = static_cast<HcclCMDType>(detail.opType);
    const bool isVOp = curCmdType == HcclCMDType::HCCL_CMD_ALLGATHER_V ||
                       curCmdType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V;
    if (isVOp) {
        if (detailTab.rankId >= detailTab.rankSize) {
            HCCL_VM_ERROR("Invalid V operator rank id {}, rankSize={}", detailTab.rankId, detailTab.rankSize);
            return HcclResult::HCCL_E_PARA;
        }
        if (m_checker_param.vRankParams.size() < detailTab.rankSize) {
            m_checker_param.vRankParams.resize(detailTab.rankSize);
        }
        VRankParam &rankParam = m_checker_param.vRankParams[detailTab.rankId];
        if (!rankParam.counts.empty()) {
            HCCL_VM_ERROR("Duplicate V operator parameter report from rank {}", detailTab.rankId);
            return HcclResult::HCCL_E_PARA;
        }
        if (!ReadVParamExtInfo(detailTab.opExtInfo, detailTab.rankSize, rankParam)) {
            HCCL_VM_ERROR("Invalid V operator opExtInfo, opType={}, rankId={}, rankSize={}, payloadSize={}",
                          static_cast<uint32_t>(curCmdType), detailTab.rankId, detailTab.rankSize,
                          detailTab.opExtInfo.size());
            return HcclResult::HCCL_E_PARA;
        }
        if (detail.opV1.count != rankParam.localCount) {
            HCCL_VM_ERROR("V operator local count mismatch, opType={}, rankId={}, detailCount={}, extInfoCount={}",
                          static_cast<uint32_t>(curCmdType), detailTab.rankId, detail.opV1.count,
                          rankParam.localCount);
            return HcclResult::HCCL_E_PARA;
        }
    } else if ((curCmdType == HcclCMDType::HCCL_CMD_ALLTOALL ||
                curCmdType == HcclCMDType::HCCL_CMD_ALLTOALLV ||
                curCmdType == HcclCMDType::HCCL_CMD_ALLTOALLVC) &&
               detailTab.opExtInfo.size() >= sizeof(uint32_t)) {
        uint32_t count = 0;
        std::memcpy(&count, detailTab.opExtInfo.data(), sizeof(uint32_t));
        m_checker_param.all2AllDataDes.count = count;

        std::vector<uint64_t> currentMatrix;
        for (uint32_t i = 0; i < count; i++) {
            uint64_t val = 0;
            size_t offset = sizeof(uint32_t) + i * sizeof(uint64_t);
            if (offset + sizeof(uint64_t) <= detailTab.opExtInfo.size()) {
                std::memcpy(&val, detailTab.opExtInfo.data() + offset, sizeof(uint64_t));
            }
            currentMatrix.push_back(val);
        }

        bool needMerge = (curCmdType == HcclCMDType::HCCL_CMD_ALLTOALL ||
                          curCmdType == HcclCMDType::HCCL_CMD_ALLTOALLV ||
                          curCmdType == HcclCMDType::HCCL_CMD_ALLTOALLVC);

        if (needMerge) {
            m_all2AllvSendMatrices[detailTab.rankId] = currentMatrix;
        }
        m_checker_param.all2AllDataDes.sendCountMatrix = currentMatrix;
    }

    HCCL_VM_INFO("opIter={}, rankId={}", detailTab.opIter, detailTab.rankId);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult StorageManager::FinalizeOpGroup()
{
    switch (m_checker_param.cmdType) {
        case HcclCMDType::HCCL_CMD_ALLGATHER_V:
            return FinalizeVDataDes(m_checker_param, true);
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V:
            return FinalizeVDataDes(m_checker_param, false);
        default:
            return HcclResult::HCCL_SUCCESS;
    }
}

void StorageManager::MergeAll2AllVSendCountMatrix()
{
    if (m_all2AllvSendMatrices.empty()) {
        return;
    }

    uint32_t rankSize = m_checker_param.rankSize;
    uint32_t matrixSize = rankSize * rankSize;
    uint32_t numRanks = static_cast<uint32_t>(m_all2AllvSendMatrices.size());
    std::vector<uint64_t> mergedMatrix(matrixSize, 0);

    for (auto& pair : m_all2AllvSendMatrices) {
        const auto& partial = pair.second;
        if (partial.size() == matrixSize) {
            for (size_t i = 0; i < matrixSize; i++) {
                mergedMatrix[i] |= partial[i];
            }
        }
    }

    m_checker_param.all2AllDataDes.sendCountMatrix = mergedMatrix;
    m_checker_param.all2AllDataDes.count = matrixSize;
    m_all2AllvSendMatrices.clear();

    HCCL_VM_INFO("Merged {} ranks into {}x{} matrix", numRanks, rankSize, rankSize);
}

HcclResult StorageManager::LoadHcclVmSynthesisData(uint32_t rankId, sim::OpMemInfoTab memInfo, std::vector<sim::CcuChannelTab>& channels)
{
    // 转换channel映射表
    for (auto &channel : channels) {
        RemoteDieInfo rmtDieInfo1;
        rmtDieInfo1.dstRank = channel.dstRankId;
        rmtDieInfo1.remoteDieId = channel.dstDieId;
        HCCL_VM_INFO("[Channel info] channelId= {}, srcRank= {}, srcDie= {}, dstRank= {}, dstDie= {}",
            channel.channelId, channel.srcRankId, static_cast<uint32_t>(channel.srcDieId), channel.dstRankId, channel.dstDieId);
        g_allRankChannelInfo[channel.srcRankId][channel.srcDieId][channel.channelId] = rmtDieInfo1;
    }

    // 构造memory layout
    if (memInfo.inputAddr != 0 && memInfo.inputSize > 0) {
        MemBlock memBlock;
        memBlock.bufferType = BufferType::INPUT;
        memBlock.startAddr = memInfo.inputAddr;
        memBlock.size = memInfo.inputSize;
        memBlock.globalOffset = 0; // todo: 预期一个rank只有一个同类型的buffer时，globalOffset为0。若有多个，需要按照下面json方案计算
        m_mem_layout[rankId][BufferType::INPUT][memInfo.inputAddr] = memBlock;
        HCCL_VM_INFO("[Init MemLayout] rank{}, bufType= {}, startAddr={}, size={}, globalOffset= {}",
                     rankId, static_cast<int>(BufferType::INPUT), memInfo.inputAddr, memInfo.inputSize, memBlock.globalOffset);
    }

    if (memInfo.outputAddr != 0 && memInfo.outputSize > 0) {
        MemBlock memBlock;
        memBlock.bufferType = BufferType::OUTPUT;
        memBlock.startAddr = memInfo.outputAddr;
        memBlock.size = memInfo.outputSize;
        memBlock.globalOffset = 0; // todo: 预期一个rank只有一个同类型的buffer时，globalOffset为0。若有多个，需要按照下面json方案计算
        m_mem_layout[rankId][BufferType::OUTPUT][memInfo.outputAddr] = memBlock;
        HCCL_VM_INFO("[Init MemLayout] rank{}, bufType= {}, startAddr={}, size={}, globalOffset= {}",
                     rankId, static_cast<int>(BufferType::OUTPUT), memInfo.outputAddr, memInfo.outputSize, memBlock.globalOffset);
    }

    if (memInfo.cclAddr != 0 && memInfo.cclSize > 0) {
        MemBlock memBlock;
        memBlock.bufferType = BufferType::CCL;
        memBlock.startAddr = memInfo.cclAddr;
        memBlock.size = memInfo.cclSize;
        memBlock.globalOffset = 0; // todo: 预期一个rank只有一个同类型的buffer时，globalOffset为0。若有多个，需要按照下面json方案计算
        m_mem_layout[rankId][BufferType::CCL][memInfo.cclAddr] = memBlock;
        HCCL_VM_INFO("[Init MemLayout] rank{}, bufType= {}, startAddr={}, size={}, globalOffset= {}",
                     rankId, static_cast<int>(BufferType::CCL), memInfo.cclAddr, memInfo.cclSize, memBlock.globalOffset);
    }
    return HcclResult::HCCL_SUCCESS;
}

void StorageManager::InitCcuInfo(DevType &devType, std::vector<uint64_t> &resourceBaseAddr)
{
    devType = devType_;
    resourceBaseAddr.clear();
    resourceBaseAddr.push_back(0x123123123);
    resourceBaseAddr.push_back(0x456456456);
}

HcclResult StorageManager::LoadHcclVmInstrData(std::vector<sim::CcuInstrResTab>& instrRes)
{
    for (auto& instr : instrRes) {
        if (instr.instrCount == 0 || instr.instrCount > 32 * 1024) {
            HCCL_VM_WARN("invalid instrCount={}, rankId={}, dieId={}",
                instr.instrCount, instr.rankId, static_cast<uint32_t>(instr.dieId));
            continue;
        }

        MicrocodeInstrInner instrInner;
        instrInner.desc.rank_id = instr.rankId;
        instrInner.desc.die_id  = static_cast<uint8_t>(instr.dieId);
        instrInner.desc.count   = static_cast<uint16_t>(instr.instrCount);

        instrInner.data.reserve(instr.instrCount);
        for (uint32_t i = 0; i < instr.instrCount; ++i) {
            hcomm::CcuRep::CcuInstr ccuInstr;
            std::memcpy(&ccuInstr, instr.instrSpace[i], sizeof(hcomm::CcuRep::CcuInstr));
            instrInner.data.push_back(ccuInstr);
        }

        m_instrData.instr_data.push_back(std::move(instrInner));

        HCCL_VM_INFO("rankId={}, dieId={}, count={}",
            instr.rankId, static_cast<uint32_t>(instr.dieId), instr.instrCount);
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult StorageManager::LoadHcclVmTaskMetaData(std::vector<std::vector<sim::OpTaskTab>>& allTasks)
{
    size_t totalTasks = 0;
    for (const auto& rankTasks : allTasks) {
        totalTasks += rankTasks.size();
    }
    HCCL_VM_INFO("total tasks: {}, ranks: {}", totalTasks, allTasks.size());
    HcclVmTaskMetaData taskMeataData;
    for (const auto& rankTasks : allTasks) {
        for (const auto &task : rankTasks) {
            if (task.optaskMeta.size() >= sizeof(HcclTaskMetaData)) {
                HcclTaskMetaData metaData;
                std::memcpy(&metaData, task.optaskMeta.data(), sizeof(HcclTaskMetaData));
                taskMeataData.task_meta.push_back(metaData);
            } else {
                HCCL_VM_WARN("optaskMeta too small, taskSeq={} src:{:d}, dst:{:d}", task.taskSeq, task.optaskMeta.size(), sizeof(HcclTaskMetaData));
            }
        }
    }
    m_taskMeataData = taskMeataData;
    std::unordered_map<uint32_t, std::set<uint32_t>> notifyId2Ranks;
    for (auto &taskMeta : m_taskMeataData.task_meta) {
        if (taskMeta.taskType == HccLTaskMetaType::NOTIFY_RECORD || taskMeta.taskType == HccLTaskMetaType::NOTIFY_WAIT) {
            uint64_t notifyId = taskMeta.taskData.notify.notifyId;
            notifyId2Ranks[notifyId].insert(taskMeta.rankId);
        }
    }

    // AICPU生成的Task需要更新Notify节点的对端信息
    for (auto &taskMeta : m_taskMeataData.task_meta) {
        if (taskMeta.taskType != HccLTaskMetaType::NOTIFY_RECORD && taskMeta.taskType != HccLTaskMetaType::NOTIFY_WAIT) {
            continue;
        }
        uint32_t rankId = taskMeta.rankId;
        for (auto id : notifyId2Ranks[taskMeta.taskData.notify.notifyId]) {
            if (id != rankId) {
                rankId = id;
                break;
            }
        }

        if (taskMeta.taskType == HccLTaskMetaType::NOTIFY_RECORD) {
            taskMeta.taskData.notify.dstRankId = rankId;
        } else if (taskMeta.taskType == HccLTaskMetaType::NOTIFY_WAIT) {
            taskMeta.taskData.notify.srcRankId = rankId;
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

uint64_t StorageManager::GetBlockSize(uint32_t rankId, BufferType bufferType) {
    // 1. 定位 Rank
    auto rankIt = m_mem_layout.find(rankId);
    if (rankIt == m_mem_layout.end()) {
        HCCL_VM_INFO("Cannot find rank id from memory layout");
        return 0;
    }

    // 2. 定位 BufferType
    auto typeIt = rankIt->second.find(bufferType);
    if (typeIt == rankIt->second.end()) {
        HCCL_VM_INFO("Cannot find buffer type from memory layout");
        return 0;
    }

    // 3. 获取该类型下的最后一个块 (map 的 rbegin)
    const auto& addrMap = typeIt->second;
    if (addrMap.empty()) {
        HCCL_VM_INFO("Cannot find addr from memory layout");
        return 0;
    }

    // map 是有序的，rbegin() 指向起始地址最大的那个块
    const MemBlock& lastBlock = addrMap.rbegin()->second;

    // 总大小 = 最后一个块的逻辑起始偏移 + 最后一个块的大小
    return lastBlock.globalOffset + lastBlock.size;
}

HcclResult StorageManager::GetSlice(uint64_t addr, uint64_t len, DataSlice& dataSlice, uint32_t* rank)
{
    dataSlice.SetSize(len);

    for (auto &rankMem : m_mem_layout) {
        // 2. 遍历该 Rank 下的所有 Buffer 类型 (INPUT, OUTPUT, CCL...)
        // typeEntry.first 是 BufferType, typeEntry.second 是 addrMap
        for (auto const& typeEntry : rankMem.second) {
            const auto& addrMap = typeEntry.second;

            // 3. 使用 upper_bound 在当前类型的地址图中快速查找
            // 找到第一个起始地址大于 addr 的块，那么目标块就是它的前一个
            auto it = addrMap.upper_bound(addr);
            if (it != addrMap.begin()) {
                --it;
                const MemBlock& block = it->second;

                // 4. 边界检查：确认物理地址 addr 是否落在该块 [start, start + size) 内
                if (addr >= block.startAddr && addr < (block.startAddr + block.size)) {
                    // 校验区间完整性（可选）：确保整个 size 都在这个块内
                    // 如果允许跨块，逻辑会更复杂，这里按单块逻辑处理
                    
                    dataSlice.SetBufferType(block.bufferType);
                    // 核心转换公式：逻辑基址 + (物理地址 - 物理块基址)
                    dataSlice.SetOffset(block.globalOffset + (addr - block.startAddr));
                    if (rank != nullptr) {
                        *rank = rankMem.first;
                    }
                    
                    return HcclResult::HCCL_SUCCESS;
                }
            }
        }
    }

    HCCL_VM_ERROR("{} Failed to resolve data slice from memory layout, addr=0x{:X}, len=0x{:X}",
        MakeErrorCodeText(ErrorCode::GRAPH_ADDRESS_INVALID), addr, len);
    return HcclResult::HCCL_E_MEMORY;
}

std::string StorageManager::FindRootPath()
{
    // 使用相对路径前缀：.  ./..  ./../..
    std::string current_search_path = ".";
    for (int i = 0; i <= 3; ++i) {
        char abs_path[PATH_MAX];
        // 尝试获取当前探测点的绝对路径
        if (realpath(current_search_path.c_str(), abs_path) != nullptr) {
            std::string check_target = std::string(abs_path) + PLUGIN_PATH;

            // 检查该绝对路径下的 plugin 目录是否存在
            if (IsDirExists(check_target)) {
                return std::string(abs_path);
            }
        } else {
            // 如果 realpath 失败（例如路径被删除或权限不足）
            HCCL_VM_ERROR("Iteration {}: realpath failed for {}", i, current_search_path);
        }

        // 没找到，将探测路径向上推一级
        current_search_path += "/..";
    }

    HCCL_VM_INFO("RootPath NOT found.");
    return ""; 
}

bool StorageManager::IsDirExists(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return false; // 不存在
    }
    return (info.st_mode & S_IFDIR); // 存在且是目录
}

uint32_t StorageManager::GetRankSize() const
{
    return m_checker_param.rankSize;
}

HcclVmInstrData StorageManager::GetHvmInstrData() const
{
    return m_instrData;
}

HcclVmTaskMetaData StorageManager::GetHvmTaskMetaData() const
{
    return m_taskMeataData;
}
}
