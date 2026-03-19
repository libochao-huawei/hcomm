/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hcclCommTaskException.h"
#include <memory>
#include "log.h"
#include "coll_comm.h"
#include "acl/acl_rt.h"
#include "orion_adapter_hccp.h"
#include <adapter_error_manager_pub.h>
#include "op_type.h"
#include "task_exception_handler.h"
#include "task_param.h"
#include "ccu_rep_type.h"

namespace hcomm {

using namespace std;

constexpr u32 MAX_MODULE_DEVICE_NUM_V2 = 65;
constexpr uint32_t TASK_CONTEXT_SIZE = 50;
constexpr uint32_t TASK_CONTEXT_INFO_SIZE = LOG_TMPBUF_SIZE - 50; // task 执行失败时打印前序task信息的长度限制
constexpr int BYTE = 8;
constexpr uint64_t CCU_MSG_256MB_LEN = 256 * 1024 * 1024; // CCU消息长度不能大于256MB

std::mutex g_communicatorCallbackMapMutexV2;
array<map<s32, GetAicpuTaskExceptionCallBackHcomm>, MAX_MODULE_DEVICE_NUM_V2> g_communicatorCallbackMapV2;
std::mutex g_commHadCallbackArrayMutexV2;
array<bool, MAX_MODULE_DEVICE_NUM_V2> g_commHadCallbackArrayV2 = {false};


TaskExceptionHost::~TaskExceptionHost()
{
    (void)UnRegister();
}

HcclResult TaskExceptionHost::Register()
{
    CHK_PRT_RET(isRegistered_, HCCL_DEBUG("[%s]has been registered, skip", __func__), HCCL_SUCCESS);
    aclError ret = aclrtSetExceptionInfoCallback(Process);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[%s]aclrtSetExceptionInfoCallback failed, ret = [%u]", __func__, ret), HCCL_E_RUNTIME);
    isRegistered_ = true;
    HCCL_INFO("[TaskExceptionHost] registered success.");
    return HCCL_SUCCESS;
}

HcclResult TaskExceptionHost::UnRegister()
{
    aclError ret = aclrtSetExceptionInfoCallback(nullptr);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[%s]aclrtSetExceptionInfoCallback failed, ret[%u]", __func__, ret), HCCL_E_RUNTIME);
    isRegistered_ = false;
    HCCL_INFO("[TaskExceptionHost]%s success.", __func__);
    return HCCL_SUCCESS;
}

TaskExceptionHost *TaskExceptionHostManager::GetHandler(size_t devId)
{
    // 检查 devId 是否越界
    if (devId >= MAX_MODULE_DEVICE_NUM_V2) {
        HCCL_ERROR("[TaskExceptionHost][GetInstance] deviceLogicID[%lu] is invalid", devId);
        return nullptr;
    }

    static TaskExceptionHost handlers_[MAX_MODULE_DEVICE_NUM_V2];
    return &handlers_[devId];
}
TaskExceptionHostManager::TaskExceptionHostManager() {}

TaskExceptionHostManager::~TaskExceptionHostManager() {}

void TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(s32 streamId, u32 deviceLogicId,
    GetAicpuTaskExceptionCallBackHcomm p1)
{
   lock_guard<mutex> lock(g_communicatorCallbackMapMutexV2);
   g_communicatorCallbackMapV2[deviceLogicId].emplace(streamId, p1);
   return ;
}


HcclResult TaskExceptionHost::PrintUbRegisters(s32 devLogicId, RdmaHandle rdmaHandle)
{
    HCCL_INFO("[PrintUbRegister] start, devLogicId[%d], rdmaHandle[%p]", devLogicId, rdmaHandle);
    Hccl::AuxInfoIn in;
    in.cqe.status = 0xffffffff; // 0xffffffff代表查询所有寄存器
    in.auxInfoInType = Hccl::AuxInfoInType::AUX_INFO_IN_TYPE_CQE;
    in.cqe.sR = 0;
    Hccl::AuxInfoOut auxInfo;
    auto ret = Hccl::RaGetAuxInfo(rdmaHandle, in, auxInfo);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[PrintUbRegister]GetUbRegisterInfo failed, devLogicId[%d], rdmaHandle[%p]", devLogicId, rdmaHandle);
        return ret;
    }

    uint16_t isAuxInfoExisted{false};
    for (u32 i = 0; i < auxInfo.auxInfoNum; i++) {
        if (auxInfo.auxInfoValues[i]) { // 非零进行打印
            isAuxInfoExisted = true;
            HCCL_ERROR("devLogicId[%d], cqe_aux_info_type[%u], cqe_aux_info_value[0x%x]",
 	  	            devLogicId, auxInfo.auxInfoTypes[i], auxInfo.auxInfoValues[i]);
 	    } else {
 	        HCCL_INFO("devLogicId[%d], cqe_aux_info_type[%u], cqe_aux_info_value[0x%x]",
 	            devLogicId, auxInfo.auxInfoTypes[i], auxInfo.auxInfoValues[i]);
        }
    }
    if (!isAuxInfoExisted) {
        HCCL_ERROR("devLogicId[%d], all aux_info values are zero.", devLogicId);
    }
    return HCCL_SUCCESS;
}
    

void TaskExceptionHost::Process(rtExceptionInfo_t* exceptionInfo)
{
    if (exceptionInfo == nullptr) {
        HCCL_ERROR("[%s]fail, exceptionInfo is nullptr", __func__);
        return;
    }

    //Task Exception 入口，使用宏捕获执行间异常
    const auto curTask = Hccl::GlobalMirrorTasks::Instance().GetTaskInfo(
        exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);

    if (curTask == nullptr) {
        // 未找到异常对应的TaskInfo
        HCCL_ERROR("[%s]Exception task not found, deviceid:[%u], streamid:[%u], taskid:[%u]",
            __func__, exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);
        return;
    }

    if (curTask->dfxOpInfo_ == nullptr) {
        HCCL_ERROR("[%s]fail, dfxOpInfo is nullptr", __func__);
        return;
    }

    bool isIndop_ = curTask->dfxOpInfo_->isIndop_;
    if (!isIndop_) {
        HCCL_INFO("Start to the old process");
        Hccl::TaskExceptionHandler::Process(exceptionInfo);
        return;
    } 
    HCCL_INFO("Start to the new process");
    if (curTask->taskParam_.taskType == Hccl::TaskParamType::TASK_CCU) {
        ProcessCcuException(exceptionInfo, *curTask); 
    } else {
        ProcessException(exceptionInfo, *curTask);
    }  
}

std::string TaskExceptionHost::GetGroupRankInfo(const Hccl::TaskInfo& taskInfo)
{
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]TaskInfo communicator is nullptr.", __func__);
        return "";
    }

    hccl::CollComm *communicator = static_cast<hccl::CollComm*>(taskInfo.dfxOpInfo_->comm_);
    return Hccl::StringFormat("group:[%s], rankSize[%u], rankId[%d]",
        communicator->GetCommId().c_str(), communicator->GetRankSize(), communicator->GetMyRankId());
}

void TaskExceptionHost::ProcessException(rtExceptionInfo_t* exceptionInfo, const Hccl::TaskInfo& taskInfo)
{
    HCCL_RUN_INFO("[TaskExceptionHost][%s]begin to execute hccl task exception callback function.", __func__);
    if (exceptionInfo == nullptr) {
        HCCL_ERROR("[TaskExceptionHost][ProcessException] exceptionInfo is nullptr.");
        return;
    }
    PrintAicpuErrorMessage(exceptionInfo);
    HCCL_ERROR("[TaskExceptionHost][%s]Task from HCCL run failed.", __func__);
    if (taskInfo.taskParam_.taskType == Hccl::TaskParamType::TASK_NOTIFY_WAIT) {
        PrintTaskContextInfo(exceptionInfo->deviceid, exceptionInfo->streamid, exceptionInfo->taskid);
    }
    HCCL_ERROR("[TaskExceptionHost]Task run failed, base information is deviceID:[%u], %s.",
        exceptionInfo->deviceid, taskInfo.GetBaseInfo().c_str());
    HCCL_ERROR("[TaskExceptionHost]Task run failed, para information is %s.", taskInfo.GetParaInfo().c_str());
    HCCL_ERROR("[TaskExceptionHost]Task run failed, groupRank information is %s.",
        GetGroupRankInfo(taskInfo).c_str());
    HCCL_ERROR("[TaskExceptionHost]Task run failed, opData information is %s.", taskInfo.GetOpInfo().c_str());
}

void TaskExceptionHost::PrintTaskContextInfo(uint32_t deviceId, uint32_t streamId, uint32_t taskId)
{
    Hccl::TaskInfoQueue *queue = nullptr;
    try {
        queue = Hccl::GlobalMirrorTasks::Instance().GetQueue(deviceId, streamId);
    } catch (Hccl::HcclException &e) {
        HCCL_ERROR("Exception task queue  not found. deviceId[%u], streamId[%u].", deviceId, streamId);
        return ;
    }

    if (queue == nullptr) {
        // 未找到异常对应的TaskQueue
        HCCL_ERROR("Exception task queue not found. deviceId[%u], streamId[%u].", deviceId, streamId);
        return;
    }

    auto func = [taskId] (const shared_ptr<Hccl::TaskInfo>& task) { return task->taskId_ == taskId; };
    auto taskItorPtr = queue->Find(func);
    if (taskItorPtr == nullptr || *taskItorPtr == *queue->End()) {
        // 在队列中未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId[%u], taskId[%u].", deviceId, streamId, taskId);
        return;
    }

    // 找到当前异常task的前50个task(至多)
    vector<shared_ptr<Hccl::TaskInfo>> taskContext {};
    for (uint32_t i = 0; i < TASK_CONTEXT_SIZE && *taskItorPtr != *queue->Begin(); ++i, --(*taskItorPtr)) {
        if ((**taskItorPtr)->taskId_ > taskId) {
            HCCL_ERROR("[%s]prev taskId[%u]is bigger than err taskId[%u], traversal end.",
                __func__, (**taskItorPtr)->taskId_, taskId);
            break;
        }
        if ((**taskItorPtr)->taskId_ != taskId) {
            taskContext.emplace_back(**taskItorPtr);
        }
    }

    HCCL_ERROR("[TaskExceptionHost]Task run failed, context sequence before error task is "
        "[SDMA:M(rank), RDMA:RS(rank,id), SendPayload:SP(rank), InlineReduce:IR(rank), Reduce:R(rank), "
        "NotifyRecord:NR(rank,id), NotifyWait:NW(rank,id), SendNotify:SN(rank,id), "
        "WriteWithNotify:WN(rank,id), WriteReduceWithNotify:WRN(rank,id)]:");

    std::string taskContextInfo = "";
    for (auto it = taskContext.rbegin(); it != taskContext.rend(); ++it) {
        std::string conciseInfo = (*it)->GetConciseBaseInfo();
        conciseInfo += ",";

        if (taskContextInfo.size() + conciseInfo.size() >= TASK_CONTEXT_INFO_SIZE) {
            HCCL_ERROR("[TaskExceptionHost]%s", taskContextInfo.c_str());
            taskContextInfo = "";
        }

        taskContextInfo += conciseInfo;
    }
    HCCL_ERROR("[TaskExceptionHost]%s end.", taskContextInfo.c_str());
}
    
 	 
inline void PrintBaseErrorLog(const std::string &stageErrInfo, const std::string &baseInfo)
{
    HCCL_ERROR("%sTask run failed, base information is %s", stageErrInfo.c_str(), baseInfo.c_str());
}

inline void PrintParaErrorLog(const std::string &stageErrInfo, const std::string &paraInfoStr)
{
    HCCL_ERROR("%sTask run failed, para information is %s.", stageErrInfo.c_str(), paraInfoStr.c_str());
}

inline void PrintOpDataErrorLog(const std::string &stageErrInfo, const std::string &opDataContent)
{
    HCCL_ERROR("%sTask run failed, opData information is %s", stageErrInfo.c_str(), opDataContent.c_str());
}

inline void PrintGroupErrorLog(const std::string &stageErrInfo, const std::string &groupRankContent)
{
    HCCL_ERROR("%sTask run failed, groupRank information is %s.", stageErrInfo.c_str(), groupRankContent.c_str());
}

void TaskExceptionHost::PrintGroupErrorMessage(Hccl::ErrorMessageReport &errorMessage, Hccl::TaskInfo &exceptionTaskInfo,
    std::string &groupRankContent, std::string &stageErrInfo)
{
    groupRankContent += "group:[";
    groupRankContent += std::string(errorMessage.group);
    groupRankContent += "], rankSize[";
    groupRankContent += std::to_string(errorMessage.rankSize);
    groupRankContent += "], localRank[";
    groupRankContent += std::to_string(errorMessage.rankId);
    groupRankContent += "], remoteRank[";
    groupRankContent += std::to_string(errorMessage.remoteUserRank);
    groupRankContent += "]";

    PrintGroupErrorLog(stageErrInfo, groupRankContent);
    return;
}

const std::map<HcclReduceOp, std::string> HCOM_REDUCE_OP_STR_MAP{
    {HcclReduceOp::HCCL_REDUCE_SUM, "sum"},
    {HcclReduceOp::HCCL_REDUCE_PROD, "prod"},
    {HcclReduceOp::HCCL_REDUCE_MAX, "max"},
    {HcclReduceOp::HCCL_REDUCE_MIN, "min"},
    {HcclReduceOp::HCCL_REDUCE_RESERVED, "invalid"}
};

inline std::string GetReduceOpEnumStr2(HcclReduceOp reduceOp)
{
    auto iter = HCOM_REDUCE_OP_STR_MAP.find(reduceOp);
    if (iter == HCOM_REDUCE_OP_STR_MAP.end()) {
        return "HcclReduceOp(" + std::to_string(reduceOp) + ")";
    } else {
        return iter->second;
    }
}

const std::map<HcclDataType, std::string> HCOM_DATA_TYPE_STR_MAP{
    {HcclDataType::HCCL_DATA_TYPE_INT8, "int8"},
    {HcclDataType::HCCL_DATA_TYPE_INT16, "int16"},
    {HcclDataType::HCCL_DATA_TYPE_INT32, "int32"},
    {HcclDataType::HCCL_DATA_TYPE_INT64, "int64"},
    {HcclDataType::HCCL_DATA_TYPE_UINT64, "uint64"},
    {HcclDataType::HCCL_DATA_TYPE_FP16, "float16"},
    {HcclDataType::HCCL_DATA_TYPE_FP32, "float32"},
    {HcclDataType::HCCL_DATA_TYPE_UINT8, "uint8"},
    {HcclDataType::HCCL_DATA_TYPE_UINT16, "uint16"},
    {HcclDataType::HCCL_DATA_TYPE_UINT32, "uint32"},
    {HcclDataType::HCCL_DATA_TYPE_FP64, "float64"},
    {HcclDataType::HCCL_DATA_TYPE_BFP16, "bfloat16"},
    {HcclDataType::HCCL_DATA_TYPE_INT128, "int128"},
    {HcclDataType::HCCL_DATA_TYPE_FP8E4M3, "fp8e4m3"},
    {HcclDataType::HCCL_DATA_TYPE_FP8E5M2, "fp8e5m2"},
    {HcclDataType::HCCL_DATA_TYPE_RESERVED, "reserved"}
};

inline std::string GetDataTypeEnumStr2(HcclDataType dataType)
{
    auto iter = HCOM_DATA_TYPE_STR_MAP.find(dataType);
    if (iter == HCOM_DATA_TYPE_STR_MAP.end()) {
        return "HcclDataType(" + std::to_string(dataType) + ")";
    } else {
        return iter->second;
    }
}

inline std::string GetDataTypeEnumStr(u32 dataType)
{
    auto hcclDataType = static_cast<HcclDataType>(dataType);
    return GetDataTypeEnumStr2(hcclDataType);
}
inline std::string GetOpTypeEnumStr(u32 opType)
{
 	Hccl::OpType hcclOpType = static_cast<Hccl::OpType::Value>(opType);
 	return hcclOpType.Describe();
}

void TaskExceptionHost::PrintOpDataErrorMessage(u32 deviceId, Hccl::ErrorMessageReport &errorMessage,
    std::string &stageErrInfo)
{
    std::stringstream opDataStr;
    opDataStr << "src" << "[0x"
            << std::hex << errorMessage.srcAddr << "], dst[0x"
            << std::hex << errorMessage.dstAddr << "], ";

    std::string opStr;
    if (errorMessage.reduceType != HcclReduceOp::HCCL_REDUCE_RESERVED) {
        opStr += "reduceType[";
        opStr += GetReduceOpEnumStr2(static_cast<HcclReduceOp>(errorMessage.reduceType));
        opStr += "], ";
    }

    std::string opDataContent;
    opDataContent += "deviceId:[";
    opDataContent += std::to_string(deviceId);
    opDataContent += "], index[";
    opDataContent += std::to_string(errorMessage.opIndex);
    opDataContent += "], count[";
    opDataContent += std::to_string(errorMessage.count);
    opDataContent += "], ";
    opDataContent += opStr;
    opDataContent += opDataStr.str();
    opDataContent += "dataType[";
    opDataContent += GetDataTypeEnumStr(errorMessage.dataType);
    opDataContent += "].";

    PrintOpDataErrorLog(stageErrInfo, opDataContent);
    return;
}

void ReportErrorMsg(const Hccl::TaskInfo &exceptionTaskInfo, const std::string &groupRankContent,
    const Hccl::ErrorMessageReport &errorMessage, const rtExceptionInfo_t *exceptionInfo)
{
    HCCL_RUN_INFO("[ReportErrorMsg] start, taskType[%d]", exceptionTaskInfo.taskParam_.taskType);
    if (exceptionTaskInfo.taskParam_.taskType == Hccl::TaskParamType::TASK_NOTIFY_WAIT) {
        HCCL_ERROR("[ReportErrorMsg] EI0002");
        RPT_INPUT_ERR(true,
            "EI0002",
            std::vector<std::string>({"remote_rankid", "base_information", "task_information", "group_rank_content"}),
            std::vector<std::string>({
                std::to_string(exceptionTaskInfo.remoteRank_),
                exceptionTaskInfo.GetBaseInfo().c_str(), (exceptionTaskInfo.GetParaInfo()).c_str(),
                ""})
        );
    } else if (exceptionTaskInfo.taskParam_.taskType == Hccl::TaskParamType::TASK_WRITE_REDUCE_WITH_NOTIFY 
        || exceptionTaskInfo.taskParam_.taskType == Hccl::TaskParamType::TASK_WRITE_WITH_NOTIFY
        || exceptionTaskInfo.taskParam_.taskType == Hccl::TaskParamType::TASK_UB_INLINE_WRITE
        || exceptionTaskInfo.taskParam_.taskType == Hccl::TaskParamType::TASK_UB_REDUCE_INLINE
        || exceptionTaskInfo.taskParam_.taskType == Hccl::TaskParamType::TASK_UB) {
        HCCL_ERROR("[ReportErrorMsg] EI0018");
        RPT_INPUT_ERR(true,
            "EI0018",
            std::vector<std::string>({"localServerId", "localDeviceId", "localDeviceIp", "remoteServerId", "remoteDeviceId", "remoteDeviceIp"}),
            std::vector<std::string>({
                "", std::to_string(exceptionInfo->deviceid), errorMessage.locEid.Describe().c_str(), "", "", errorMessage.rmtEid.Describe().c_str()})
            );
    }
}

void GetTaskParam(Hccl::TaskParam &taskParam, const Hccl::ErrorMessageReport &errorMessage) {
    if (errorMessage.taskType == Hccl::TaskParamType::TASK_NOTIFY_WAIT) {
        taskParam.taskPara.Notify.notifyID = errorMessage.notifyId;
        taskParam.taskPara.Notify.value = errorMessage.notifyValue;
    } else if (errorMessage.taskType == Hccl::TaskParamType::TASK_UB_REDUCE_INLINE ||
        errorMessage.taskType == Hccl::TaskParamType::TASK_WRITE_REDUCE_WITH_NOTIFY) {
        taskParam.taskPara.Reduce.notifyID = errorMessage.notifyId;
        taskParam.taskPara.Reduce.notifyValue = errorMessage.notifyValue;
        taskParam.taskPara.Reduce.src = reinterpret_cast<void *>(errorMessage.taskSrcAddr);
 	    taskParam.taskPara.Reduce.dst = reinterpret_cast<void *>(errorMessage.taskDstAddr);
 	    taskParam.taskPara.Reduce.linkType = errorMessage.linkType;
 	    taskParam.taskPara.Reduce.size = errorMessage.size;
    } else if (errorMessage.taskType == Hccl::TaskParamType::TASK_UB_INLINE_WRITE ||
        errorMessage.taskType == Hccl::TaskParamType::TASK_WRITE_WITH_NOTIFY) {
        taskParam.taskPara.DMA.notifyID = errorMessage.notifyId;
        taskParam.taskPara.DMA.notifyValue = errorMessage.notifyValue;
        taskParam.taskPara.DMA.src = reinterpret_cast<void *>(errorMessage.taskSrcAddr);
 	    taskParam.taskPara.DMA.dst = reinterpret_cast<void *>(errorMessage.taskDstAddr);
 	    taskParam.taskPara.DMA.linkType = errorMessage.linkType;
 	    taskParam.taskPara.DMA.size = errorMessage.size;
    }
}

void TaskExceptionHost::PrintAicpuErrorMessage(rtExceptionInfo_t *exceptionInfo)
{
    Hccl::ErrorMessageReport errorMessage;
    unique_lock<std::mutex> lock(g_commHadCallbackArrayMutexV2);
    if (g_commHadCallbackArrayV2[exceptionInfo->deviceid]) {
        // 防止同一个device上出现通信主流和kernel流均出现task exception时runtime调用两次callback
        // HDC通道信息不是读清，防止aicpu task exception重复上报
        HCCL_WARNING("aicpu error message been reported. deviceid[%u]", exceptionInfo->deviceid);
        return;
    }
    lock.unlock();
    if (g_communicatorCallbackMapV2[exceptionInfo->deviceid].find(exceptionInfo->streamid) !=\
        g_communicatorCallbackMapV2[exceptionInfo->deviceid].end()) {
        // 找到对应的通信域，并调用回调函数从HDC通道获取AICPU异常信息
        errorMessage = (g_communicatorCallbackMapV2[exceptionInfo->deviceid])[exceptionInfo->streamid]();
        if (strlen(errorMessage.tag) > 0) {
            std::string groupRankContent;
            u32 streamId = static_cast<u32>(errorMessage.streamId);
            std::string tag = std::string(errorMessage.tag);
            Hccl::TaskParam taskParam{};
            taskParam.taskType = errorMessage.taskType;

            GetTaskParam(taskParam, errorMessage);

            std::shared_ptr<Hccl::DfxOpInfo> dfxOpInfo = std::make_shared<Hccl::DfxOpInfo>();
            dfxOpInfo->tag_ = tag;
            dfxOpInfo->algType_ = errorMessage.algType;
            Hccl::TaskInfo exceptionTaskInfo(streamId, errorMessage.taskId, errorMessage.remoteUserRank, taskParam, dfxOpInfo);
            auto logKeywordL2 = exceptionTaskInfo.taskParam_.taskType ==
                Hccl::TaskParamType::TASK_NOTIFY_WAIT ? Hccl::LOG_KEYWORDS_TIMEOUT : Hccl::LOG_KEYWORDS_RUN_FAILED;
            auto stageErrInfo = "[" + Hccl::LOG_KEYWORDS_TASK_EXEC + "][" + logKeywordL2 + "][" + Hccl::LOG_KEYWORDS_AICPU + "]";
            HCCL_ERROR("%sTask from HCCL run failed.", stageErrInfo.c_str());
            // 防止tag字符串过长， 信息分开打印
            PrintBaseErrorLog(stageErrInfo, exceptionTaskInfo.GetBaseInfo());
            PrintParaErrorLog(stageErrInfo, exceptionTaskInfo.GetParaInfo());
            PrintGroupErrorMessage(errorMessage, exceptionTaskInfo, groupRankContent, stageErrInfo);
            PrintOpDataErrorMessage(exceptionInfo->deviceid, errorMessage, stageErrInfo);
            HCCL_ERROR("errorMessage taskType[%s], rtCqErrorType[%u], rtCqErrorCode[%u]. ",
                errorMessage.taskType.Describe().c_str(), (u32)errorMessage.rtCqErrorType, errorMessage.rtCqErrorCode);

            // 打印UB DFX寄存器信息
            if (errorMessage.taskType == Hccl::TaskParamType::TASK_WRITE_WITH_NOTIFY ||
                errorMessage.taskType == Hccl::TaskParamType::TASK_WRITE_REDUCE_WITH_NOTIFY ||
                errorMessage.taskType == Hccl::TaskParamType::TASK_UB_INLINE_WRITE ||
                errorMessage.taskType == Hccl::TaskParamType::TASK_UB_REDUCE_INLINE ||
                errorMessage.taskType == Hccl::TaskParamType::TASK_UB) {
                HCCL_ERROR("errorMessage ubCqeStatus[%u], localEid[%s], remoteEid[%s]. ", (u32)errorMessage.ubCqeStatus,
                errorMessage.locEid.Describe().c_str(), errorMessage.rmtEid.Describe().c_str());
                auto reverseAddr = Hccl::IpAddress(errorMessage.locEid);
                auto addr = Hccl::IpAddress(reverseAddr.GetReverseEid());
                u32 devPhyId = Hccl::HrtGetDevicePhyIdByIndex(exceptionInfo->deviceid);
                auto rdmaHandle = Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId, addr);
                PrintUbRegisters(static_cast<s32>(exceptionInfo->deviceid), rdmaHandle);
            }

            ReportErrorMsg(exceptionTaskInfo, groupRankContent, errorMessage, exceptionInfo);

            lock.lock();
            g_commHadCallbackArrayV2[exceptionInfo->deviceid] = true;
        } else {
            HCCL_WARNING("PrintAicpuErrorMessage No Vaild errorMessage!");
        }
    } else {
        HCCL_INFO("PrintAicpuErrorMessage streamId[%u] is not found.", exceptionInfo->streamid);
    }
}
// CCU新加代码

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
void TaskExceptionHost::ProcessCcuException(const rtExceptionInfo_t* exceptionInfo, const Hccl::TaskInfo& taskInfo)
{
    auto deviceId = exceptionInfo->deviceid;
    HCCL_ERROR("[TaskExceptionHost][%s]Task from HCCL run failed.", __func__);
    HCCL_ERROR("[TaskExceptionHost]Task run failed, base information is deviceID:[%u], %s.",
        deviceId, taskInfo.GetBaseInfo().c_str());
    HCCL_ERROR("[TaskExceptionHost]Task run failed, groupRank information is %s.",
        GetGroupRankInfo(taskInfo).c_str());
    HCCL_ERROR("[TaskExceptionHost]Task run failed, opData information is %s.", taskInfo.GetOpInfo().c_str());
    auto& ccuExDetailInfo = exceptionInfo->expandInfo.u.ccuInfo;
    for (uint32_t i = 0; i < ccuExDetailInfo.ccuMissionNum; ++i) { // ccuExDetailInfo.ccuMissionNum为1
        const auto& missionInfo = ccuExDetailInfo.missionInfo[i]; // 异常mission
        uint16_t status = static_cast<uint16_t>(missionInfo.status) << BYTE | missionInfo.subStatus;
        PrintCcuErrorInfo(deviceId, status, taskInfo);
        // 打印寄存器信息
        PrintPanicLogInfo(missionInfo.panicLog);
    }

    const int32_t devLogicId = static_cast<int32_t>(deviceId);
    if (Hccl::CcuCleanTaskKillState(devLogicId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TaskExceptionHost][%s] failed to clean ccu task kill state, "
            "devLogicId[%d].", __func__, devLogicId);
    }

    const uint8_t dieId = taskInfo.taskParam_.taskPara.Ccu.dieId;
    if (Hccl::CcuCleanDieCkes(devLogicId, dieId) != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[TaskExceptionHost][%s] failed to clean ccu die ckes, "
            "dieId[%u], devLogicId[%d].", __func__, dieId, devLogicId);
    }
}
void TaskExceptionHost::PrintCcuErrorInfo(uint32_t deviceId, uint16_t status, const Hccl::TaskInfo& taskInfo)
{
    const Hccl::ParaCcu& ccuTaskParam = taskInfo.taskParam_.taskPara.Ccu;
    vector<Hccl::CcuErrorInfo> errorInfos {};
    HcclResult ret = GetCcuErrorMsg(deviceId, status, ccuTaskParam, errorInfos);
    const uint8_t missionStatus = (status >> 8) & 0xFF;
    if (ret != HcclResult::HCCL_SUCCESS || errorInfos.empty()) {
        HCCL_ERROR("Get CCU error info failed. deviceId[%u], dieId[%u], missionId[%u], executeId[%llu].",
            deviceId, ccuTaskParam.dieId, ccuTaskParam.missionId,
            ccuTaskParam.executeId);
        return;
    }
    PrintCcuErrorLog(errorInfos, taskInfo);

    if (missionStatus >= 0x01 && missionStatus <= 0x05) { // 如果是UB错误(missionStatus为[0x01, 0x05])，打印Ub Dfx寄存器信息
        PrintCcuUbRegisters(static_cast<s32>(deviceId), taskInfo.taskParam_.taskPara.Ccu);
    }
}
void TaskExceptionHost::PrintCcuErrorLog(const std::vector<Hccl::CcuErrorInfo>& errorInfos, const Hccl::TaskInfo& taskInfo)
{
    if (errorInfos.empty()) {
        return;
    }
    HCCL_ERROR("[TaskExceptionHost]Task run failed, ccu runtime information is: %s", __func__);
    for (const auto& errorInfo : errorInfos) {
        HCCL_ERROR("[TaskExceptionHost][%s]", GetCcuErrorMsgByType(errorInfo, taskInfo).c_str());
    }
}
HcclResult TaskExceptionHost::PrintCcuUbRegisters(s32 devLogicId, const Hccl::ParaCcu &ccuTaskParam)
{
    std::vector<Hccl::CcuJetty *> ccuJettys;
    HcclResult ret = Hccl::GetCcuJettys(devLogicId, ccuTaskParam, ccuJettys);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("PrintCcuUbRegisters failed");
    }
    u32 jettyNum = ccuJettys.size();
    std::vector<Hccl::JettyHandle> jettyHandles;
    for (auto &ccuJetty : ccuJettys) {
        jettyHandles.push_back(ccuJetty->GetJettyHandle());
    }

    std::vector<Hccl::JettyStatus> jettyStatusVec;
    RaBatchQueryJettyStatus(jettyHandles, jettyStatusVec, jettyNum);

    for (u32 i = 0; i < jettyNum; ++i) {
        if (jettyStatusVec[i] == Hccl::JettyStatus::ERROR) {
            auto rdmaHandle = ccuJettys[i]->GetRdmaHandle();
            HCCL_ERROR("PrintCcuUbRegisters jettyId[%u]", ccuJettys[i]->GetJettyId());
            PrintUbRegisters(devLogicId, rdmaHandle);
            break;
        }
    }
    return HCCL_SUCCESS;
}

string TaskExceptionHost::GetCcuLenErrorMsg(const uint64_t len)
{
    if ((0 < len) && (len <= CCU_MSG_256MB_LEN)) {
        return "";
    }
    return StringFormat("ccu transMem Len[%llu]B > 256MB or is zero, not support!", len);
}

string TaskExceptionHost::GetCcuErrorMsgLoop(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    return Hccl::StringFormat("InstrId[%u]: Loop startInstrId[%u], endInstrId[%u], executorId[%u], "
                        "totalIter[%u], curIter[%u], addressStride[0x%llx]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.loop.startInstrId, ccuErrorInfo.msg.loop.endInstrId,
                        ccuErrorInfo.msg.loop.loopEngineId, ccuErrorInfo.msg.loop.loopCnt,
                        ccuErrorInfo.msg.loop.loopCurrentCnt, ccuErrorInfo.msg.loop.addrStride);
}

string TaskExceptionHost::GetCcuErrorMsgLoopGroup(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    return Hccl::StringFormat("InstrId[%u]: LoopGroup startLoopInsId[%u], loopInsCnt[%u], "
                        "expandOffset[%u], expandCnt[%u]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.loopGroup.startLoopInsId,
                        ccuErrorInfo.msg.loopGroup.loopInsCnt, ccuErrorInfo.msg.loopGroup.expandOffset,
                        ccuErrorInfo.msg.loopGroup.expandCnt);
}

string TaskExceptionHost::GetCcuErrorMsgLocPostSem(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    return Hccl::StringFormat("InstrId[%u]: Set sem[%u], semValue[0x%04x], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHost::GetCcuErrorMsgLocWaitSem(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    return Hccl::StringFormat("InstrId[%u]: Wait sem[%u], semValue[0x%04x], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalValue,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHost::GetCcuErrorMsgRemPostSem(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    return Hccl::StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x], rankId[%d]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHost::GetCcuErrorMsgRemWaitSem(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    return Hccl::StringFormat("InstrId[%u]: Wait, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalValue, ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHost::GetCcuErrorMsgRemPostVar(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    return Hccl::StringFormat("InstrId[%u]: Post Variable[0x%016llx] To Param[%u], Use sem[%u], mask[0x%04x], rankId[%d]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.paramValue,
                        ccuErrorInfo.msg.waitSignal.paramId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalMask,
                        GetRankIdByChannelId(ccuErrorInfo.msg.waitSignal.channelId[0], taskInfo));
}

string TaskExceptionHost::GetCcuErrorMsgRemWaitGroup(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
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
    return Hccl::StringFormat("InstrId[%u]: Wait Group, Use sem[%u], semValue[0x%04x], mask[0x%04x], rankIds[%s]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalValue, ccuErrorInfo.msg.waitSignal.signalMask,
                        ranks.str().c_str());
}

string TaskExceptionHost::GetCcuErrorMsgPostSharedVar(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    return Hccl::StringFormat("InstrId[%u]: Post Shared Variable[%u] from Variable[0x%016llx], "
                        "Use sem[%u], mask[0x%04x]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.waitSignal.paramId,
                        ccuErrorInfo.msg.waitSignal.paramValue, ccuErrorInfo.msg.waitSignal.signalId,
                        ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHost::GetCcuErrorMsgPostSharedSem(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    return Hccl::StringFormat("InstrId[%u]: Post, Use sem[%u], mask[0x%04x]", ccuErrorInfo.instrId,
                        ccuErrorInfo.msg.waitSignal.signalId, ccuErrorInfo.msg.waitSignal.signalMask);
}

string TaskExceptionHost::GetCcuErrorMsgRead(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return Hccl::StringFormat(
        "InstrId[%u]: Read Memory[0x%016llx] To Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.rmtAddr, ccuErrorInfo.msg.transMem.locAddr,
        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId, ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgWrite(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return Hccl::StringFormat(
        "InstrId[%u]: Write Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
        "Set sem[%u] with mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId, ccuErrorInfo.msg.transMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.transMem.channelId, taskInfo),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgLocalCpy(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return Hccl::StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask, printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgLocalReduce(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.transMem.len);
    return Hccl::StringFormat("InstrId[%u]: Read Memory[0x%016llx] to Memory[0x%016llx], Len[%llu], "
                        "Set sem[%u] with mask[0x%04x], dataType[%u], opType[%u] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.transMem.locAddr, ccuErrorInfo.msg.transMem.rmtAddr,
                        ccuErrorInfo.msg.transMem.len, ccuErrorInfo.msg.transMem.signalId,
                        ccuErrorInfo.msg.transMem.signalMask, ccuErrorInfo.msg.transMem.dataType,
                        ccuErrorInfo.msg.transMem.opType, printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgBufRead(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return Hccl::StringFormat(
        "InstrId[%u]: Read Rmt Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.addr, ccuErrorInfo.msg.bufTransMem.bufId,
        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId, ccuErrorInfo.msg.bufTransMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgBufWrite(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    auto pair = GetAddrPairByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo);
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return Hccl::StringFormat(
        "InstrId[%u]: Write CcuBuffer[%u] To Rmt Mem[0x%016llx], Len[%llu], "
        "sem[%u], mask[0x%04x], remoteRankId[%d], srcEID[%s], dstEID[%s] %s",
        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.bufId, ccuErrorInfo.msg.bufTransMem.addr,
        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId, ccuErrorInfo.msg.bufTransMem.signalMask,
        GetRankIdByChannelId(ccuErrorInfo.msg.bufTransMem.channelId, taskInfo),
        pair.first.Describe().c_str(),
        pair.second.Describe().c_str(), printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgBufLocRead(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return Hccl::StringFormat("InstrId[%u]: Read Loc Mem[0x%016llx] To CcuBuffer[%u], Len[%llu], sem[%u], mask[0x%04x] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.addr, ccuErrorInfo.msg.bufTransMem.bufId,
                        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask, printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgBufLocWrite(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    string printMsg = GetCcuLenErrorMsg(ccuErrorInfo.msg.bufTransMem.len);
    return Hccl::StringFormat("InstrId[%u]: Write CcuBuffer[%u] To Loc Mem[0x%016llx], Len[%llu], sem[%u], mask[0x%04x] %s",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufTransMem.bufId, ccuErrorInfo.msg.bufTransMem.addr,
                        ccuErrorInfo.msg.bufTransMem.len, ccuErrorInfo.msg.bufTransMem.signalId,
                        ccuErrorInfo.msg.bufTransMem.signalMask, printMsg.c_str());
}

string TaskExceptionHost::GetCcuErrorMsgBufReduce(const Hccl::CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo)
{
    (void)taskInfo;
    stringstream buffIds;
    for (uint32_t i = 0; i < Hccl::BUF_REDUCE_ID_SIZE; ++i) {
        const auto buffId = ccuErrorInfo.msg.bufReduce.bufIds[i];
        if (buffId == UINT16_MAX) {
            break;
        }
        if (i != 0) {
            buffIds << ", ";
        }
        buffIds << to_string(buffId);
    }

    return Hccl::StringFormat("InstrId[%u]: Buffer Reduce count[%u], dataType[%u], outputDataType[%u], opType[%u], "
                        "sem[%u], mask[0x%04x], CcuBuffers[%s]",
                        ccuErrorInfo.instrId, ccuErrorInfo.msg.bufReduce.count, ccuErrorInfo.msg.bufReduce.dataType,
                        ccuErrorInfo.msg.bufReduce.outputDataType, ccuErrorInfo.msg.bufReduce.opType,
                        ccuErrorInfo.msg.bufReduce.signalId, ccuErrorInfo.msg.bufReduce.signalMask,
                        buffIds.str().c_str());
}

string TaskExceptionHost::GetCcuErrorMsgDefault(const Hccl::CcuErrorInfo &ccuErrorInfo)
{
    return Hccl::StringFormat("InstrId[%u]: CcuErrorType[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.type.Describe().c_str());
}

string TaskExceptionHost::GetCcuErrorMsgMission(const Hccl::CcuErrorInfo &ccuErrorInfo)
{
    return Hccl::StringFormat("InstrId[%u]: dieId[%u], missionId[%u], missionError[%s]",
        ccuErrorInfo.instrId, ccuErrorInfo.dieId, ccuErrorInfo.missionId,
        ccuErrorInfo.msg.mission.missionError);
}

string TaskExceptionHost::GetCcuErrorMsgByType(const Hccl::CcuErrorInfo& ccuErrorInfo, const Hccl::TaskInfo& taskInfo)
{
    if (ccuErrorInfo.type == Hccl::CcuErrorType::MISSION) {
        return GetCcuErrorMsgMission(ccuErrorInfo);
    }

    using GetCcuErrorMsgFunc = string (*)(const Hccl::CcuErrorInfo& ccuErrorInfo, const Hccl::TaskInfo& taskInfo);
    static const map<Hccl::CcuRep::CcuRepType, GetCcuErrorMsgFunc> handlerMap {
        {Hccl::CcuRep::CcuRepType::LOOP, &TaskExceptionHost::GetCcuErrorMsgLoop},
        {Hccl::CcuRep::CcuRepType::LOOPGROUP, &TaskExceptionHost::GetCcuErrorMsgLoopGroup},
        {Hccl::CcuRep::CcuRepType::LOC_POST_SEM, &TaskExceptionHost::GetCcuErrorMsgLocPostSem},
        {Hccl::CcuRep::CcuRepType::LOC_WAIT_SEM, &TaskExceptionHost::GetCcuErrorMsgLocWaitSem},
        {Hccl::CcuRep::CcuRepType::REM_POST_SEM, &TaskExceptionHost::GetCcuErrorMsgRemPostSem},
        {Hccl::CcuRep::CcuRepType::REM_WAIT_SEM, &TaskExceptionHost::GetCcuErrorMsgRemWaitSem},
        {Hccl::CcuRep::CcuRepType::REM_POST_VAR, &TaskExceptionHost::GetCcuErrorMsgRemPostVar},
        {Hccl::CcuRep::CcuRepType::REM_WAIT_GROUP, &TaskExceptionHost::GetCcuErrorMsgRemWaitGroup},
        {Hccl::CcuRep::CcuRepType::POST_SHARED_VAR, &TaskExceptionHost::GetCcuErrorMsgPostSharedVar},
        {Hccl::CcuRep::CcuRepType::POST_SHARED_SEM, &TaskExceptionHost::GetCcuErrorMsgPostSharedSem},
        {Hccl::CcuRep::CcuRepType::READ, &TaskExceptionHost::GetCcuErrorMsgRead},
        {Hccl::CcuRep::CcuRepType::WRITE, &TaskExceptionHost::GetCcuErrorMsgWrite},
        {Hccl::CcuRep::CcuRepType::LOCAL_CPY, &TaskExceptionHost::GetCcuErrorMsgLocalCpy},
        {Hccl::CcuRep::CcuRepType::LOCAL_REDUCE, &TaskExceptionHost::GetCcuErrorMsgLocalReduce},
        {Hccl::CcuRep::CcuRepType::BUF_READ, &TaskExceptionHost::GetCcuErrorMsgBufRead},
        {Hccl::CcuRep::CcuRepType::BUF_WRITE, &TaskExceptionHost::GetCcuErrorMsgBufWrite},
        {Hccl::CcuRep::CcuRepType::BUF_LOC_READ, &TaskExceptionHost::GetCcuErrorMsgBufLocRead},
        {Hccl::CcuRep::CcuRepType::BUF_LOC_WRITE, &TaskExceptionHost::GetCcuErrorMsgBufLocWrite},
        {Hccl::CcuRep::CcuRepType::BUF_REDUCE, &TaskExceptionHost::GetCcuErrorMsgBufReduce}
    };

    const auto funcIt = handlerMap.find(ccuErrorInfo.repType);
    if (funcIt == handlerMap.end()) {
        return GetCcuErrorMsgDefault(ccuErrorInfo);
    } else {
        return funcIt->second(ccuErrorInfo, taskInfo);
    }
}

// RankId TaskExceptionHost::GetRankIdByChannelId(uint16_t channelId, const Hccl::TaskInfo &taskInfo)
// {
//     if (taskInfo.taskParam_.taskType != Hccl::TaskParamType::TASK_CCU) {
//         HCCL_ERROR("[TaskException][%s]Get RankId failed, task type error.", __func__);
//         return INVALID_RANKID;
//     }
//     if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
//         HCCL_ERROR("[TaskException][%s]Get RankId failed, communicator is nullptr.", __func__);
//         return INVALID_RANKID;
//     }
//     const Hccl::CommunicatorImpl* communicator = (Hccl::CommunicatorImpl*)taskInfo.dfxOpInfo_->comm_;
//     auto* collServiceBase = communicator->GetCcuCollService();
//     if (collServiceBase == nullptr) {
//         HCCL_ERROR("[TaskException][%s]Failed to get collService from communicator.", __func__);
//         return INVALID_RANKID;
//     }
//     auto         *collServiceCcu = static_cast<CollServiceDeviceMode *>(collServiceBase);
//     const uint8_t dieId          = taskInfo.taskParam_.taskPara.Ccu.dieId;
//     return collServiceCcu->GetCcuInsPreprocessor()->GetCcuComm()->GetCcuJettyMgr()->GetRemoteRankIdByChannelId(
//         dieId, channelId);
// }

// std::pair<Hccl::IpAddress, Hccl::IpAddress> TaskExceptionHost::GetAddrPairByChannelId(uint16_t        channelId,
//                                                                              const Hccl::TaskInfo &taskInfo)
// {
//     std::pair<Hccl::IpAddress, Hccl::IpAddress> dummy = {Hccl::IpAddress(), Hccl::IpAddress()};
//     if (taskInfo.taskParam_.taskType != Hccl::TaskParamType::TASK_CCU) {
//         HCCL_ERROR("[TaskException][%s]Get AddrPair failed, task type error[%s]", __func__,
//                    taskInfo.taskParam_.Describe().c_str());
//         return dummy;
//     }
//     if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
//         HCCL_ERROR("[TaskException][%s]Get AddrPair failed, communicator is nullptr.", __func__);
//         return dummy;
//     }
//     const Hccl::CommunicatorImpl *communicator    = (Hccl::CommunicatorImpl *)taskInfo.dfxOpInfo_->comm_;
//     auto                   *collServiceBase = communicator->GetCcuCollService();
//     if (collServiceBase == nullptr) {
//         HCCL_ERROR("[TaskException][%s]Failed to get collService from communicator.", __func__);
//         return dummy;
//     }
//     auto         *collServiceCcu = static_cast<Hccl::CollServiceDeviceMode *>(collServiceBase);
//     const uint8_t dieId          = taskInfo.taskParam_.taskPara.Ccu.dieId;
//     return collServiceCcu->Hccl::HGetCcuInsPreprocessor()->Hccl::GetCcuComm()->Hccl::GetCcuJettyMgr()->Hccl::GetAddrPairByChannelId(
//         dieId, channelId);
// }


} // namespace Hccl