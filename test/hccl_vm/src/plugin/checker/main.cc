/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>
#include <utility>
#include <nlohmann_json/json.hpp>
#include "dump/dump_manager.h"
#include "dump/dump_run_manifest.h"
#include "dump/validation_issue_recorder.h"
#include "dump_v3/dump_v3_manager.h"
#include "setting_manager.h"
#include "storage_manager.h"
#include "task_utils.h"
#include "checker.h"
#include "hccl_verifier.h"
#include "sim_log.h"
#include "ccu_all_rank_param_recorder.h"
#include "framework/task_graph_generator_v3/ccu_graph_generator_v3/ccu_all_rank_param_recorder_v3.h"
#include "mem_conflict_check_utils.h"
#include "sim_loader.h"
#include "sim_common_defs.h"

using json = nlohmann::json;
loader::Loader g_loader;

// 使用原子变量控制程序生命周期
std::atomic<bool> g_keep_running{true};
std::mutex g_run_checker_mutex;
std::mutex g_worker_mutex;
std::thread g_worker_thread;

static std::vector<std::map<uint32_t, sim::CompositeOpDetail>> TransposeCompositeOpMap(
    const std::map<uint32_t, std::vector<sim::CompositeOpDetail>>& compositeDataMap)
{
    size_t maxOps = 0;
    for (const auto& entry : compositeDataMap) {
        maxOps = std::max(maxOps, entry.second.size());
    }
    std::vector<std::map<uint32_t, sim::CompositeOpDetail>> opGroups(maxOps);
    for (const auto& entry : compositeDataMap) {
        uint32_t rankId = entry.first;
        const auto& ops = entry.second;
        for (size_t i = 0; i < ops.size(); i++) {
            opGroups[i][rankId] = ops[i];
        }
    }
    return opGroups;
}

json BuildOpParamSummaryJson(const HcclSim::CheckerParam &param);

static bool IsAivOpExpansionMode(uint32_t opExpansionMode)
{
    constexpr uint32_t SIM_OP_EXPANSION_MODE_AIV = 2U;
    return opExpansionMode == SIM_OP_EXPANSION_MODE_AIV;
}

static bool HasAivGraphTask(const std::vector<std::vector<sim::OpTaskTab>> &allTasks)
{
    for (const auto &rankTasks : allTasks) {
        for (const auto &task : rankTasks) {
            if (task.optaskMeta.size() < sizeof(HcclTaskMetaData)) {
                continue;
            }
            HcclTaskMetaData metaData;
            std::memcpy(&metaData, task.optaskMeta.data(), sizeof(HcclTaskMetaData));
            if (metaData.taskType == HccLTaskMetaType::AIV_GRAPH) {
                return true;
            }
        }
    }
    return false;
}

static const char* HcclCmdTypeToString(HcclCMDType t) {
    switch (t) {
        case HCCL_CMD_BROADCAST:       return "Broadcast";
        case HCCL_CMD_ALLREDUCE:       return "AllReduce";
        case HCCL_CMD_REDUCE:          return "Reduce";
        case HCCL_CMD_SEND:            return "Send";
        case HCCL_CMD_RECEIVE:         return "Recv";
        case HCCL_CMD_ALLGATHER:       return "AllGather";
        case HCCL_CMD_REDUCE_SCATTER:  return "ReduceScatter";
        case HCCL_CMD_ALLTOALLV:       return "AllToAllV";
        case HCCL_CMD_ALLTOALLVC:      return "AllToAllVC";
        case HCCL_CMD_ALLTOALL:        return "AllToAll";
        case HCCL_CMD_SCATTER:         return "Scatter";
        case HCCL_CMD_BATCH_SEND_RECV: return "BatchSendRecv";
        case HCCL_CMD_ALLGATHER_V:     return "AllGatherV";
        case HCCL_CMD_REDUCE_SCATTER_V: return "ReduceScatterV";
        default: return "Unknown";
    }
}

static const char* HcclDataTypeToString(HcclDataType t) {
    switch (t) {
        case HCCL_DATA_TYPE_INT8:   return "INT8";
        case HCCL_DATA_TYPE_INT16:  return "INT16";
        case HCCL_DATA_TYPE_INT32:  return "INT32";
        case HCCL_DATA_TYPE_FP16:   return "FP16";
        case HCCL_DATA_TYPE_FP32:   return "FP32";
        case HCCL_DATA_TYPE_INT64:  return "INT64";
        case HCCL_DATA_TYPE_UINT64: return "UINT64";
        case HCCL_DATA_TYPE_UINT8:  return "UINT8";
        case HCCL_DATA_TYPE_UINT16: return "UINT16";
        case HCCL_DATA_TYPE_UINT32: return "UINT32";
        case HCCL_DATA_TYPE_FP64:   return "FP64";
        case HCCL_DATA_TYPE_BFP16:  return "BFP16";
        default: return "Unknown";
    }
}

static const char* HcclReduceOpToString(HcclReduceOp t) {
    switch (t) {
        case HCCL_REDUCE_SUM:  return "SUM";
        case HCCL_REDUCE_PROD: return "PROD";
        case HCCL_REDUCE_MAX:  return "MAX";
        case HCCL_REDUCE_MIN:  return "MIN";
        default: return "Unknown";
    }
}

static HcclResult LoadCheckerDataBase(
    std::vector<sim::CcuChannelTab>& channels,
    std::vector<sim::CcuInstrResTab>& instrRes,
    std::vector<sim::SyncRecordTab>& syncRecords,
    uint32_t& syncIterMaxNum)
{
   HcclResult ret = g_loader.GetCcuChannelInfo(channels);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[LoadCheckerDataBase] GetCcuChannelInfo failed, ret[{}]", static_cast<u32>(ret));
        return ret;
    }

    ret = g_loader.GetInstrResInfo(instrRes);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[LoadCheckerDataBase] GetInstrResInfo failed, ret[{}]", static_cast<u32>(ret));
        return ret;
    }

    ret = g_loader.GetSyncInfo(syncRecords);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[LoadCheckerDataBase] GetSyncInfo failed, ret[{}]", static_cast<u32>(ret));
        return ret;
    }

    if (syncRecords.empty()) {
        HCCL_VM_WARN("[LoadCheckerDataBase] No sync records found");
        return HcclResult::HCCL_E_PARA;
    }

    std::sort(syncRecords.begin(), syncRecords.end(), [](const sim::SyncRecordTab& a, const sim::SyncRecordTab& b) {
        return a.syncIter < b.syncIter;
    });

    syncIterMaxNum = syncRecords.back().syncIter;
    HCCL_VM_INFO("[LoadCheckerDataBase] channels={}, instrRes={}, syncRecords={}, syncIterMaxNum={}",
                 channels.size(), instrRes.size(), syncRecords.size(), syncIterMaxNum);
    return HcclResult::HCCL_SUCCESS;
}

static HcclResult DispatchCheckByCmdType(
    HcclSim::AllRankTaskQueues& taskQueues,
    HcclSim::CheckerParam& param)
{
    HcclCMDType cmdType = param.cmdType;
    switch (cmdType) {
        case HcclCMDType::HCCL_CMD_ALLREDUCE:
            return CheckAllReduce(taskQueues, param.rankSize, param.dataType, param.dataCount, param.reduceType);
        case HcclCMDType::HCCL_CMD_ALLGATHER:
            return CheckAllGather(taskQueues, param.rankSize, param.dataType, param.dataCount);
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER:
            return CheckReduceScatter(taskQueues, param.rankSize, param.dataType, param.dataCount, param.reduceType);
        case HcclCMDType::HCCL_CMD_SEND:
            return CheckSend(taskQueues, param.rankSize, param.dataType, param.dataCount, param.srcRank,
                             param.dstRank);
        case HcclCMDType::HCCL_CMD_RECEIVE:
            return CheckRecv(taskQueues, param.rankSize, param.dataType, param.dataCount, param.srcRank,
                             param.dstRank);
        case HcclCMDType::HCCL_CMD_BROADCAST:
            return CheckBroadcast(taskQueues, param.rankSize, param.dataType, param.dataCount, param.root);
        case HcclCMDType::HCCL_CMD_REDUCE:
            return CheckReduce(taskQueues, param.rankSize, param.dataType, param.dataCount, param.reduceType,
                               param.root);
        case HcclCMDType::HCCL_CMD_SCATTER:
            return CheckScatter(taskQueues, param.rankSize, param.dataType, param.dataCount, param.root);
        case HcclCMDType::HCCL_CMD_BATCH_SEND_RECV:
            return CheckBatchSendRecv(taskQueues, param.rankSize, param.dataType, param.dataCount);
        case HcclCMDType::HCCL_CMD_ALLGATHER_V:
            return CheckAllGatherV(taskQueues, param.rankSize, param.vDataDes);
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V:
            return CheckReduceScatterV(taskQueues, param.rankSize, param.reduceType, param.vDataDes);
        case HcclCMDType::HCCL_CMD_ALLTOALL:
            return CheckAll2All(taskQueues, param.rankSize,
                                static_cast<HcclDataType>(param.all2AllDataDes.sendType), param.all2AllDataDes.sendCount);
        case HcclCMDType::HCCL_CMD_ALLTOALLVC:
            return CheckAll2AllVC(taskQueues, param.rankSize,
                                  static_cast<HcclDataType>(param.all2AllDataDes.sendType), param.all2AllDataDes.sendCountMatrix);
        case HcclCMDType::HCCL_CMD_ALLTOALLV:
            HCCL_VM_WARN("CheckOpSemantics does not support HCCL_CMD_ALLTOALLV, using HCCL_CMD_ALLTOALLVV instead");
            return CheckAll2AllVC(taskQueues, param.rankSize,
                                  static_cast<HcclDataType>(param.all2AllDataDes.sendType), param.all2AllDataDes.sendCountMatrix);
        default:
            HCCL_VM_ERROR("Unknown HcclCMDType {}", static_cast<u32>(cmdType));
            return HcclResult::HCCL_E_NOT_SUPPORT;
    }
}

static HcclResult LoadOpDataForOneRank(
    HcclSim::StorageManager& storage,
    std::vector<sim::CcuChannelTab>& channels,
    std::vector<sim::CcuInstrResTab>& instrRes,
    uint32_t rankId,
    sim::CompositeOpDetail& op)
{
    HcclResult ret = storage.LoadHcclVmSynthesisData(rankId, op.memInfo, channels);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[LoadOpDataForOneRank] LoadHcclVmSynthesisData rankId={} failed, ret={}",
                      rankId, static_cast<u32>(ret));
        return ret;
    }

    ret = storage.LoadHcclVmInstrData(instrRes);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[LoadOpDataForOneRank] LoadHcclVmInstrData rankId={} failed, ret={}",
                      rankId, static_cast<u32>(ret));
        return ret;
    }

    if (op.detail.opDetail.size() < sizeof(::OpDetails)) {
        HCCL_VM_ERROR("[LoadOpDataForOneRank] opDetail BLOB too small, rankId={}", rankId);
        return HcclResult::HCCL_E_PARA;
    }

    ::OpDetails opDetails{};
    std::memcpy(&opDetails, op.detail.opDetail.data(), sizeof(::OpDetails));
    ret = storage.Trans2CheckerParam(op.detail, opDetails);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[LoadOpDataForOneRank] Trans2CheckerParam rankId={} failed, ret={}",
                      rankId, static_cast<u32>(ret));
        return ret;
    }
    return HcclResult::HCCL_SUCCESS;
}

static HcclResult ProcessOneOpGroup(
    HcclSim::StorageManager &storage,
    std::vector<sim::CcuChannelTab> &channels,
    std::vector<sim::CcuInstrResTab> &instrRes,
    uint32_t opIdx,
    std::map<uint32_t, sim::CompositeOpDetail> &opGroup)
{
    HcclSim::ValidationIssueRecorder::GetInstance().Reset();
    HcclSim::AllRankParamRecorder::Global()->Reset();
    HcclSim::TaskGraphGeneratorV3::AllRankParamRecorder::Global()->Reset();
    HcclSim::g_ccuGraphTaskOri2New.clear();
    HcclSim::DumpManager &dumpManager = HcclSim::DumpManager::GetInstance();
    HcclSim::SettingManager &settingManager = HcclSim::SettingManager::GetInstance();
    bool enableNewChecker = settingManager.IsNewCheckerEnabled();
    bool enableOldChecker = settingManager.IsOldCheckerEnabled();
    bool isAivOp = false;

    HCCL_VM_INFO("[RunChecker] opgroup size {}", opGroup.size());
    std::vector<std::vector<sim::OpTaskTab>> allTasks;
    for (auto& entry : opGroup) {
        uint32_t rankId = entry.first;
        HCCL_VM_INFO("[ProcessOneOpGroup] rankId={}", rankId);
        sim::CompositeOpDetail& op = entry.second;
        isAivOp = isAivOp || IsAivOpExpansionMode(op.detail.opExpansionMode);
        allTasks.push_back(op.tasks);
        HcclResult r = LoadOpDataForOneRank(storage, channels, instrRes, rankId, op);
        if (r != HcclResult::HCCL_SUCCESS) {
            return r;
        }
        if (dumpManager.IsEnabled()) {
            HcclSim::DumpRunManifest::GetInstance().SetOpParam(BuildOpParamSummaryJson(storage.GetCheckerParam()));
        }
    }

    if (!enableNewChecker && !enableOldChecker) {
        HCCL_VM_WARN("[ProcessOneOpGroup] both new checker and old checker are disabled, skip op[{}].", opIdx);
        return HcclResult::HCCL_SUCCESS;
    }

    // alltoallv alltoallvc需要进行矩阵合并
    storage.MergeAll2AllVSendCountMatrix();

    auto ret = storage.LoadHcclVmTaskMetaData(allTasks);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[ProcessOneOpGroup] LoadHcclVmTaskMetaData failed, ret={}",
                      static_cast<u32>(ret));
        return ret;
    }
    isAivOp = isAivOp || HasAivGraphTask(allTasks);
    if (isAivOp) {
        if (!enableNewChecker) {
            HCCL_VM_ERROR("[ProcessOneOpGroup] AIV checker requires V3 checker, but new checker is disabled.");
            return HcclResult::HCCL_E_NOT_SUPPORT;
        }
        if (enableOldChecker) {
            HCCL_VM_INFO("[ProcessOneOpGroup] AIV op detected, skip legacy checker and use V3-only.");
            enableOldChecker = false;
        }
        enableNewChecker = true;
    }

    {
        auto checkerParamBrief = storage.GetCheckerParam();
        HCCL_VM_INFO("[Checker][OpInfo] op[{}] op = {}, rankSize = {}, dataType = {}, dataCount = {}, reduceType = {}, "
                     "srcRank = {}, dstRank = {}, root = {}, opGroupSize={}, isAivOp={}",
                     opIdx, HcclCmdTypeToString(checkerParamBrief.cmdType), checkerParamBrief.rankSize,
                     HcclDataTypeToString(checkerParamBrief.dataType), checkerParamBrief.dataCount,
                     HcclReduceOpToString(checkerParamBrief.reduceType),
                     checkerParamBrief.srcRank, checkerParamBrief.dstRank, checkerParamBrief.root,
                     opGroup.size(), isAivOp);
    }

    HcclResult newCheckerRet = HcclResult::HCCL_SUCCESS;
    if (enableNewChecker) {
        newCheckerRet = HcclSim::GenAndCheckGraphV3();
        HCCL_VM_WARN("----------[V3 Checker Finished]----------");
        HCCL_VM_INFO("[ProcessOneOpGroup] new checker finished, ret={}", static_cast<u32>(newCheckerRet));
    } else {
        HCCL_VM_INFO("[ProcessOneOpGroup] new checker disabled by manifest setting.");
    }

    HcclResult oldCheckerRet = HcclResult::HCCL_SUCCESS;
    if (enableOldChecker) {
        HCCL_VM_WARN("----------[Start Old Checker]----------");
        HcclSim::AllRankTaskQueues taskQueues;
        HcclSim::ConvertTaskQueue(taskQueues);
        auto checkerParam = storage.GetCheckerParam();
        HCCL_VM_INFO("[ProcessOneOpGroup] dispatch legacy checker, rankCount={}, cmdType={}",
                     taskQueues.size(), static_cast<u32>(checkerParam.cmdType));
        oldCheckerRet = DispatchCheckByCmdType(taskQueues, checkerParam);
        HCCL_VM_INFO("[ProcessOneOpGroup] old checker finished, ret={}", static_cast<u32>(oldCheckerRet));
    } else {
        HCCL_VM_INFO("[ProcessOneOpGroup] old checker disabled by manifest setting.");
    }

    if (!isAivOp && enableNewChecker && enableOldChecker && newCheckerRet != oldCheckerRet) {
        HCCL_VM_WARN("[ProcessOneOpGroup] new checker and old checker results differ, newCheckerRet={}, oldCheckerRet={}",
                      static_cast<u32>(newCheckerRet), static_cast<u32>(oldCheckerRet));
    }
    if (enableNewChecker) {
        return newCheckerRet;
    }
    return oldCheckerRet;
}

json BuildOpParamSummaryJson(const HcclSim::CheckerParam &param)
{
    json opParamJson = json::object();
    opParamJson["cmd_type"] = static_cast<u32>(param.cmdType);
    opParamJson["rank_size"] = param.rankSize;
    opParamJson["data_type"] = static_cast<u32>(param.dataType);
    opParamJson["data_count"] = param.dataCount;

    if (param.cmdType == HcclCMDType::HCCL_CMD_ALLREDUCE ||
        param.cmdType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER ||
        param.cmdType == HcclCMDType::HCCL_CMD_REDUCE ||
        param.cmdType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V) {
        opParamJson["reduce_type"] = static_cast<u32>(param.reduceType);
    }

    if (param.cmdType == HcclCMDType::HCCL_CMD_SEND ||
        param.cmdType == HcclCMDType::HCCL_CMD_RECEIVE) {
        opParamJson["src_rank"] = param.srcRank;
        opParamJson["dst_rank"] = param.dstRank;
    }

    if (param.cmdType == HcclCMDType::HCCL_CMD_BROADCAST ||
        param.cmdType == HcclCMDType::HCCL_CMD_REDUCE ||
        param.cmdType == HcclCMDType::HCCL_CMD_SCATTER) {
        opParamJson["root"] = param.root;
    }

    if (param.cmdType == HcclCMDType::HCCL_CMD_ALLGATHER_V ||
        param.cmdType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V) {
        json vDataDesJson = json::object();
        vDataDesJson["data_type"] = param.vDataDes.dataType;
        vDataDesJson["rank_count"] = param.vDataDes.count;
        vDataDesJson["counts_size"] = param.vDataDes.counts.size();
        vDataDesJson["displs_size"] = param.vDataDes.displs.size();
        opParamJson["v_data_des"] = std::move(vDataDesJson);
    }

    if (param.cmdType == HcclCMDType::HCCL_CMD_ALLTOALL ||
        param.cmdType == HcclCMDType::HCCL_CMD_ALLTOALLVC ||
        param.cmdType == HcclCMDType::HCCL_CMD_ALLTOALLV) {
        json all2AllDataDesJson = json::object();
        all2AllDataDesJson["send_type"] = param.all2AllDataDes.sendType;
        all2AllDataDesJson["recv_type"] = param.all2AllDataDes.recvType;
        all2AllDataDesJson["send_count"] = param.all2AllDataDes.sendCount;
        all2AllDataDesJson["recv_count"] = param.all2AllDataDes.recvCount;
        all2AllDataDesJson["count"] = param.all2AllDataDes.count;
        all2AllDataDesJson["send_count_matrix_size"] = param.all2AllDataDes.sendCountMatrix.size();
        opParamJson["all2all_data_des"] = std::move(all2AllDataDesJson);
    }
    return opParamJson;
}

// --- 业务函数修正 ---
void RunChecker(const std::string& data_id) {
    std::lock_guard<std::mutex> runLock(g_run_checker_mutex);
    HcclSim::StorageManager& storage = HcclSim::StorageManager::GetInstance();
    storage.Reset();
    storage.SetDataId(data_id);
    const HcclResult settingRefreshRet = HcclSim::SettingManager::GetInstance().Refresh();
    if (settingRefreshRet != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_WARN("[RunChecker] failed to refresh manifest settings, keep previous settings, ret = {}",
                     static_cast<u32>(settingRefreshRet));
    }
    HcclSim::DumpManager &dumpManager = HcclSim::DumpManager::GetInstance();
    dumpManager.Reset();
    HcclResult dumpInitRet = dumpManager.Initialize(data_id);
    if (dumpInitRet != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[RunChecker] DumpManager initialize failed, ret = {}", static_cast<u32>(dumpInitRet));
        return;
    }
    HcclSim::DumpV3Manager &dumpV3Manager = HcclSim::DumpV3Manager::GetInstance();
    dumpV3Manager.Reset();
    dumpInitRet = dumpV3Manager.Initialize(data_id);
    if (dumpInitRet != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[RunChecker] DumpV3Manager initialize failed, ret = {}", static_cast<u32>(dumpInitRet));
        return;
    }
    HcclSim::DumpRunManifest::GetInstance().Reset(data_id);
    HcclSim::AllRankParamRecorder::Global()->Reset();
    HcclSim::TaskGraphGeneratorV3::AllRankParamRecorder::Global()->Reset();
    storage.InitCcuInfo(HcclSim::AllRankParamRecorder::Global()->devType_,
        HcclSim::AllRankParamRecorder::Global()->ccu_resource_base_addr_);
    HcclSim::g_ccuGraphTaskOri2New.clear();

    HcclResult loadRet = g_loader.LoadOpTaskFile();
    if (loadRet != HcclResult::HCCL_SUCCESS) {
        HCCL_VM_ERROR("[RunChecker] LoadOpTaskFile failed, ret = {}", static_cast<u32>(loadRet));
        return;
    }

    std::vector<sim::CcuChannelTab> channels;
    std::vector<sim::CcuInstrResTab> instrRes;
    std::vector<sim::SyncRecordTab> syncRecords;
    uint32_t syncIterMaxNum = 0;
    HcclResult ret = LoadCheckerDataBase(channels, instrRes, syncRecords, syncIterMaxNum);
    if (ret != HcclResult::HCCL_SUCCESS || syncRecords.empty()) {
        return;
    }

    HCCL_VM_INFO("[RunChecker] start check semantic: syncRecords size = {}", syncRecords.size());
    uint32_t opIdx = 0;
    do {
        for (uint32_t syncIter = 0; syncIter <= syncIterMaxNum; syncIter++) {
            std::map<uint32_t, std::vector<sim::CompositeOpDetail>> compositeDataMap;
            g_loader.LoadCompositeOpDetailBySyncIter(syncIter, compositeDataMap);
            auto opGroups = TransposeCompositeOpMap(compositeDataMap);
            HCCL_VM_INFO("[RunChecker] syncIter={}, opGroups={}", syncIter, opGroups.size());
            for (auto& opGroup : opGroups) {
                HCCL_VM_INFO("[RunChecker] opgroup size {}", opGroup.size());
                ret = ProcessOneOpGroup(storage, channels, instrRes, opIdx, opGroup);
                if (dumpManager.IsEnabled()) {
                    HcclSim::DumpRunManifest::GetInstance().SetCheckResult(ret);
                    const HcclResult flushRet = HcclSim::ValidationIssueRecorder::GetInstance().Flush();
                    if (flushRet != HcclResult::HCCL_SUCCESS) {
                        HCCL_VM_WARN("[RunChecker] failed to flush validation issue dump for op[{}], ret={}",
                                     opIdx, static_cast<u32>(flushRet));
                    }
                    const HcclResult manifestRet = HcclSim::DumpRunManifest::GetInstance().Flush();
                    if (manifestRet != HcclResult::HCCL_SUCCESS) {
                        HCCL_VM_WARN("[RunChecker] failed to flush dump manifest for op[{}], ret={}",
                                     opIdx, static_cast<u32>(manifestRet));
                    }
                }
                if (ret != HcclResult::HCCL_SUCCESS) {
                    HCCL_VM_ERROR("[RunChecker] op[{}] failed with error code {}", opIdx, static_cast<u32>(ret));
                    break;
                }
                HCCL_VM_INFO("[RunChecker] op[{}] Checker Success.", opIdx);
                opIdx++;
            }
        }
    } while (false);
    std::cout << "(hvm)$> " << std::flush;
    FlushLog(); // 将本轮完整日志落盘
}

void StartCheckerWorker(const std::string &dataId)
{
    std::lock_guard<std::mutex> workerLock(g_worker_mutex);
    if (g_worker_thread.joinable()) {
        g_worker_thread.join();
    }
    g_worker_thread = std::thread([dataId]() { RunChecker(dataId); });
}

void JoinCheckerWorker()
{
    std::lock_guard<std::mutex> workerLock(g_worker_mutex);
    if (g_worker_thread.joinable()) {
        g_worker_thread.join();
    }
}

// --- 分发函数修正 ---
// 不再使用 exit(0)，而是通过标记位通知主线程
void ProcessCommand(const std::string& line) {
    try {
        auto j = json::parse(line);
        const HcclResult settingRefreshRet = HcclSim::SettingManager::GetInstance().Refresh();
        if (settingRefreshRet != HcclResult::HCCL_SUCCESS) {
            HCCL_VM_WARN("[Checker] failed to refresh manifest settings, keep previous settings, ret = {}",
                         static_cast<u32>(settingRefreshRet));
        }
        std::string action = j.value("action", "");
        auto payload = j.value("payload", json::object());

        if (action == "status") {
            std::string status = payload.value("status", "");
            HCCL_VM_INFO("[Checker] Status signal received. Status: {}", status);
            if (status != "finish") {
                return;
            }
            std::string data_id = payload.value("data_id", "");
            StartCheckerWorker(data_id);
        } 
        else if (action == "stop") {
            HCCL_VM_INFO("[Checker] Stop signal received. Initiating shutdown...");
            g_keep_running.store(false); // 仅仅修改标志位
        }
        // 其他 action...
    } catch (const std::exception& e) {
        std::cerr << "[Error] Command processing failed: " << e.what() << std::endl;
    }
}

int main() {
    LogConfig config = LoadLogConfig("checker");
    InitLogger(config);

    HCCL_VM_INFO("[Checker] Plugin process active. Listening for commands...");

    // 主循环检查标志位
    std::string line;
    // 注意：std::getline 是阻塞的。如果 stop 指令后没有后续输入，
    // 循环会卡在 getline。但在插件管理场景下，发送完 stop 后通常会关闭管道，
    // 导致 getline 返回 false。
    while (g_keep_running.load() && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        ProcessCommand(line);
    }

    JoinCheckerWorker();
    HCCL_VM_INFO("[Checker] shutdown.");
    return 0; // 整个进程唯一的正常出口
}
