/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <memory>
#include "task_exception_handler.h"
#include "log.h"
#include "communicator_impl.h"
#include "coll_service_device_mode.h"
#include "mc2_global_mirror_tasks.h"
#include "ccu_device_manager.h"
#include "ccu_dev_mgr.h"
#include "orion_adapter_hccp.h"

namespace Hccl {

using namespace std;
using namespace CcuRep;

constexpr uint32_t TASK_CONTEXT_SIZE = 50;
constexpr uint32_t TASK_CONTEXT_INFO_SIZE = LOG_TMPBUF_SIZE - 50; // task 执行失败时打印前序task信息的长度限制

std::array<TaskExceptionHandler *, MAX_MODULE_DEVICE_NUM> TaskExceptionHandlerManager::handlers_;

std::mutex g_communicatorCallbackMapMutex;
array<map<s32, GetAicpuTaskExceptionCallBack>, MAX_MODULE_DEVICE_NUM> g_communicatorCallbackMap;
std::mutex g_commHadCallbackArrayMutex;
array<bool, MAX_MODULE_DEVICE_NUM> g_commHadCallbackArray = {false};
 
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
void RegisterGetAicpuTaskExceptionCallBack(s32 streamId, u32 deviceLogicId, GetAicpuTaskExceptionCallBack p1)
{
    lock_guard<mutex> lock(g_communicatorCallbackMapMutex);
    g_communicatorCallbackMap[deviceLogicId].emplace(streamId, p1);
    return;
}
#ifdef __cplusplus
}
#endif // __cplusplus

TaskExceptionHandler::TaskExceptionHandler(int deviceId) : devId_(deviceId)
{
    Register();
}

TaskExceptionHandler::~TaskExceptionHandler()
{
    UnRegister();
}

void TaskExceptionHandler::Register() const
{
    HrtRegTaskFailCallbackByModule(Process);
    HCCL_INFO("[TaskExceptionHandler]exception process func registered.");
}

void TaskExceptionHandler::UnRegister() const
{
    HrtRegTaskFailCallbackByModule(nullptr);
}

TaskExceptionHandler *TaskExceptionHandlerManager::GetHandler(size_t devId)
{
    // 检查 devId 是否越界
    if (devId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_ERROR("[TaskExceptionHandler][GetInstance] deviceLogicID[%lu] is invalid", devId);
        return nullptr;
    }
    // 如果对应位置的实例为空，则创建新实例
    if (handlers_[devId] == nullptr) {
        handlers_[devId] = new TaskExceptionHandler(devId);
    }
    return handlers_[devId];
}
TaskExceptionHandlerManager::TaskExceptionHandlerManager()
{
    handlers_.fill(nullptr);
}

TaskExceptionHandlerManager::~TaskExceptionHandlerManager()
{
    for (auto &instance : handlers_) {
        if (instance != nullptr) {
            delete instance;
            instance = nullptr;
        }
    }
}

static bool IsMC2Exception(rtExceptionInfo_t* exceptionInfo)
{
    return exceptionInfo != nullptr && exceptionInfo->expandInfo.type == RT_EXCEPTION_FUSION &&
        exceptionInfo->expandInfo.u.fusionInfo.type == RT_FUSION_AICORE_CCU;
}

void PrintUbRegisters(s32 devLogicId, RdmaHandle rdmaHandle)
{
    // TODO：补充locAddr和rmtAddr入参，打印addr，明确出问题的jetty对应的locEid和rmtEid
    AuxInfoIn in;
    in.cqe.status = 0xffffffff; // 0xffffffff代表查询所有寄存器
    AuxInfoOut auxInfo;
    auto ret = RaGetAuxInfo(rdmaHandle, in, auxInfo);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[PrintUbRegister]GetUbRegisterInfo failed.");
    }
 
    uint16_t flag= 0;
    for (u32 i = 0; i < auxInfo.auxInfoNum; i++) {
        if (auxInfo.auxInfoValues[i]) {
            flag++;
            HCCL_ERROR("devLogicId[%d], aux_info_type[%u]=%u, aux_info_value[%u]=%u",
                devLogicId, i, auxInfo.auxInfoTypes[i], i, auxInfo.auxInfoValues[i]);
        }
    }
    if (flag == 0) {
        HCCL_ERROR("devLogicId[%d], all aux_info values are zero.", devLogicId);
    }
}
 
void PrintCcuUbRegisters(s32 devLogicId, const ParaCcu &ccuTaskParam)
{
    std::vector<CcuJetty *> ccuJettys;
    HcclResult ret = GetCcuJettys(devLogicId, ccuTaskParam, ccuJettys);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("PrintCcuUbRegisters failed");
    }
    u32 jettyNum = ccuJettys.size();
 
    std::vector<JettyHandle> jettyHandles;
    for (auto &ccuJetty : ccuJettys) {
        jettyHandles.push_back(ccuJetty->GetJettyHandle());
    }
 
    std::vector<JettyStatus> jettyStatusVec;
    RaBatchQueryJettyStatus(jettyHandles, jettyStatusVec, jettyNum);
 
    for (auto i = 0; i < jettyNum; ++i) {
        if (jettyStatusVec[i] == JettyStatus::ERROR) {
            auto rdmaHandle = ccuJettys[i]->GetRdmaHandle();
            PrintUbRegisters(devLogicId, rdmaHandle);
        }
    }
}

void TaskExceptionHandler::Process(rtExceptionInfo_t* exceptionInfo)
{
    //Task Exception 入口，使用宏捕获执行间异常
    TRY_CATCH_PRINT_ERROR(
        if (exceptionInfo == nullptr) {
            HCCL_ERROR("Exception process failed, rtExceptionInfo is nullptr.");
            return;
        }

        if (IsMC2Exception(exceptionInfo)) {
            ProcessCcuMC2Exception(exceptionInfo);
            return;
        }

        const auto curTask = GlobalMirrorTasks::Instance().GetTaskInfo(
            exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);
        if (curTask == nullptr) {
            // 未找到异常对应的TaskInfo
            HCCL_ERROR("Exception task not found. deviceId[%u], streamId[%u], taskId[%u].",
                    exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);
            return;
        }

        if (curTask->taskParam_.taskType == TaskParamType::TASK_CCU) {
            ProcessCcuException(exceptionInfo, *curTask);
        } else {
            ProcessException(exceptionInfo, *curTask);
        }
    );
}

string TaskExceptionHandler::GetGroupRankInfo(const TaskInfo& taskInfo)
{
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]TaskInfo communicator is nullptr.", __func__);
        return "";
    }
    const CommunicatorImpl* communicator = static_cast<CommunicatorImpl*>(taskInfo.dfxOpInfo_->comm_);
    return StringFormat("group:[%s], rankSize[%u], rankId[%d]",
        communicator->GetId().c_str(), communicator->GetRankSize(), communicator->GetMyRank());
}

void TaskExceptionHandler::ProcessException(rtExceptionInfo_t* exceptionInfo, const TaskInfo& taskInfo)
{
    HCCL_RUN_INFO("[TaskExceptionHandler][%s]begin to execute hccl task exception callback function.", __func__);
    if (exceptionInfo == nullptr) {
        HCCL_ERROR("[TaskExceptionHandler][ProcessException] exceptionInfo is nullptr.");
        return;
    }
    PrintAicpuErrorMessage(exceptionInfo);

    HCCL_ERROR("[TaskExceptionHandler][%s]Task from HCCL run failed.", __func__);
    if (taskInfo.taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT) {
        PrintTaskContextInfo(exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);
    }
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, base information is deviceID:[%u], %s.",
        exceptionInfo->deviceid, taskInfo.GetBaseInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, para information is %s.", taskInfo.GetParaInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, groupRank information is %s.",
        GetGroupRankInfo(taskInfo).c_str());
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, opData information is %s.", taskInfo.GetOpInfo().c_str());
}

void TaskExceptionHandler::PrintTaskContextInfo(uint32_t deviceId, uint32_t streamId, uint32_t taskId)
{
    auto queue = GlobalMirrorTasks::Instance().GetQueue(deviceId, streamId);
    if (queue == nullptr) {
        // 未找到异常对应的TaskQueue
        HCCL_ERROR("Exception task queue not found. deviceId[%u], streamId[%u].", deviceId, streamId);
        return;
    }

    auto func = [taskId](const shared_ptr<TaskInfo> &task) {
        return task->taskId_ == taskId;
    };
    auto taskItorPtr = queue->Find(func);
    if (taskItorPtr == nullptr || *taskItorPtr == *queue->End()) {
        // 在队列中未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId[%u], taskId[%u].", deviceId, streamId, taskId);
        return;
    }

    // 找到当前异常task的前50个task(至多)
    vector<shared_ptr<TaskInfo>> taskContext{};
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

    HCCL_ERROR("[TaskExceptionHandler]Task run failed, context sequence before error task is "
               "[SDMA:M(rank), RDMA:RS(rank,id), SendPayload:SP(rank), InlineReduce:IR(rank), Reduce:R(rank), "
               "NotifyRecord:NR(rank,id), NotifyWait:NW(rank,id), SendNotify:SN(rank,id), "
               "WriteWithNotify:WN(rank,id), WriteReduceWithNotify:WRN(rank,id)]:");

    string taskContextInfo = "";
    for (auto it = taskContext.rbegin(); it != taskContext.rend(); ++it) {
        string conciseInfo = (*it)->GetConciseBaseInfo();
        conciseInfo += ",";

        if (taskContextInfo.size() + conciseInfo.size() >= TASK_CONTEXT_INFO_SIZE) {
            HCCL_ERROR("[TaskExceptionHandler]%s", taskContextInfo.c_str());
            taskContextInfo = "";
        }

        taskContextInfo += conciseInfo;
    }
    HCCL_ERROR("[TaskExceptionHandler]%s end.", taskContextInfo.c_str());
}

void TaskExceptionHandler::ProcessCcuMC2Exception(rtExceptionInfo_t *exceptionInfo)
{
    set<uint8_t> exDieIds{};
    auto        &ccuExDetailInfo = exceptionInfo->expandInfo.u.fusionInfo.u.aicoreCcuInfo.ccuDetailMsg;
    for (uint32_t i = 0; i < ccuExDetailInfo.ccuTaskNum; ++i) {
        const auto &sqeInfo = ccuExDetailInfo.sqeInfo[i]; // 异常sqe
        HCCL_INFO("Exception sqeInfo: dieId[%u], missionId[%u], instrId[%u]", static_cast<u32>(sqeInfo.dieId),
                  static_cast<u32>(sqeInfo.missionId), sqeInfo.instrId);
        exDieIds.insert(sqeInfo.dieId);

        auto serverTaskInfo = MC2GlobalMirrorTasks::GetInstance().GetTaskInfo(exceptionInfo->deviceid, sqeInfo.dieId,
                                                                              sqeInfo.missionId, sqeInfo.instrId);
        if (serverTaskInfo == nullptr) {
            HCCL_ERROR("MC2 TaskInfo not found, deviceId[%u], dieId[%u], missionId[%u], instrId[%u].",
                       exceptionInfo->deviceid, static_cast<u32>(sqeInfo.dieId), static_cast<u32>(sqeInfo.missionId),
                       sqeInfo.instrId);
            continue;
        }
        ParaCcu serverParam       = serverTaskInfo->taskParam_.taskPara.Ccu;
        serverParam.execMissionId = sqeInfo.missionId;
        vector<CcuErrorInfo> serverErrorInfos {};
        uint16_t ccuMissionStatus = 0;
        if (GetCcuErrorMsg(exceptionInfo->deviceid, serverParam, serverErrorInfos, ccuMissionStatus) != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("Get CCU error info failed.");
            continue;
        }

        if (!serverErrorInfos.empty()) {
            HCCL_INFO("Exception instr is in MC2 Server.");
            PrintCcuErrorLog(serverErrorInfos, *serverTaskInfo);
            continue;
        }

        vector<CcuTaskParam> algoTaskParams = GetMC2AlgTaskParam(*serverTaskInfo);
        for (const auto &algoTaskParam : algoTaskParams) {
            HCCL_INFO("MC2 algo TaskParam: dieId[%u], missionId[%u], instrId[%u]",
                      static_cast<u32>(algoTaskParam.dieId), static_cast<u32>(algoTaskParam.missionId),
                      algoTaskParam.instStartId);

            auto algoTaskInfo = MC2GlobalMirrorTasks::GetInstance().GetTaskInfo(
                exceptionInfo->deviceid, algoTaskParam.dieId, algoTaskParam.missionId, algoTaskParam.instStartId);
            if (algoTaskInfo == nullptr) {
                HCCL_ERROR("MC2 TaskInfo not found, deviceId[%u], dieId[%u], missionId[%u], instrId[%u].",
                           exceptionInfo->deviceid, static_cast<u32>(algoTaskParam.dieId),
                           static_cast<u32>(algoTaskParam.missionId), algoTaskParam.instStartId);
                continue;
            }
            ParaCcu algoParam       = algoTaskInfo->taskParam_.taskPara.Ccu;
            algoParam.execMissionId = sqeInfo.missionId;
            vector<CcuErrorInfo> algoErrorInfos {};
            ccuMissionStatus = 0;
            if (GetCcuErrorMsg(exceptionInfo->deviceid, algoParam, algoErrorInfos, ccuMissionStatus) != HcclResult::HCCL_SUCCESS) {
                HCCL_ERROR("Get CCU error info failed.");
                continue;
            }
            PrintCcuErrorLog(algoErrorInfos, *algoTaskInfo);
        }
    }

    // 清除TaskKill状态, 清除CKE
    const int32_t devLogicId = static_cast<int32_t>(exceptionInfo->deviceid);
    if (CcuCleanTaskKillState(devLogicId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TaskExceptionHandler][%s] failed to clean ccu task kill state, "
                   "devLogicId[%d].",
                   __func__, devLogicId);
    }

    for (const uint8_t dieId : exDieIds) {
        if (CcuCleanDieCkes(devLogicId, dieId) != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[TaskExceptionHandler][%s] failed to clean ccu die ckes, "
                       "dieId[%u], devLogicId[%d].",
                       __func__, dieId, devLogicId);
        }
    }
}

vector<CcuTaskParam> TaskExceptionHandler::GetMC2AlgTaskParam(const TaskInfo &taskInfo)
{
    if (taskInfo.taskParam_.taskType != TaskParamType::TASK_CCU) {
        HCCL_ERROR("[TaskInfo][%s]Get MC2 Alg TaskParam failed, task type error.", __func__);
        return {};
    }
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]Get MC2 Alg TaskParam failed, communicator is nullptr.", __func__);
        return {};
    }
    const CommunicatorImpl* communicator = (CommunicatorImpl*)taskInfo.dfxOpInfo_->comm_;
    auto* collServiceBase = communicator->GetCcuCollService();
    if (collServiceBase == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]Failed to get collService from communicator.", __func__);
        return {};
    }
    auto *collServiceCcu = static_cast<CollServiceDeviceMode *>(collServiceBase);
    return collServiceCcu->GetMc2Compont().GetAlgoCcuTaskInfo(taskInfo.taskParam_.taskPara.Ccu.executeId);
}

void TaskExceptionHandler::ProcessCcuException(rtExceptionInfo_t* exceptionInfo, const TaskInfo& taskInfo)
{
    HCCL_ERROR("[TaskExceptionHandler][%s]Task from HCCL run failed.", __func__);
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, base information is deviceID:[%u], %s.",
        exceptionInfo->deviceid, taskInfo.GetBaseInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, groupRank information is %s.",
               GetGroupRankInfo(taskInfo).c_str());
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, opData information is %s.", taskInfo.GetOpInfo().c_str());
    PrintCcuErrorInfo(exceptionInfo, taskInfo);

    const int32_t devLogicId = static_cast<int32_t>(exceptionInfo->deviceid);
    if (CcuCleanTaskKillState(devLogicId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TaskExceptionHandler][%s] failed to clean ccu task kill state, "
                   "devLogicId[%d].",
                   __func__, devLogicId);
    }

    const uint8_t dieId = taskInfo.taskParam_.taskPara.Ccu.dieId;
    if (CcuCleanDieCkes(devLogicId, dieId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TaskExceptionHandler][%s] failed to clean ccu die ckes, "
                   "dieId[%u], devLogicId[%d].",
                   __func__, dieId, devLogicId);
    }
}

inline void PrintBaseErrorLog(const std::string &stageErrInfo, const std::string &baseInfo)
{
    HCCL_ERROR("%sTask run failed, base information is %s", stageErrInfo.c_str(), baseInfo.c_str());
}
 
inline void PrintParaErrorLog(const std::string &stageErrInfo, const std::string &paraInfoStr, const std::string &tag)
{
    HCCL_ERROR("%sTask run failed, para information is %s, tag[%s].", stageErrInfo.c_str(), paraInfoStr.c_str(), tag.c_str());
}
 
inline void PrintOpDataErrorLog(const std::string &stageErrInfo, const std::string &opDataContent)
{
    HCCL_ERROR("%sTask run failed, opData information is %s", stageErrInfo.c_str(), opDataContent.c_str());
}
 
inline void PrintGroupErrorLog(const std::string &stageErrInfo, const std::string &groupRankContent, const std::string &tag)
{
    HCCL_ERROR("%sTask run failed, groupRank information is %s, tag[%s].", stageErrInfo.c_str(), groupRankContent.c_str(), tag.c_str());
}
 
void TaskExceptionHandler::PrintAicpuErrorMessage(rtExceptionInfo_t *exceptionInfo)
{
    ErrorMessageReport errorMessage;
    unique_lock<std::mutex> lock(g_commHadCallbackArrayMutex);
    if (g_commHadCallbackArray[exceptionInfo->deviceid]) {
        // 防止同一个device上出现通信主流和kernel流均出现task exception时runtime调用两次callback
        // HDC通道信息不是读清，防止aicpu task exception重复上报
        HCCL_WARNING("aicpu error message been reported. deviceid[%u]", exceptionInfo->deviceid);
        return;
    }
    lock.unlock();
    if (g_communicatorCallbackMap[exceptionInfo->deviceid].find(exceptionInfo->streamid) !=\
        g_communicatorCallbackMap[exceptionInfo->deviceid].end()) {
        // 找到对应的通信域，并调用回调函数从HDC通道获取AICPU异常信息
        errorMessage = (g_communicatorCallbackMap[exceptionInfo->deviceid])[exceptionInfo->streamid]();
        if (strlen(errorMessage.tag) > 0) {
            string groupRankContent;
            u32 streamId = static_cast<u32>(errorMessage.streamId);
            std::string tag = std::string(errorMessage.tag);
            u32 index = 0;
            TaskParam taskParam{};
            taskParam.taskType = errorMessage.taskType;
            std::shared_ptr<DfxOpInfo> dfxOpInfo;
            dfxOpInfo->tag_ = tag;
            TaskInfo exceptionTaskInfo(streamId, errorMessage.taskId, errorMessage.remoteUserRank, taskParam, dfxOpInfo);
            auto logKeywordL2 = exceptionTaskInfo.taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT ? LOG_KEYWORDS_TIMEOUT : LOG_KEYWORDS_RUN_FAILED;
            auto stageErrInfo = "[" + LOG_KEYWORDS_TASK_EXEC + "][" + logKeywordL2 + "][" + LOG_KEYWORDS_AICPU + "]";
            HCCL_ERROR("%sTask from HCCL run failed.", stageErrInfo.c_str());
            // 防止tag字符串过长， 信息分开打印
            PrintBaseErrorLog(stageErrInfo, exceptionTaskInfo.GetBaseInfo());
            PrintParaErrorLog(stageErrInfo, exceptionTaskInfo.GetParaInfo(), tag);
            // PrintGroupErrorMessage(errorMessage, exceptionTaskInfo, groupRankContent, stageErrInfo);
            // PrintOpDataErrorMessage(exceptionInfo->deviceid, errorMessage, stageErrInfo);
 
            // 打印UB DFX寄存器信息
            if (errorMessage.taskType == TaskParamType::TASK_SEND_NOTIFY || errorMessage.taskType == TaskParamType::TASK_SEND_PAYLOAD
                || errorMessage.taskType == TaskParamType::TASK_WRITE_WITH_NOTIFY || errorMessage.taskType == TaskParamType::TASK_WRITE_REDUCE_WITH_NOTIFY) {
                auto addr = IpAddress(errorMessage.locEid);
                u32 devPhyId = HrtGetDevicePhyIdByIndex(exceptionInfo->deviceid);
                auto rdmaHandle = RdmaHandleManager::GetInstance().GetByIp(devPhyId, addr);
                PrintUbRegisters(static_cast<s32>(exceptionInfo->deviceid), rdmaHandle);
            }
 
            if (exceptionTaskInfo.taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT) {
                RPT_INPUT_ERR(true,
                    "EI0002",
                    std::vector<std::string>({"remote_rankid", "base_information", "task_information", "group_rank_content"}),
                    std::vector<std::string>({
                        std::to_string(exceptionTaskInfo.remoteRank_),
                        exceptionTaskInfo.GetBaseInfo().c_str(), (exceptionTaskInfo.GetParaInfo()).c_str(),
                        ""})
                );
            } else if (exceptionTaskInfo.taskParam_.taskType == TaskParamType::TASK_SDMA || exceptionTaskInfo.taskParam_.taskType == TaskParamType::TASK_REDUCE_INLINE) {
                RPT_INPUT_ERR(true,
                    "EI0012",
                    std::vector<std::string>({"remote_rankid", "base_information", "task_information", "group_rank_content"}),
                    std::vector<std::string>({
                        std::to_string(exceptionTaskInfo.remoteRank_), exceptionTaskInfo.GetBaseInfo().c_str(),
                        (exceptionTaskInfo.GetParaInfo()).c_str(), groupRankContent.c_str()})
                    );
            }
 
            lock.lock();
            g_commHadCallbackArray[exceptionInfo->deviceid] = true;
        }
    } else {
        HCCL_INFO("PrintAicpuErrorMessage streamId[%u] is not found.", exceptionInfo->streamid);
    }
    return;
}

void TaskExceptionHandler::PrintCcuErrorInfo(rtExceptionInfo_t* exceptionInfo, const TaskInfo& taskInfo)
{
    const ParaCcu& ccuTaskParam = taskInfo.taskParam_.taskPara.Ccu;
    vector<CcuErrorInfo> errorInfos {};
    uint16_t ccuMissionStatus = 0;
    HcclResult ret = GetCcuErrorMsg(exceptionInfo->deviceid, ccuTaskParam, errorInfos, ccuMissionStatus);
    if (ret != HcclResult::HCCL_SUCCESS || errorInfos.empty()) {
        HCCL_ERROR("Get CCU error info failed. deviceId[%u], dieId[%u], missionId[%u], executeId[%llu].",
            exceptionInfo->deviceid, static_cast<uint32_t>(ccuTaskParam.dieId), static_cast<uint32_t>(ccuTaskParam.missionId),
            ccuTaskParam.executeId);
        return;
    }
    PrintCcuErrorLog(errorInfos, taskInfo);
    // 如果是UB错误(missionStatus为[0x1, 0x5])，打印Ub Dfx寄存器信息
    if (ccuMissionStatus >= 0x01 && ccuMissionStatus <= 0x05) {
        PrintCcuUbRegisters(static_cast<s32>(exceptionInfo->deviceid), taskInfo.taskParam_.taskPara.Ccu);
    }
}

void TaskExceptionHandler::PrintCcuErrorLog(const std::vector<CcuErrorInfo> &errorInfos, const TaskInfo &taskInfo)
{
    if (errorInfos.empty()) {
        return;
    }
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, ccu runtime information is: %s", __func__);
    for (const auto &errorInfo : errorInfos) {
        HCCL_ERROR("[TaskExceptionHandler][%s]", GetCcuErrorMsgByType(errorInfo, taskInfo).c_str());
    }
}

string TaskExceptionHandler::GetCcuErrorMsgLoop(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Loop startInstrId[%u], endInstrId[%u], executorId[%u], "
                        "totalIter[%u], curIter[%u], addressStride[0x%llx]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.loop.startInstrId, ccuErrorInfo.msg.loop.endInstrId,
                        ccuErrorInfo.msg.loop.loopEngineId, ccuErrorInfo.msg.loop.loopCnt,
                        ccuErrorInfo.msg.loop.loopCurrentCnt, ccuErrorInfo.msg.loop.addrStride);
}

string TaskExceptionHandler::GetCcuErrorMsgLoopGroup(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: LoopGroup startLoopInsId[%u], loopInsCnt[%u], "
                        "expandOffset[%u], expandCnt[%u]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.loopGroup.startLoopInsId,
                        ccuErrorInfo.msg.loopGroup.loopInsCnt, ccuErrorInfo.msg.loopGroup.expandOffset,
                        ccuErrorInfo.msg.loopGroup.expandCnt);
}

string TaskExceptionHandler::GetCcuErrorMsgLocPostSem(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Set sem[%u], semValue[0x%04x], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgLocWaitSem(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Wait sem[%u], semValue[0x%04x], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgRemPostSem(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    return StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x], rankId[%d]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgRemWaitSem(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    return StringFormat("InstrId[%u]: Wait, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalValue, ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgRemPostVar(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    return StringFormat("InstrId[%u]: Post Variable[0x%016llx] To Param[%u], Use sem[%u], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.paramValue,
                        ccuErrorInfo.msg.waitSignal.paramId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgRemWaitGroup(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    stringstream ranks;
    for (uint32_t i = 0; i < WAIT_SIGNAL_CHANNEL_SIZE; ++i) {
        const auto channelId = ccuErrorInfo.msg.waitSignal.channelId[i];
        if (channelId == UINT16_MAX) {
            break;
        }
        const auto rankId = GetRankIdByChannelId(channelId, taskInfo);
        if (i != 0) {
            ranks << ", ";
        }
        ranks << to_string(rankId);
    }
    return StringFormat("InstrId[%u]: Wait Group, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankIds[%s]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalValue, ccuErrorInfo.msg.waitSignal.signalMask,
                        ranks.str().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgPostSharedVar(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Post Shared Variable[%u] from Variable[0x%016llx], "
                        "Use sem[%u], mask[0x%04x]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.paramId,
                        ccuErrorInfo.msg.waitSignal.paramValue, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgPostSharedSem(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgRead(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    return StringFormat(
        "InstrId[%u]: Read Memory[0x%016llx] To Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.rmtAddr, ccuErrorInfo.msg.transMem.locAddr,
        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId, ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo),
        GetAddrPairByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo).first.Describe().c_str(),
        GetAddrPairByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo).second.Describe().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgWrite(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    return StringFormat(
        "InstrId[%u]: Write Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId, ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo),
        GetAddrPairByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo).first.Describe().c_str(),
        GetAddrPairByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo).second.Describe().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgLocalCpy(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgLocalReduce(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x], dataType[%u], opType[%u]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask, ccuErrorInfo.msg.transMem.dataType,
                        ccuErrorInfo.msg.transMem.opType);
}

string TaskExceptionHandler::GetCcuErrorMsgBufRead(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    return StringFormat(
        "InstrId[%u]: Read Rmt Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.addr, ccuErrorInfo.msg.bufTransMem.bufId,
        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
        ccuErrorInfo.msg.bufTransMem.signalMask, GetRankIdByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo),
        GetAddrPairByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo).first.Describe().c_str(),
        GetAddrPairByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo).second.Describe().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgBufWrite(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    return StringFormat(
        "InstrId[%u]: Write CcuBuffer[%u] To Rmt Mem[0x%016llx], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.bufId, ccuErrorInfo.msg.bufTransMem.addr,
        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
        ccuErrorInfo.msg.bufTransMem.signalMask, GetRankIdByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo),
        GetAddrPairByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo).first.Describe().c_str(),
        GetAddrPairByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo).second.Describe().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgBufLocRead(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Read Loc Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], sem[%u], mask[0x%04x]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.addr, ccuErrorInfo.msg.bufTransMem.bufId,
                        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgBufLocWrite(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    return StringFormat("InstrId[%u]: Write CcuBuffer[%u] To Loc Mem[0x%016llx], Len[%llu], sem[%u], mask[0x%04x]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.bufId, ccuErrorInfo.msg.bufTransMem.addr,
                        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgBufReduce(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    (void)taskInfo;
    stringstream buffIds;
    for (uint32_t i = 0; i < BUF_REDUCE_ID_SIZE; ++i) {
        const auto buffId = ccuErrorInfo.msg.bufReduce.bufIds[i];
        if (buffId == UINT16_MAX) {
            break;
        }
        if (i != 0) {
            buffIds << ", ";
        }
        buffIds << to_string(buffId);
    }

    return StringFormat("InstrId[%u]: Buffer Reduce count[%u], dataType[%u], outputDataType[%u], opType[%u], "
                        "sem[%u], mask[0x%04x], CcuBuffers[%s]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufReduce.count, ccuErrorInfo.msg.bufReduce.dataType,
                        ccuErrorInfo.msg.bufReduce.outputDataType, ccuErrorInfo.msg.bufReduce.opType,
                        ccuErrorInfo.msg.bufReduce.signalId, ccuErrorInfo.msg.bufReduce.signalMask,
                        buffIds.str().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgDefault(const CcuErrorInfo &ccuErrorInfo)
{
    return StringFormat("InstrId[%u]: Internal Error", ccuErrorInfo.instrId);
}

string TaskExceptionHandler::GetCcuErrorMsgMission(const CcuErrorInfo &ccuErrorInfo)
{
    return StringFormat("InstrId[%u]: dieId[%u], missionId[%u], missionError[%s]", ccuErrorInfo.instrId,
                        static_cast<uint32_t>(ccuErrorInfo.dieId), static_cast<uint32_t>(ccuErrorInfo.missionId),
                        ccuErrorInfo.msg.mission.missionError);
}

string TaskExceptionHandler::GetCcuErrorMsgByType(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo)
{
    if (ccuErrorInfo.type == CcuErrorType::MISSION) {
        return GetCcuErrorMsgMission(ccuErrorInfo);
    }

    using GetCcuErrorMsgFunc = string (*)(const CcuErrorInfo &ccuErrorInfo, const TaskInfo &taskInfo);
    static const map<CcuRepType, GetCcuErrorMsgFunc> handlerMap{
        {CcuRepType::LOOP, &TaskExceptionHandler::GetCcuErrorMsgLoop},
        {CcuRepType::LOOPGROUP, &TaskExceptionHandler::GetCcuErrorMsgLoopGroup},
        {CcuRepType::LOC_POST_SEM, &TaskExceptionHandler::GetCcuErrorMsgLocPostSem},
        {CcuRepType::LOC_WAIT_SEM, &TaskExceptionHandler::GetCcuErrorMsgLocWaitSem},
        {CcuRepType::REM_POST_SEM, &TaskExceptionHandler::GetCcuErrorMsgRemPostSem},
        {CcuRepType::REM_WAIT_SEM, &TaskExceptionHandler::GetCcuErrorMsgRemWaitSem},
        {CcuRepType::REM_POST_VAR, &TaskExceptionHandler::GetCcuErrorMsgRemPostVar},
        {CcuRepType::REM_WAIT_GROUP, &TaskExceptionHandler::GetCcuErrorMsgRemWaitGroup},
        {CcuRepType::POST_SHARED_VAR, &TaskExceptionHandler::GetCcuErrorMsgPostSharedVar},
        {CcuRepType::POST_SHARED_SEM, &TaskExceptionHandler::GetCcuErrorMsgPostSharedSem},
        {CcuRepType::READ, &TaskExceptionHandler::GetCcuErrorMsgRead},
        {CcuRepType::WRITE, &TaskExceptionHandler::GetCcuErrorMsgWrite},
        {CcuRepType::LOCAL_CPY, &TaskExceptionHandler::GetCcuErrorMsgLocalCpy},
        {CcuRepType::LOCAL_REDUCE, &TaskExceptionHandler::GetCcuErrorMsgLocalReduce},
        {CcuRepType::BUF_READ, &TaskExceptionHandler::GetCcuErrorMsgBufRead},
        {CcuRepType::BUF_WRITE, &TaskExceptionHandler::GetCcuErrorMsgBufWrite},
        {CcuRepType::BUF_LOC_READ, &TaskExceptionHandler::GetCcuErrorMsgBufLocRead},
        {CcuRepType::BUF_LOC_WRITE, &TaskExceptionHandler::GetCcuErrorMsgBufLocWrite},
        {CcuRepType::BUF_REDUCE, &TaskExceptionHandler::GetCcuErrorMsgBufReduce}};

    const auto funcIt = handlerMap.find(ccuErrorInfo.repType);
    if (funcIt == handlerMap.end()) {
        return GetCcuErrorMsgDefault(ccuErrorInfo);
    } else {
        return funcIt->second(ccuErrorInfo, taskInfo);
    }
}

RankId TaskExceptionHandler::GetRankIdByChannelId(uint16_t channelId, const TaskInfo &taskInfo)
{
    if (taskInfo.taskParam_.taskType != TaskParamType::TASK_CCU) {
        HCCL_ERROR("[TaskException][%s]Get RankId failed, task type error.", __func__);
        return INVALID_RANKID;
    }
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskException][%s]Get RankId failed, communicator is nullptr.", __func__);
        return INVALID_RANKID;
    }
    const CommunicatorImpl* communicator = (CommunicatorImpl*)taskInfo.dfxOpInfo_->comm_;
    auto* collServiceBase = communicator->GetCcuCollService();
    if (collServiceBase == nullptr) {
        HCCL_ERROR("[TaskException][%s]Failed to get collService from communicator.", __func__);
        return INVALID_RANKID;
    }
    auto         *collServiceCcu = static_cast<CollServiceDeviceMode *>(collServiceBase);
    const uint8_t dieId          = taskInfo.taskParam_.taskPara.Ccu.dieId;
    return collServiceCcu->GetCcuInsPreprocessor()->GetCcuComm()->GetCcuJettyMgr()->GetRemoteRankIdByChannelId(
        dieId, static_cast<uint32_t>(channelId));
}

std::pair<IpAddress, IpAddress> TaskExceptionHandler::GetAddrPairByChannelId(uint16_t        channelId,
                                                                             const TaskInfo &taskInfo)
{
    std::pair<IpAddress, IpAddress> dummy = {IpAddress(), IpAddress()};
    if (taskInfo.taskParam_.taskType != TaskParamType::TASK_CCU) {
        HCCL_ERROR("[TaskException][%s]Get AddrPair failed, task type error[%s]", __func__,
                   taskInfo.taskParam_.Describe().c_str());
        return dummy;
    }
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskException][%s]Get AddrPair failed, communicator is nullptr.", __func__);
        return dummy;
    }
    const CommunicatorImpl *communicator    = (CommunicatorImpl *)taskInfo.dfxOpInfo_->comm_;
    auto                   *collServiceBase = communicator->GetCollService();
    if (collServiceBase == nullptr) {
        HCCL_ERROR("[TaskException][%s]Failed to get collService from communicator.", __func__);
        return dummy;
    }
    auto         *collServiceCcu = static_cast<CollServiceDeviceMode *>(collServiceBase);
    const uint8_t dieId          = taskInfo.taskParam_.taskPara.Ccu.dieId;
    return collServiceCcu->GetCcuInsPreprocessor()->GetCcuComm()->GetCcuJettyMgr()->GetAddrPairByChannelId(
        dieId, static_cast<uint32_t>(channelId));
}

} // namespace Hccl