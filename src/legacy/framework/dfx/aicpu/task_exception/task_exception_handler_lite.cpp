/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "task_exception_handler_lite.h"
#include "task_exception_func.h"
#include "communicator_impl_lite.h"
#include "error_message_v2.h"
#include "dlhal_function_v2.h"
#include "sqe_v82.h"
#include "task_param.h"

#include "task_struct.h"
#include "orion_adapter_hal.h"

namespace Hccl {

using namespace std;

constexpr uint32_t TASK_CONTEXT_SIZE = 50;
constexpr uint32_t TASK_CONTEXT_INFO_SIZE = LOG_TMPBUF_SIZE - 50; // task 执行失败时打印前序task信息的长度限制

TaskExceptionHandlerLite &TaskExceptionHandlerLite::GetInstance()
{
    static TaskExceptionHandlerLite instance;
    return instance;
}

TaskExceptionHandlerLite::TaskExceptionHandlerLite()
{
    Register();
}

TaskExceptionHandlerLite::~TaskExceptionHandlerLite()
{
}

void TaskExceptionHandlerLite::Register() const
{
    TaskExceptionFunc::GetInstance().RegisterCallback(Process);
}

HcclResult GenerateErrorMessageReport(CommunicatorImplLite *aicpuComm, std::shared_ptr<TaskInfo> taskInfo, ErrorMessageReport &errMsgInfo)
{
    // 获取需要上报的关键信息
    errMsgInfo.remoteUserRank = taskInfo->remoteRank_;
    errMsgInfo.streamId = taskInfo->streamId_;
    errMsgInfo.taskId = taskInfo->taskId_;
    errMsgInfo.rankId = aicpuComm->GetMyRank();
    errMsgInfo.rankSize = aicpuComm->GetRankSize();
    errMsgInfo.algType = taskInfo->dfxOpInfo_ == nullptr ? static_cast<Hccl::AlgType>(AlgType::MESH) : taskInfo->dfxOpInfo_->algType_;
    errMsgInfo.opIndex = taskInfo->dfxOpInfo_ == nullptr ? 0 : taskInfo->dfxOpInfo_->index_;
    errMsgInfo.count = taskInfo->taskParam_.taskPara.DMA.size;
    errMsgInfo.dataType = taskInfo->taskParam_.taskPara.Reduce.dataType;
    errMsgInfo.dstAddr = reinterpret_cast<u64>(taskInfo->taskParam_.taskPara.DMA.dst);
    errMsgInfo.srcAddr = reinterpret_cast<u64>(taskInfo->taskParam_.taskPara.DMA.src);
    errMsgInfo.taskType = taskInfo->taskParam_.taskType;
    errMsgInfo.notifyId = taskInfo->taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT ?
        taskInfo->taskParam_.taskPara.Notify.notifyID : taskInfo->taskParam_.taskPara.DMA.notifyID;
    errMsgInfo.reduceType = taskInfo->taskParam_.taskPara.Reduce.reduceOp;
    memcpy_s(errMsgInfo.tag, sizeof(errMsgInfo.tag), aicpuComm->GetCurrentOp().opTag.c_str(),
        aicpuComm->GetCurrentOp().opTag.size());
    memcpy_s(errMsgInfo.group, sizeof(errMsgInfo.group), aicpuComm->GetId().c_str(), aicpuComm->GetId().size());
    if (taskInfo->taskParam_.taskType == TaskParamType::TASK_WRITE_REDUCE_WITH_NOTIFY || taskInfo->taskParam_.taskType == TaskParamType::TASK_WRITE_WITH_NOTIFY) {
        errMsgInfo.locEid = taskInfo->taskParam_.taskPara.DMA.locEid;
        errMsgInfo.rmtEid = taskInfo->taskParam_.taskPara.DMA.rmtEid;
    }
    return HCCL_SUCCESS;
}

HcclResult SendErrorMessageReportToHost(CommunicatorImplLite *aicpuComm, ErrorMessageReport &errMsgInfo)
{
    CHK_RET(aicpuComm->SendErrorMessageReportToHost(errMsgInfo));
    return HCCL_SUCCESS;
}

constexpr u32 RT_SDMA_COMPERR = 0x9; // A3 sdma error类型为0x9时，表示写拷贝发生超时代答，或者数据搬移时地址译码错误
constexpr u32 RT_SDMA_COMPDATAERR = 0xa; // A3 sdma error类型为0xa时，表示读拷贝发生超时代答，或者读HBM返回ERROR
constexpr u32 RT_SDMA_DATAERR = 0x8; // A3 sdma error类型为0x8时，表示读HBM返回ERROR

// 把SDMA类错误码转换成Ts对应的错误码
uint16_t SwitchSdmaCqeErrCodeToTsErrCode(u32 cqeErrCode){
    switch (cqeErrCode) {
        case RT_SDMA_COMPERR:
            return TS_ERROR_SDMA_LINK_ERROR;
        case RT_SDMA_COMPDATAERR:
            return TS_ERROR_SDMA_POISON_ERROR;
        case RT_SDMA_DATAERR:
            return TS_ERROR_SDMA_DDRC_ERROR;
        default:
            return TS_ERROR_HCCL_OTHER_ERROR;
    }
}

HcclResult SendTaskExceptionByMBox(const u32 localDeviceId, const u32 notifyId, const u32 tsId,
    const s32 userStreamId, const u32 cqeErrCode)
{
    CHK_PRT_RET((halEschedSubmitEvent == nullptr), HCCL_ERROR("halEschedSubmitEvent is nullptr, "
        "Does not support this interface."), HCCL_E_DRV);
    ts_aicpu_sqe_t aicpuSqe = {};
    u32 hostpid = 0;
    u32 vf_id = 0;
    int pid = getpid();
    HCCL_INFO("SendTaskExceptionByMBox getpid[%d]", pid);
    // 调整drvQueryProcessHostPid获取pid和vf_id的值
    CHK_RET(HrtHalDrvQueryProcessHostPid(pid, nullptr, &vf_id, &hostpid, nullptr));

    aicpuSqe.pid = hostpid;
    aicpuSqe.cmd_type = AICPU_RECORD;
    aicpuSqe.vf_id = vf_id;
    aicpuSqe.tid = 0U;  // notify is no need tid
    aicpuSqe.u.aicpu_record.record_type = AICPU_MSG_NOTIFY_RECORD;
    aicpuSqe.u.aicpu_record.record_id = notifyId;

    aicpuSqe.ts_id = static_cast<uint8_t>(tsId);

    aicpuSqe.u.aicpu_record.fault_stream_id = static_cast<uint16_t>(userStreamId);

    aicpuSqe.u.aicpu_record.ret_code = SwitchSdmaCqeErrCodeToTsErrCode(cqeErrCode);

    struct event_summary event;
    event.dst_engine = TS_CPU;
    event.policy = ONLY;
    event.pid = 0;
    event.grp_id = 0;
    event.event_id = EVENT_TS_CTRL_MSG;
    event.subevent_id = 0U;
    event.msg_len = static_cast<uint32_t>(sizeof(ts_aicpu_sqe_t));
    event.msg = reinterpret_cast<char_t *>(&aicpuSqe);
    auto ret = halEschedSubmitEvent(localDeviceId, &event);
    if (ret != static_cast<int32_t>(DRV_ERROR_NONE)) {
        HCCL_ERROR("[SendTaskExceptionByMBox]Send msg async to ts failed. ret=%d, streamId=%d, "
                "notifyId=%u.", ret, userStreamId, notifyId);
        return HCCL_E_DRV;
    }
    HCCL_RUN_INFO("[SendTaskExceptionByMBox]Send msg async to ts fininsh. streamId=%d, notifyId=%u, msg_size=%u, "
        "hostpid=%u, vf_id=%u, errCode=%u.", userStreamId, notifyId,
        static_cast<uint32_t>(sizeof(ts_aicpu_sqe_t)),  hostpid, vf_id, cqeErrCode);
    return HCCL_SUCCESS;
}

HcclResult SendTaskExceptionByMBox(CommunicatorImplLite *aicpuComm, const uint16_t &rsErrorCode)
{
    u32 localDeviceId = 0;
    u32 notifyId = aicpuComm->GetHostDeviceSyncNotifyLiteMgr()->GetHostWaitNotify()->GetId();
    u32 devPhyId = aicpuComm->GetHostDeviceSyncNotifyLiteMgr()->GetHostWaitNotify()->GetDevPhyId();

    HCCL_INFO("[HcclCommAicpu][SendTaskExceptionByMBox] HostToDeviceLogicId[%u]", devPhyId);
    auto ret = drvGetLocalDevIDByHostDevID(devPhyId, &localDeviceId);
    if (ret != 0) {
        HCCL_ERROR("[HcclCommAicpu][SendTaskExceptionByMBox] HostToDeviceLogicId[%u] failed.",
        devPhyId);
        return HCCL_E_DRV;
    }
    HCCL_INFO("[HcclCommAicpu][SendTaskExceptionByMBox] HostToDeviceLogicId[%u], localDeviceid[%u]",
        devPhyId, localDeviceId);

    
    CHK_RET(SendTaskExceptionByMBox(localDeviceId, notifyId, 0, aicpuComm->GetUserStreamId(), rsErrorCode));
    return HCCL_SUCCESS;
}

void TaskExceptionHandlerLite::Process(CommunicatorImplLite *aicpuComm, rtLogicCqReport_t* exceptionInfo)
{
    if (exceptionInfo == nullptr) {
        HCCL_ERROR("Exception process failed, rtExceptionInfo is nullptr.");
        return;
    }
    // exceptionInfo->taskId和exceptionInfo->streamId拼成sqeId
    const u32 sqeId = static_cast<uint32_t>(exceptionInfo->taskId << 16) | static_cast<uint32_t>(exceptionInfo->streamId);
    const auto curTask = GlobalMirrorTasks::Instance().GetTaskInfo(
            0, exceptionInfo->sqId, sqeId);
    if (curTask == nullptr) {
        // 未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId(sqId)[%u], taskId(sqeId)[%u].",
                   0, exceptionInfo->sqId, sqeId);
        return;
    }

    // 每个通信域仅首次上报（N秒快恢时重置）
    if (!aicpuComm->IsErrorReported()) {
        // 1) errorMessage上报
        ErrorMessageReport errMsgInfo{};
        auto ret = GenerateErrorMessageReport(aicpuComm, curTask, errMsgInfo);
        if (ret != HCCL_SUCCESS) {
            THROW<InvalidParamsException>("GenerateErrorMessageReport failed.");
        }
        ret = SendErrorMessageReportToHost(aicpuComm, errMsgInfo);
        if (ret != HCCL_SUCCESS) {
            THROW<InvalidParamsException>("SendErrorMessageReportToHost Failed.");
        }
        
        aicpuComm->SetErrorReported();
    }

    HCCL_ERROR("[TaskExceptionHandlerLite][%s]Task from HCCL run failed.", __func__);
    if (curTask->taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT) {
        PrintTaskContextInfo(exceptionInfo->sqId, sqeId);
    }
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, base information is %s.", curTask->GetBaseInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, para information is %s.", curTask->GetParaInfo().c_str());
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, groupRank information is %s.",
        GetGroupRankInfo(*curTask).c_str());
    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, opData information is %s.", curTask->GetOpInfo().c_str());
}

string TaskExceptionHandlerLite::GetGroupRankInfo(const TaskInfo& taskInfo)
{
    if (taskInfo.dfxOpInfo_ == nullptr || taskInfo.dfxOpInfo_->comm_ == nullptr) {
        HCCL_ERROR("[TaskInfo][%s]TaskInfo communicator is nullptr.", __func__);
        return "";
    }
    const CommunicatorImplLite* commImplLite = static_cast<CommunicatorImplLite*>(taskInfo.dfxOpInfo_->comm_);
    return StringFormat("group:[%s], rankSize[%u], rankId[%d]",
        commImplLite->GetId().c_str(), commImplLite->GetRankSize(), commImplLite->GetMyRank());
}

void TaskExceptionHandlerLite::PrintTaskContextInfo(uint32_t sqId, uint32_t taskId)
{
    auto queue = GlobalMirrorTasks::Instance().GetQueue(0, sqId);
    if (queue == nullptr) {
        // 未找到异常对应的TaskQueue
        HCCL_ERROR("Exception task queue not found. deviceId[%u], streamId(sqId)[%u].", 0, sqId);
        return;
    }

    auto func = [taskId] (const shared_ptr<TaskInfo>& task) { return task->taskId_ == taskId; };
    auto taskItorPtr = queue->Find(func);
    if (taskItorPtr == nullptr || *taskItorPtr == *queue->End()) {
        // 在队列中未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId(sqId)[%u], taskId(sqeId)[%u].", 0, sqId, taskId);
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

    HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, context sequence before error task is "
        "[SDMA:M(rank), RDMA:RS(rank,id), SendPayload:SP(rank), InlineReduce:IR(rank), Reduce:R(rank), "
        "NotifyRecord:NR(rank,id), NotifyWait:NW(rank,id), SendNotify:SN(rank,id), "
        "WriteWithNotify:WN(rank,id), WriteReduceWithNotify:WRN(rank,id)]:");

    string taskContextInfo = "";
    for (auto it = taskContext.rbegin(); it != taskContext.rend(); ++it) {
        string conciseInfo = (*it)->GetConciseBaseInfo();
        conciseInfo += ",";

        if (taskContextInfo.size() + conciseInfo.size() >= TASK_CONTEXT_INFO_SIZE) {
            HCCL_ERROR("[TaskExceptionHandlerLite]%s", taskContextInfo.c_str());
            taskContextInfo = "";
        }

        taskContextInfo += conciseInfo;
    }
    HCCL_ERROR("[TaskExceptionHandlerLite]%s end.", taskContextInfo.c_str());
}

} // namespace Hccl