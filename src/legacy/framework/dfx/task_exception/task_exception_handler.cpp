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
#include "acl/acl_rt.h"

namespace Hccl {

using namespace std;
using namespace CcuRep;

constexpr uint32_t AIV_FLAG_UB_ALIGN_SIZE=32; //aiv flag对齐规则
constexpr uint32_t TASK_CONTEXT_SIZE = 50;
constexpr uint32_t TASK_CONTEXT_INFO_SIZE = LOG_TMPBUF_SIZE - 50; // task 执行失败时打印前序task信息的长度限制
constexpr int BYTE = 8; // 一字节的位数

std::array<TaskExceptionHandler *, MAX_MODULE_DEVICE_NUM> TaskExceptionHandlerManager::handlers_;

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
        } else if(curTask->taskParam_.taskType == TaskParamType::TASK_AIV){
            ProcessAivException(exceptionInfo, *curTask);
        } else {
            ProcessException(exceptionInfo, *curTask);
        }
    );
}

/*
 @Desc: AIV 算子异常DFX
*/
void TaskExceptionHandler::ProcessAivException(rtExceptionInfo_t* exceptionInfo, const TaskInfo& taskInfo){
    HCCL_ERROR("[TaskExceptionHandler][%s]Task from HCCL run failed.", __func__);
    
    HCCL_ERROR("[TaskExceptionHandler][AIV]Task run failed, para information is "
                "deviceId[%u] streamId[%u], TaskId[%u], cmdType[%u], "
                "tag[%d],rank[%u],rankSize[%u], dataCount[%u], blockDim[%d],"
                "dataType:[%u], beginTime:[%llu], flagMem[%p]",
                exceptionInfo->deviceid, exceptionInfo->streamid, 
                exceptionInfo->taskid, taskInfo.taskParam_.taskPara.Aiv.cmdType, 
                taskInfo.taskParam_.taskPara.Aiv.tag, taskInfo.taskParam_.taskPara.Aiv.rank, 
                taskInfo.taskParam_.taskPara.Aiv.rankSize, taskInfo.taskParam_.taskPara.Aiv.count, 
                taskInfo.taskParam_.taskPara.Aiv.blockDim, taskInfo.taskParam_.taskPara.Aiv.dataType, 
                taskInfo.taskParam_.beginTime, taskInfo.taskParam_.taskPara.Aiv.flagMem);

    // 打印算子flag 区域, flag区域比较大，需要通过LOG_TMPBUF_SIZE控制打印的长度
    void *flag_buff_temp = nullptr;
    aclError aclRet = 0;
    aclRet = aclrtMallocHost(&flag_buff_temp, taskInfo.taskParam_.taskPara.Aiv.flagMemSize);
    if(aclRet != ACL_SUCCESS){
        HCCL_ERROR("[TaskExceptionHandler] [%s] error[%d].", __func__, aclRet);
        return;
    }
    aclRet = aclrtMemcpy(flag_buff_temp, taskInfo.taskParam_.taskPara.Aiv.flagMemSize, taskInfo.taskParam_.taskPara.Aiv.flagMem, taskInfo.taskParam_.taskPara.Aiv.flagMemSize, ACL_MEMCPY_DEVICE_TO_HOST);
    if(aclRet != ACL_SUCCESS){
        HCCL_ERROR("[TaskExceptionHandler] [%s] error[%d].", __func__, aclRet);
        return;
    }

    std::stringstream flagStr;
    int32_t          *flagMemInt32 = static_cast<int32_t*>(flag_buff_temp);
    u64               flagCount    = taskInfo.taskParam_.taskPara.Aiv.flagMemSize / sizeof(int32_t);
    //aiv 内部是32 byte对齐,即每32字节首位存放一个4字节的有效flag
    u64               alignstep = AIV_FLAG_UB_ALIGN_SIZE/sizeof(int32_t);
    flagStr << "[TaskExceptionHandler][AIV]Task run failed, para information is deviceId["
            << exceptionInfo->deviceid << "], streamId[" << exceptionInfo->streamid << "], TaskId["
            << exceptionInfo->taskid << "], flag:";
    for (u64 i = 0; (flag_buff_temp != nullptr) && (i < flagCount) && (flagStr.str().size() <= LOG_TMPBUF_SIZE); i++) {
        if (i % alignstep == 0) {
            flagStr << flagMemInt32[i] << " ";
        }
    }
    HCCL_ERROR(flagStr.str().c_str());
    
    if(flag_buff_temp != nullptr){
        aclRet = aclrtFreeHost(flag_buff_temp);
        if(aclRet != ACL_SUCCESS){
            HCCL_ERROR("[TaskExceptionHandler] [%s] error[%d].", __func__, aclRet);
            return;
        }
    }
    PrintAivPreviousTaskException(exceptionInfo);
}

void TaskExceptionHandler::PrintAivPreviousTaskException(rtExceptionInfo_t *exceptionInfo)
{
    // 倒序打印前序AIV task信息,找到当前异常task的前50个task(至多)
    auto queue = GlobalMirrorTasks::Instance().GetQueue(exceptionInfo->deviceid, exceptionInfo->streamid);
    if (queue == nullptr) {
        // 未找到异常对应的TaskQueue
        HCCL_ERROR("Exception task queue not found. deviceId[%u], streamId[%u].", exceptionInfo->deviceid, exceptionInfo->streamid);
        return;
    }

    u32  taskId = exceptionInfo->taskid;
    auto func   = [taskId](const shared_ptr<TaskInfo> &task) {
        return task->taskId_ == taskId;
    };
    auto taskItorPtr = queue->Find(func);
    if (taskItorPtr == nullptr || *taskItorPtr == *queue->End()) {
        // 在队列中未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId[%u], taskId[%u].", exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);
        return;
    }

    HCCL_ERROR("[TaskExceptionHandler][AIV]Task run failed, para information is "
               "deviceId[%u] streamId[%u], TaskId[%u], task info before failed task is:",
               exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);

    for (uint32_t i = 0; i < TASK_CONTEXT_SIZE && *taskItorPtr != *queue->Begin(); --(*taskItorPtr)) {
        if ((**taskItorPtr)->taskId_ > taskId) {
            break;
        }
        if ((**taskItorPtr)->taskId_ != taskId && (**taskItorPtr)->taskParam_.taskType == TaskParamType::TASK_AIV) {
                HCCL_ERROR("[TaskExceptionHandler][AIV] "
                "previous TaskId[%u],streamId[%u], cmdType[%u], "
                "tag[%d],rank[%u],rankSize[%u], dataCount[%u], blockDim[%d],"
                "dataType:[%u], beginTime:[%llu], flagMem[%p]",
                (**taskItorPtr)->taskId_, 
                (**taskItorPtr)->streamId_,
                (**taskItorPtr)->taskParam_.taskPara.Aiv.cmdType, 
                (**taskItorPtr)->taskParam_.taskPara.Aiv.tag, 
                (**taskItorPtr)->taskParam_.taskPara.Aiv.rank, 
                (**taskItorPtr)->taskParam_.taskPara.Aiv.rankSize, 
                (**taskItorPtr)->taskParam_.taskPara.Aiv.count, 
                (**taskItorPtr)->taskParam_.taskPara.Aiv.blockDim,
                (**taskItorPtr)->taskParam_.taskPara.Aiv.dataType, 
                (**taskItorPtr)->taskParam_.beginTime,
                (**taskItorPtr)->taskParam_.taskPara.Aiv.flagMem);
        }
        i++;
    }
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

    auto func = [taskId] (const shared_ptr<TaskInfo>& task) { return task->taskId_ == taskId; };
    auto taskItorPtr = queue->Find(func);
    if (taskItorPtr == nullptr || *taskItorPtr == *queue->End()) {
        // 在队列中未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId[%u], taskId[%u].", deviceId, streamId, taskId);
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

struct ccum_dfx_info {
    unsigned int query_result; // 0:success, 1:fail
    unsigned int ccum_sqe_recv_cnt;
    unsigned int ccum_sqe_send_cnt;
    unsigned int ccum_mission_dfx;
    unsigned int ccum_sqe_drop_cnt;
    unsigned int ccum_sqe_addr_len_err_drop_cnt;
    unsigned int lqc_ccu_sec_reg0;
    unsigned int ccum_tif_sqe_cnt;
    unsigned int ccum_tif_cqe_cnt;
    unsigned int ccum_cif_sqe_cnt;
    unsigned int ccum_cif_cqe_cnt;
};
    
void PrintPanicLogInfo(const uint8_t *panicLog)
{
    struct ccum_dfx_info *info = reinterpret_cast<struct ccum_dfx_info *>(const_cast<uint8_t*>(panicLog));
    const uint16_t ccumIsEnable = info->lqc_ccu_sec_reg0 & 1;
    if (info->query_result != 0) {
        HCCL_ERROR("get ccu dfx info fail, ccu dfx info not all correct");
    }
    HCCL_ERROR("CCU DFX INFO: SQE_RECV_CNT[%u] SQE_SEND_CNT[%u] MISSION_DFX[%u]"
                "TIF_SQE_CNT[%u] TIF_CQE_CNT[%u] CIF_SQE_CNT[%u] CIF_CQE_CNT[%u]"
                "SQE_DROP_CNT[%u] SQE_ADDR_LEN_ERR_DROP_CNT[%u] ccumIsEnable[%u]",
                info->ccum_sqe_recv_cnt, info->ccum_sqe_send_cnt, info->ccum_mission_dfx,
                info->ccum_tif_sqe_cnt, info->ccum_tif_cqe_cnt, info->ccum_cif_sqe_cnt, info->ccum_cif_cqe_cnt,
                info->ccum_sqe_drop_cnt, info->ccum_sqe_addr_len_err_drop_cnt, ccumIsEnable);
}
 	 
void TaskExceptionHandler::ProcessCcuMC2Exception(rtExceptionInfo_t* exceptionInfo)
{
    set<uint8_t> exDieIds{};
    auto& ccuExDetailInfo = exceptionInfo->expandInfo.u.fusionInfo.u.aicoreCcuInfo.ccuDetailMsg;
    for (uint32_t i = 0; i < ccuExDetailInfo.ccuMissionNum; ++i) {
        const auto& missionInfo = ccuExDetailInfo.missionInfo[i];   // 异常sqe
        HCCL_INFO("[%s] Exception missionInfo: dieId[%u], missionId[%u], instrId[%u], status[0x%x], subStatus[0x%x]",
            __func__, static_cast<u32>(missionInfo.dieId), static_cast<u32>(missionInfo.missionId), missionInfo.instrId, 
            missionInfo.status, missionInfo.subStatus);
        exDieIds.insert(missionInfo.dieId);
        uint16_t status = static_cast<uint16_t>(missionInfo.status) << BYTE | missionInfo.subStatus;
        // 打印寄存器信息
        PrintPanicLogInfo(missionInfo.panicLog);

        auto serverTaskInfo = MC2GlobalMirrorTasks::GetInstance().GetTaskInfo(
            exceptionInfo->deviceid, missionInfo.dieId, missionInfo.missionId, missionInfo.instrId);
        if (serverTaskInfo == nullptr) {
            HCCL_ERROR("MC2 TaskInfo not found, deviceId[%u], dieId[%u], missionId[%u], instrId[%u].",
                exceptionInfo->deviceid, static_cast<u32>(missionInfo.dieId),
 	            static_cast<u32>(missionInfo.missionId), missionInfo.instrId);
            continue;
        }
        ParaCcu serverParam = serverTaskInfo->taskParam_.taskPara.Ccu;
        serverParam.execMissionId = missionInfo.missionId;
        vector<CcuErrorInfo> serverErrorInfos {};
        if (GetCcuErrorMsg(exceptionInfo->deviceid, status, missionInfo.instrId, serverParam, serverErrorInfos) != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("Get CCU error info failed.");
            continue;
        }

        if (!serverErrorInfos.empty()) {
            HCCL_INFO("Exception instr is in MC2 Server.");
            PrintCcuErrorLog(serverErrorInfos, *serverTaskInfo);
            continue;
        }

        vector<CcuTaskParam> algoTaskParams = GetMC2AlgTaskParam(*serverTaskInfo);
        for (const auto& algoTaskParam : algoTaskParams) {
            HCCL_INFO("MC2 algo TaskParam: dieId[%u], missionId[%u], instrId[%u]",
                static_cast<u32>(algoTaskParam.dieId), static_cast<u32>(algoTaskParam.missionId), algoTaskParam.instStartId);

            auto algoTaskInfo = MC2GlobalMirrorTasks::GetInstance().GetTaskInfo(
                exceptionInfo->deviceid, algoTaskParam.dieId, algoTaskParam.missionId, algoTaskParam.instStartId);
            if (algoTaskInfo == nullptr) {
                HCCL_ERROR("MC2 TaskInfo not found, deviceId[%u], dieId[%u], missionId[%u], instrId[%u].",
                    exceptionInfo->deviceid, static_cast<u32>(algoTaskParam.dieId),
                    static_cast<u32>(algoTaskParam.missionId), algoTaskParam.instStartId);
                continue;
            }
            ParaCcu algoParam = algoTaskInfo->taskParam_.taskPara.Ccu;
            algoParam.execMissionId = missionInfo.missionId;
            vector<CcuErrorInfo> algoErrorInfos {};
            if (GetCcuErrorMsg(exceptionInfo->deviceid, status, missionInfo.instrId, algoParam, algoErrorInfos) != HcclResult::HCCL_SUCCESS) {
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
            "devLogicId[%d].", __func__, devLogicId);
    }

    for (const uint8_t dieId : exDieIds) {
        if (CcuCleanDieCkes(devLogicId, dieId) != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[TaskExceptionHandler][%s] failed to clean ccu die ckes, "
                "dieId[%u], devLogicId[%d].", __func__, dieId, devLogicId);
        }
    }
}

vector<CcuTaskParam> TaskExceptionHandler::GetMC2AlgTaskParam(const TaskInfo& taskInfo)
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
    auto* collServiceCcu = static_cast<CollServiceDeviceMode*>(collServiceBase);
    return collServiceCcu->GetMc2Compont().GetAlgoCcuTaskInfo(taskInfo.taskParam_.taskPara.Ccu.executeId);
}

 void TaskExceptionHandler::ProcessCcuException(rtExceptionInfo_t* exceptionInfo, const TaskInfo& taskInfo)
{
    auto deviceId = exceptionInfo->deviceid;
    HCCL_ERROR("[TaskExceptionHandler][%s]Task from HCCL run failed.", __func__);
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, base information is deviceID:[%u], %s.",
        deviceId, taskInfo.GetBaseInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, groupRank information is %s.",
        GetGroupRankInfo(taskInfo).c_str());
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, opData information is %s.", taskInfo.GetOpInfo().c_str());
    auto& ccuExDetailInfo = exceptionInfo->expandInfo.u.ccuInfo;
    for (uint32_t i = 0; i < ccuExDetailInfo.ccuMissionNum; ++i) { // ccuExDetailInfo.ccuMissionNum为1
        const auto& missionInfo = ccuExDetailInfo.missionInfo[i]; // 异常mission
        uint16_t status = static_cast<uint16_t>(missionInfo.status) << BYTE | missionInfo.subStatus;
        PrintCcuErrorInfo(deviceId, status, missionInfo.instrId, taskInfo);
        // 打印寄存器信息
        PrintPanicLogInfo(missionInfo.panicLog);
    }

    const int32_t devLogicId = static_cast<int32_t>(deviceId);
    if (CcuCleanTaskKillState(devLogicId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TaskExceptionHandler][%s] failed to clean ccu task kill state, "
            "devLogicId[%d].", __func__, devLogicId);
    }

    const uint8_t dieId = taskInfo.taskParam_.taskPara.Ccu.dieId;
    if (CcuCleanDieCkes(devLogicId, dieId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TaskExceptionHandler][%s] failed to clean ccu die ckes, "
            "dieId[%u], devLogicId[%d].", __func__, dieId, devLogicId);
    }
}

void TaskExceptionHandler::PrintCcuErrorInfo(uint32_t deviceId, uint16_t status, uint16_t instrId, const TaskInfo& taskInfo)
{
    const ParaCcu& ccuTaskParam = taskInfo.taskParam_.taskPara.Ccu;
    vector<CcuErrorInfo> errorInfos {};
    HcclResult ret = GetCcuErrorMsg(deviceId, status, instrId, ccuTaskParam, errorInfos);
    if (ret != HcclResult::HCCL_SUCCESS || errorInfos.empty()) {
        HCCL_ERROR("Get CCU error info failed. deviceId[%u], dieId[%u], missionId[%u], executeId[%llu].",
            deviceId, static_cast<uint32_t>(ccuTaskParam.dieId), static_cast<uint32_t>(ccuTaskParam.missionId),
            ccuTaskParam.executeId);
        return;
    }
    PrintCcuErrorLog(errorInfos, taskInfo);
}

void TaskExceptionHandler::PrintCcuErrorLog(const std::vector<CcuErrorInfo>& errorInfos, const TaskInfo& taskInfo)
{
    if (errorInfos.empty()) {
        return;
    }
    HCCL_ERROR("[TaskExceptionHandler]Task run failed, ccu runtime information is: %s", __func__);
    for (const auto& errorInfo : errorInfos) {
        HCCL_ERROR("[TaskExceptionHandler][%s]", GetCcuErrorMsgByType(errorInfo, taskInfo).c_str());
    }
}

string TaskExceptionHandler::GetCcuErrorMsgLoop(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Loop startInstr[%u], endInstrId[%u], "
                        "totalIteration[%u], currentIteration[%u], addrStride[0x%llx]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.loop.startInstrId,
                        ccuErrorInfo.msg.loop.endInstrId,
                        ccuErrorInfo.msg.loop.loopCnt,
                        ccuErrorInfo.msg.loop.loopCurrentCnt,
                        ccuErrorInfo.msg.loop.addrStride);
}

string TaskExceptionHandler::GetCcuErrorMsgLoopGroup(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: LoopGroup startLoopInsId[%u], loopInsCnt[%u], "
                        "expandOffset[%u], expandCnt[%u]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.loopGroup.startLoopInsId,
                        ccuErrorInfo.msg.loopGroup.loopInsCnt,
                        ccuErrorInfo.msg.loopGroup.expandOffset,
                        ccuErrorInfo.msg.loopGroup.expandCnt);
}

string TaskExceptionHandler::GetCcuErrorMsgLocPostSem(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Set sem[%u], semValue[0x%04x], mask[0x%04x]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.waitSignal.signalId,
        ccuErrorInfo.msg.waitSignal.signalValue,
        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgLocWaitSem(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Wait sem[%u], semValue[0x%04x], mask[0x%04x]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.waitSignal.signalId,
        ccuErrorInfo.msg.waitSignal.signalValue,
        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgRemPostSem(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    return StringFormat("CurrentInstr[%u]: Post, Use sem[%u], mask[0x%04x], rankId[%d]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.waitSignal.signalId,
        ccuErrorInfo.msg.waitSignal.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgRemWaitSem(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    return StringFormat("CurrentInstr[%u]: Wait, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankId[%d]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.waitSignal.signalId,
        ccuErrorInfo.msg.waitSignal.signalValue,
        ccuErrorInfo.msg.waitSignal.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgRemPostVar(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    return StringFormat("CurrentInstr[%u]: Post Variable[0x%016llx] To Param[%u], Use sem[%u], mask[0x%04x], rankId[%d]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.waitSignal.paramValue,
        ccuErrorInfo.msg.waitSignal.paramId,
        ccuErrorInfo.msg.waitSignal.signalId,
        ccuErrorInfo.msg.waitSignal.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgRemWaitGroup(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
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
    return StringFormat("CurrentInstr[%u]: Wait Group, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankIds[%s]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.waitSignal.signalId,
        ccuErrorInfo.msg.waitSignal.signalValue,
        ccuErrorInfo.msg.waitSignal.signalMask,
        ranks.str().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgPostSharedVar(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Post Shared Variable[%u] from Variable[0x%016llx], "
                        "Use sem[%u], mask[0x%04x]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.paramId,
                        ccuErrorInfo.msg.waitSignal.paramValue,
                        ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgPostSharedSem(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Post, Use sem[%u], mask[0x%04x]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.waitSignal.signalId,
        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgRead(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    return StringFormat("CurrentInstr[%u]: Read Memory[0x%016llx] To Memory[0x%016llx], "
                        "Set sem[%u] with mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.locAddr,
                        ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgWrite(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    return StringFormat("CurrentInstr[%u]: Write Memory[0x%016llx] to Memory[0x%016llx], "
                        "Set sem[%u] with mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.transMem.locAddr,
                        ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgLocalCpy(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], "
                        "Set sem[%u] with mask[0x%04x]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.transMem.locAddr,
                        ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgLocalReduce(const CcuErrorInfo& ccuErrorInfo,const  TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], "
                        "Set sem[%u] with mask[0x%04x], dataType[%u], opType[%u]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.transMem.locAddr,
                        ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask,
                        ccuErrorInfo.msg.transMem.dataType,
                        ccuErrorInfo.msg.transMem.opType);
}

string TaskExceptionHandler::GetCcuErrorMsgBufRead(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    return StringFormat("CurrentInstr[%u]: Read Rmt Mem[0x%016llx] To CcuBuffer[%u], "
                        "sem[%u], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.bufTransMem.addr,
                        ccuErrorInfo.msg.bufTransMem.bufId,
                        ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgBufWrite(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    return StringFormat("CurrentInstr[%u]: Write CcuBuffer[%u] To Rmt Mem[0x%016llx], "
                        "sem[%u], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.bufTransMem.bufId,
                        ccuErrorInfo.msg.bufTransMem.addr,
                        ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo));
}

string TaskExceptionHandler::GetCcuErrorMsgBufLocRead(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Read Loc Mem[0x%016llx] To CcuBuffer[%u], sem[%u], mask[0x%04x]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.bufTransMem.addr,
        ccuErrorInfo.msg.bufTransMem.bufId,
        ccuErrorInfo.msg.bufTransMem.signalId,
        ccuErrorInfo.msg.bufTransMem.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgBufLocWrite(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    (void)taskInfo;
    return StringFormat("CurrentInstr[%u]: Write CcuBuffer[%u] To Loc Mem[0x%016llx], sem[%u], mask[0x%04x]",
        ccuErrorInfo.instrId,
        ccuErrorInfo.msg.bufTransMem.bufId,
        ccuErrorInfo.msg.bufTransMem.addr,
        ccuErrorInfo.msg.bufTransMem.signalId,
        ccuErrorInfo.msg.bufTransMem.signalMask);
}

string TaskExceptionHandler::GetCcuErrorMsgBufReduce(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
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

    return StringFormat("CurrentInstr[%u]: Buffer Reduce count[%u], dataType[%u], outputDataType[%u], opType[%u], "
                        "sem[%u], mask[0x%04x], CcuBuffers[%s]",
                        ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.bufReduce.count,
                        ccuErrorInfo.msg.bufReduce.dataType,
                        ccuErrorInfo.msg.bufReduce.outputDataType,
                        ccuErrorInfo.msg.bufReduce.opType,
                        ccuErrorInfo.msg.bufReduce.signalId,
                        ccuErrorInfo.msg.bufReduce.signalMask,
                        buffIds.str().c_str());
}

string TaskExceptionHandler::GetCcuErrorMsgDefault(const CcuErrorInfo& ccuErrorInfo)
{
    return StringFormat("CurrentInstr[%u]: Internal Error", ccuErrorInfo.instrId);
}

string TaskExceptionHandler::GetCcuErrorMsgMission(const CcuErrorInfo& ccuErrorInfo)
{
    return StringFormat("CurrentInstr[%u]: dieId[%u], missionId[%u], missionError[%s]",
        ccuErrorInfo.instrId,
        static_cast<uint32_t>(ccuErrorInfo.dieId),
        static_cast<uint32_t>(ccuErrorInfo.missionId),
        ccuErrorInfo.msg.mission.missionError);
}

string TaskExceptionHandler::GetCcuErrorMsgByType(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo)
{
    if (ccuErrorInfo.type == CcuErrorType::MISSION) {
        return GetCcuErrorMsgMission(ccuErrorInfo);
    }

    using GetCcuErrorMsgFunc = string (*)(const CcuErrorInfo& ccuErrorInfo, const TaskInfo& taskInfo);
    static const map<CcuRepType, GetCcuErrorMsgFunc> handlerMap {
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
        {CcuRepType::BUF_REDUCE, &TaskExceptionHandler::GetCcuErrorMsgBufReduce}
    };

    const auto funcIt = handlerMap.find(ccuErrorInfo.repType);
    if (funcIt == handlerMap.end()) {
        return GetCcuErrorMsgDefault(ccuErrorInfo);
    } else {
        return funcIt->second(ccuErrorInfo, taskInfo);
    }
}

RankId TaskExceptionHandler::GetRankIdByChannelId(uint16_t channelId, const TaskInfo& taskInfo)
{
    if (taskInfo.taskParam_.taskType != TaskParamType::TASK_CCU) {
        HCCL_ERROR("[TaskInfo][%s]Get RankId failed, task type error.", __func__);
        return INVALID_RANKID;
    }
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]Get RankId failed, communicator is nullptr.", __func__);
        return INVALID_RANKID;
    }
    const CommunicatorImpl* communicator = (CommunicatorImpl*)taskInfo.dfxOpInfo_->comm_;
    auto* collServiceBase = communicator->GetCcuCollService();
    if (collServiceBase == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]Failed to get collService from communicator.", __func__);
        return INVALID_RANKID;
    }
    auto* collServiceCcu = static_cast<CollServiceDeviceMode*>(collServiceBase);
    const uint8_t dieId = taskInfo.taskParam_.taskPara.Ccu.dieId;
    return collServiceCcu->GetCcuInsPreprocessor()->GetCcuComm()->
        GetCcuJettyMgr()->GetRemoteRankIdByChannelId(dieId, static_cast<uint32_t>(channelId));
}

} // namespace Hccl