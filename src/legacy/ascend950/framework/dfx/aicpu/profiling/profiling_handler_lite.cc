/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "profiling_handler_lite.h"
#include "exception_util.h"
#include "internal_exception.h"
#include "sal.h"
#include "task_info.h"
#include "task_param.h"
#include "communicator_impl_lite.h"
namespace Hccl {

static constexpr u32 aging = 1;
constexpr std::uint32_t HCCLINFO_REPORT_BATCH_NUM = 2;
ProfilingHandlerLite ProfilingHandlerLite::instance_;

ProfilingHandlerLite::ProfilingHandlerLite()
{
    Init();
}

ProfilingHandlerLite::~ProfilingHandlerLite()
{
}

ProfilingHandlerLite &ProfilingHandlerLite::GetInstance()
{
    return instance_;
}

void ProfilingHandlerLite::SetCachedCclTag()
{
    for (const auto &item : CMD_OP_TYPE_INFO_MAP) {
        string tag = item.second.second;
        if (cachedCclTag_.find(tag) == cachedCclTag_.end()) {
            std::string cclTag = item.second.second;
            cachedCclTag_[tag] = GetProfHashId(cclTag.c_str(), cclTag.length());
        }
    }
}

void ProfilingHandlerLite::SetCachedGroupName(const DfxOpInfo &opInfo)
{
    if (opInfo.isIndop_) {
        cachedGroupName_ = GetProfHashId(opInfo.groupName_.c_str(), opInfo.groupName_.length());
        cachedRankSize_ = opInfo.rankSize_;
    } else if (opInfo.comm_ != nullptr) {
        CommunicatorImplLite *commImp = static_cast<CommunicatorImplLite *>(opInfo.comm_);
        cachedGroupName_ = GetProfHashId(commImp->GetId().c_str(), commImp->GetId().length());
        cachedRankSize_ = commImp->GetRankSize();
    }
}

void ProfilingHandlerLite::Init()
{
    cachedTid_ = SalGetTid();
    SetCachedCclTag();
    for (auto i = 0; i < static_cast<int>(TaskParamType::__COUNT__); i++) {
        TaskParamType type(static_cast<TaskParamType::Value>(i));
        std::string nameInfo = GetProfTaskOpNameV2(type);
        taskTypeHashCache_[static_cast<uint32_t>(i)] = GetProfHashId(nameInfo.c_str(), nameInfo.length());
    }
    if (MsprofReportBatchAdditionalInfo == nullptr) {
        if (AdprofReportAdditionalInfo != nullptr) {
            reportAdditionalInfo_ = AdprofReportAdditionalInfo;
        }
        if (AdprofGetHashId != nullptr) {
            getProfHashId_ = AdprofGetHashId;
        }
    } else {
        if (MsprofReportAdditionalInfo != nullptr) {
            reportAdditionalInfo_ = [](uint32_t flag, const void* data, uint32_t len) -> int32_t {
                return MsprofReportAdditionalInfo(flag, const_cast<void*>(data), len);
            };
        }
        if (MsprofStr2Id != nullptr) {
            getProfHashId_ = MsprofStr2Id;
        }
    }
}

void ProfilingHandlerLite::ReportHcclOpInfo(const DfxOpInfo &opInfo) const
{
    if (!GetProfL0State()) {
        HCCL_INFO("[ProfilingHandlerLite][ReportHcclOpInfo] l0 is false.");
        return;
    }
    MsprofAicpuHCCLOPInfo hcclOpInfo {};
    if (aicpu::GetTaskAndStreamId == nullptr) {
        HCCL_WARNING("[ProfilingHandlerLite][ReportHcclOpInfo] GetTaskAndStreamId is nullptr.");
        return;
    }
    uint64_t taskId   = 0U;
    uint32_t streamId = 0;
    if (aicpu::GetTaskAndStreamId(taskId, streamId) != aicpu::status_t::AICPU_ERROR_NONE) {
        THROW<InternalException>("[ProfilingHandler] Failed to get task id and stream id.");
    }
    hcclOpInfo.algType  = GetProfHashId(opInfo.algType_.c_str(), opInfo.algType_.length());
    if (taskId > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        THROW<InvalidParamsException>("[ProfilingHandler] taskId is larger than u32.");
    }
    hcclOpInfo.taskId   = static_cast<uint32_t>(taskId);
    hcclOpInfo.streamId = streamId;
    hcclOpInfo.count = opInfo.op_.dataCount;
    hcclOpInfo.dataType = opInfo.op_.dataType;
    hcclOpInfo.groupName = cachedGroupName_;
    HCCL_INFO("[ProfilingHandlerLite][ReportHcclOpInfo] relay:%u, retry:%u, dataType:%s, algType:%u, count:%llu, "
              "groupName:%lu, ranksize:%u, taskId:%u, streamId:%u",
              hcclOpInfo.relay, hcclOpInfo.retry, DataTypeToSerialString(hcclOpInfo.dataType).c_str(), hcclOpInfo.algType, hcclOpInfo.count,
              hcclOpInfo.groupName, hcclOpInfo.ranksize, hcclOpInfo.taskId, hcclOpInfo.streamId);
    MsprofAdditionalInfo reporterData{};
    reporterData.level     = MSPROF_REPORT_AICPU_LEVEL;
    reporterData.type      = MSPROF_REPORT_AICPU_HCCL_OP_INFO;
    reporterData.threadId  = cachedTid_;
    reporterData.dataLen   = sizeof(hcclOpInfo);
    reporterData.timeStamp = ProfGetCurCpuTimestamp();
    s32 sret               = memcpy_s(reporterData.data, sizeof(reporterData.data), &hcclOpInfo, sizeof(hcclOpInfo));
    if (sret != EOK) {
        THROW<InternalException>("Call memcpy_s failed, errorno[%d]", sret);
    }
    ReportAdditionInfo(reporterData);
}

void ProfilingHandlerLite::ReportHcclTaskDetailsLog(const std::vector<TaskInfo> &taskInfo) const
{
    if (HcclCheckLogLevel(HCCL_LOG_INFO) == 0) {
        return;
    }
    for (std::vector<Hccl::TaskInfo>::size_type i = 0; i < taskInfo.size(); i++) {
        DumpTaskDetails(MsprofAicpuHcclTaskInfo{}, taskInfo[i]);
    }
}

void ProfilingHandlerLite::ReportHcclTaskDetails(const std::vector<TaskInfo> &taskInfo) const
{
    ReportHcclTaskDetailsLog(taskInfo);
    MsprofAdditionalInfo reporterData{};
    reporterData.level     = MSPROF_REPORT_AICPU_LEVEL;
    reporterData.type      = MSPROF_REPORT_AICPU_MC2_BATCH_HCCL_INFO;
    reporterData.threadId  = cachedTid_;
    reporterData.timeStamp = 0;
    auto *taskDetailsInfos = reinterpret_cast<MsprofAicpuHcclTaskInfo *>(reporterData.data);
    uint32_t batchId = 0;
    for (std::vector<Hccl::TaskInfo>::size_type i = 0; i < taskInfo.size(); i++) {
        GetTaskDetailInfos(taskInfo[i], taskDetailsInfos[batchId++]);
        if (batchId == HCCLINFO_REPORT_BATCH_NUM || i == taskInfo.size() - 1) {
            reporterData.dataLen = sizeof(MsprofAicpuHcclTaskInfo) * batchId;
            ReportAdditionInfo(reporterData);
            batchId = 0;
            if (i == taskInfo.size() - 1) {  // 再多一次判断会更好吗？
                memset_s(reporterData.data, sizeof(reporterData.data), 0, sizeof(reporterData.data));
            }
        }
    }
}

void ProfilingHandlerLite::GetTaskDetailInfos(const TaskInfo &it, MsprofAicpuHcclTaskInfo &taskDetailsInfos) const 
{
    auto cacheIt = taskTypeHashCache_.find(static_cast<uint32_t>(it.taskParam_.taskType));
    taskDetailsInfos.itemId = (cacheIt != taskTypeHashCache_.end()) ? cacheIt->second : INVALID_U64;
    auto cclTagIt = cachedCclTag_.find(it.dfxOpInfo_->tag_);
    taskDetailsInfos.cclTag = (cclTagIt != cachedCclTag_.end()) ? cclTagIt->second : INVALID_U64;
    taskDetailsInfos.remoteRank   = it.GetRemoteRankId();
    taskDetailsInfos.groupName = cachedGroupName_;
    taskDetailsInfos.rankSize  = cachedRankSize_;
    taskDetailsInfos.localRank = it.dfxOpInfo_->op_.myRank;
    taskDetailsInfos.stage        = 0;
    if (it.taskParam_.taskType == TaskParamType::TASK_SDMA || it.taskParam_.taskType == TaskParamType::TASK_RDMA
        || it.taskParam_.taskType == TaskParamType::TASK_UB_INLINE_WRITE
        || it.taskParam_.taskType == TaskParamType::TASK_WRITE_WITH_NOTIFY
        || it.taskParam_.taskType == TaskParamType::TASK_UB) {
        taskDetailsInfos.srcAddr  = static_cast<u64>(reinterpret_cast<uintptr_t>(it.taskParam_.taskPara.DMA.src));
        taskDetailsInfos.dstAddr  = static_cast<u64>(reinterpret_cast<uintptr_t>(it.taskParam_.taskPara.DMA.dst));
        taskDetailsInfos.dataSize = static_cast<u32>(it.taskParam_.taskPara.DMA.size);
        taskDetailsInfos.notifyID = it.taskParam_.taskPara.DMA.notifyID;
        taskDetailsInfos.linkType = static_cast<uint16_t>(it.taskParam_.taskPara.DMA.linkType);
    } else if (it.taskParam_.taskType == TaskParamType::TASK_REDUCE_INLINE
               || it.taskParam_.taskType == TaskParamType::TASK_REDUCE_TBE
               || it.taskParam_.taskType == TaskParamType::TASK_UB_REDUCE_INLINE
               || it.taskParam_.taskType == TaskParamType::TASK_WRITE_REDUCE_WITH_NOTIFY) {
        taskDetailsInfos.srcAddr  = static_cast<u64>(reinterpret_cast<uintptr_t>(it.taskParam_.taskPara.Reduce.src));
        taskDetailsInfos.dstAddr  = static_cast<u64>(reinterpret_cast<uintptr_t>(it.taskParam_.taskPara.Reduce.dst));
        taskDetailsInfos.dataSize = static_cast<u32>(it.taskParam_.taskPara.Reduce.size);
        taskDetailsInfos.notifyID = it.taskParam_.taskPara.Reduce.notifyID;
        taskDetailsInfos.dataType = static_cast<uint16_t>(it.taskParam_.taskPara.Reduce.dataType);
        taskDetailsInfos.linkType = static_cast<uint16_t>(it.taskParam_.taskPara.Reduce.linkType);
        taskDetailsInfos.opType   = it.taskParam_.taskPara.Reduce.reduceOp;
    } else if (it.taskParam_.taskType == TaskParamType::TASK_NOTIFY_RECORD
               || it.taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT) {
        taskDetailsInfos.notifyID = it.taskParam_.taskPara.Notify.notifyID;
    }
    taskDetailsInfos.timeStamp         = ProfGetCurCpuTimestamp();
    taskDetailsInfos.durationEstimated = 0;
    taskDetailsInfos.taskId            = it.taskId_;
    taskDetailsInfos.streamId          = it.streamId_;
    taskDetailsInfos.planeID           = 0;
    taskDetailsInfos.transportType     = static_cast<int32_t>(SimpleTaskType::UB);
    if (taskDetailsInfos.remoteRank == INVALID_VALUE_RANKID) {
        taskDetailsInfos.transportType     = static_cast<int32_t>(SimpleTaskType::LOCAL);
    }
    taskDetailsInfos.role              = static_cast<uint32_t>(TaskRole::DST);
    taskDetailsInfos.workFlowMode      = static_cast<uint32_t>(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
}

void ProfilingHandlerLite::DumpTaskDetails(const MsprofAicpuHcclTaskInfo &taskDetailsInfos, const TaskInfo &taskInfo) const
{
    HCCL_INFO("[ProfilingHandlerLite] DumpTaskDetails %s", taskInfo.Describe().c_str());
}

void ProfilingHandlerLite::ReportMainStreamTask(const FlagTaskInfo &flagTaskInfo) const
{
    if (!GetProfL0State()) {
        HCCL_INFO("[ProfilingHandlerLite][ReportMainStreamTask] l0 is false.");
        return;
    }
    MsprofAicpuHcclMainStreamTask flagtask {};
    if (aicpu::GetTaskAndStreamId == nullptr) {
        HCCL_WARNING("[ProfilingHandlerLite][ReportMainStreamTask] aicpu::GetTaskAndStreamId is nullptr.");
        return;
    }
    uint64_t aicpuKernelTaskId   = 0U;
    uint32_t aicpuKernelStreamId = 0;
    if (aicpu::GetTaskAndStreamId(aicpuKernelTaskId, aicpuKernelStreamId) != aicpu::status_t::AICPU_ERROR_NONE) {
        THROW<InternalException>("[ProfilingHandler] Failed to get task id and stream id.");
    }
    // flagTaskInfo.taskId的高16位填到flagtask.taskId，低16位填到flagtask.streamId
    flagtask.taskId = static_cast<uint16_t>(flagTaskInfo.taskId >> 16);
    flagtask.streamId = static_cast<uint16_t>(flagTaskInfo.taskId);
    flagtask.type = flagTaskInfo.type;

    if (aicpuKernelTaskId > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        THROW<InvalidParamsException>("[ProfilingHandler] aicpuKernelTaskId is larger than u32.");
    }
    // aicpuKernelTaskId的高16位填到flagtask.aicpuTaskId，低16位填到flagtask.aicpuStreamId
    HCCL_INFO("[ProfilingHandlerLite][kernelTask] aicpuKernelTaskId %lu. aicpuKernelStreamId %u", aicpuKernelTaskId, aicpuKernelStreamId);
    uint32_t aicpuKernelTaskIdLow32 = static_cast<uint32_t>(aicpuKernelTaskId);
    flagtask.aicpuTaskId = static_cast<uint16_t>(aicpuKernelTaskIdLow32 >> 16);
    flagtask.aicpuStreamId = static_cast<uint16_t>(aicpuKernelTaskIdLow32);
    HCCL_INFO("[ProfilingHandlerLite][ReportMainStreamTask] streamId:%u, taskId:%u, type:%u,"
              "aicpuStreamId:%u, aicpuTaskId:%u",
              flagtask.streamId, flagtask.taskId, flagtask.type, flagtask.aicpuStreamId, flagtask.aicpuTaskId);
    MsprofAdditionalInfo reporterData{};
    reporterData.level     = MSPROF_REPORT_AICPU_LEVEL;
    reporterData.type      = MSPROF_REPORT_AICPU_HCCL_FLAG_TASK;
    reporterData.threadId  = cachedTid_;
    reporterData.dataLen   = sizeof(flagtask);
    reporterData.timeStamp = ProfGetCurCpuTimestamp();
    s32 sret               = memcpy_s(reporterData.data, sizeof(reporterData.data), &flagtask, sizeof(flagtask));
    if (sret != EOK) {
        THROW<InternalException>("Call memcpy_s failed, errorno[%d]", sret);
    }
    ReportAdditionInfo(reporterData);
}

void ProfilingHandlerLite::ReportAdditionInfo(const MsprofAdditionalInfo& reporterData) const
{
    if (reportAdditionalInfo_ == nullptr) {
        HCCL_WARNING("[ProfilingHandlerLite][ReportAdditionInfo] reportAdditionalInfo_ is nullptr.");
        return;
    }
    if (reportAdditionalInfo_(aging, &reporterData, sizeof(MsprofAdditionalInfo)) != 0) {
        THROW<InternalException>("[ProfilingHandler] ReportAdditionalInfo failed.");
    }
}

void ProfilingHandlerLite::UpdateProfSwitch()
{
    IsL1fromOffToOn();
    IsProfSwitchOn(ProfilingLevel::L0);
    IsProfSwitchOn(ProfilingLevel::L1);
}

bool ProfilingHandlerLite::IsProfOn(uint64_t feature) const
{
    if (MsprofReportBatchAdditionalInfo == nullptr) {
        if (AdprofCheckFeatureIsOn == nullptr) {
            return false;
        }
        return AdprofCheckFeatureIsOn(feature) > 0;
    } else {
        if (feature == ADPROF_TASK_TIME_L1) {
            return enableHcclL1_;
        } else if (feature == ADPROF_TASK_TIME_L0) {
            return enableHcclL0_;
        }
    }
 
    return false;
}

bool ProfilingHandlerLite::IsProfSwitchOn(ProfilingLevel level)
{
    bool res = false;
    if (level == ProfilingLevel::L0) {
        res           = IsProfOn(ADPROF_TASK_TIME_L0);
        enableHcclL0_ = res;
    } else if (level == ProfilingLevel::L1) {
        res           = IsProfOn(ADPROF_TASK_TIME_L1);
        enableHcclL1_ = res;
    }
    return res;
}

bool ProfilingHandlerLite::IsL1fromOffToOn()
{
    if (((!GetProfL1State()) && IsProfSwitchOn(ProfilingLevel::L1))) {
        HCCL_INFO("Profiling L1 switch form off to on.");
        return true;
    }
    return false;
}

void ProfilingHandlerLite::SetProL1On(bool val)
{
    HCCL_INFO("[%s] val = [%d]", __func__, val);
    enableHcclL1_ = val;
}
 
void ProfilingHandlerLite::SetProL0On(bool val)
{
    HCCL_INFO("[%s] val = [%d]", __func__, val);
    enableHcclL0_ = val;
}

uint64_t ProfilingHandlerLite::GetProfHashId(const char *name, uint32_t len) const
{
    if (name == nullptr || len == 0) {
        HCCL_WARNING("HashData is empty.  name:%s, len:%u", name, len);
        return INVALID_U64;
    }
    if (getProfHashId_ == nullptr) {
        return INVALID_U64;
    }
    return getProfHashId_(name, len);
}

} // namespace Hccl