/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hcclCommTaskExceptionLite.h"
#include "aicpu_indop_process.h"
#include "stream_lite.h"
#include "global_mirror_tasks.h"

namespace hcomm {
HcclCommTaskExceptionLite &HcclCommTaskExceptionLite::GetInstance()
{
    static HcclCommTaskExceptionLite instance; // aicpu侧一个dev一个进程，不需要按dev区分单例对象
    return instance;
}

HcclCommTaskExceptionLite::HcclCommTaskExceptionLite()
{

}

HcclCommTaskExceptionLite::~HcclCommTaskExceptionLite()
{
    initFlag_ = false;
}

HcclResult HcclCommTaskExceptionLite::Init(u32 deviceId)
{
    CHK_PRT_RET(initFlag_ == true, HCCL_DEBUG("%s has been initialized", __func__), HCCL_SUCCESS);
    initFlag_ = true;
    deviceId_ = deviceId;
    HCCL_INFO("%s success, deviceId[%u]", __func__, deviceId_);
    return HCCL_SUCCESS;
}

void HcclCommTaskExceptionLite::Call()
{
    if (initFlag_ == true || stopCall_ == true) {
        return;
    }

    HcclResult ret = HandleExceptionCqe();
    if (ret != HCCL_SUCCESS) {
        stopCall_ = true;
        HCCL_ERROR("HandleExceptionCqe fail, set stopCall_[%d]", stopCall_); // 函数调用失败，停止调用避免刷屏
    }
}

HcclResult HcclCommTaskExceptionLite::HandleExceptionCqe()
{
    std::vector<std::pair<std::string, CollCommAicpuMgr *>> aicpuCommInfo;
    CHK_RET(AicpuIndopProcess::AicpuGetCommAll(aicpuCommInfo));

    for (auto &commInfo : aicpuCommInfo) {
        CollCommAicpu *aicpuComm = commInfo.second->GetCollCommAicpu();
        CHK_PTR_NULL(aicpuComm);

        std::vector<std::shared_ptr<Thread>> threads;
        aicpuComm->GetAllThread(threads);

        for (auto thread : threads) {
            rtLogicCqReport_t cqeException;
            CqeStatus cqeStatus = CqeStatus::kDefault;
            CHK_RET(GetThreadCqe(thread.get(), cqeException, cqeStatus));
            if (cqeStatus != dfx::CqeStatus::kDefault) {
                CHK_RET(ProcessCqe(aicpuComm, &cqeException));
            }
        }
        
        
    }
}

HcclResult HcclCommTaskExceptionLite::GetThreadCqe(Thread* thread, rtLogicCqReport_t &cqeException, CqeStatus &cqeStatus)
{
    CHK_SMART_PTR_NULL(thread);
    Hccl::StreamLite *streamLite = static_cast<Hccl::StreamLite *>(thread->GetStreamLitePtr());
    CHK_PTR_NULL(streamLite);

    constexpr u32 reportSize = MAX_REPORT_CNT;
    rtLogicCqReport_t streamReport[reportSize];
    
    CqeQueryInput cqeQueryInput;
    cqeQueryInput.devId = deviceId_;
    cqeQueryInput.streamId = streamLite->GetId();
    cqeQueryInput.sqId = streamLite->GetSqId();
    cqeQueryInput.cqId = streamLite->GetCqId();
    cqeQueryInput.type = static_cast<uint32_t>(DRV_LOGIC_TYPE);
    cqeQueryInput.cqeAddr = reinterpret_cast<uint8_t *>(streamReport);
    
    cqeStatus = CqReportRecv(cqeQueryInput, cqeException);
    cqeStatus == dfx::CqeStatus::kCqeException;
    return HCCL_SUCCESS;
}

HcclResult HcclCommTaskExceptionLite::ProcessCqe(CollCommAicpu *aicpuComm, rtLogicCqReport_t* exceptionInfo)
{
    CHK_PTR_NULL(aicpuComm);
    CHK_PTR_NULL(exceptionInfo);

    // exceptionInfo->taskId和exceptionInfo->streamId拼成sqeId
    const u32 sqeId = static_cast<uint32_t>(exceptionInfo->taskId << 16) | static_cast<uint32_t>(exceptionInfo->streamId);
    const auto curTask = Hccl::GlobalMirrorTasks::Instance().GetTaskInfo(0, exceptionInfo->sqId, sqeId);
    if (curTask == nullptr) {
        // 未找到异常对应的TaskInfo
        HCCL_ERROR("Exception task not found. deviceId[%u], streamId(sqId)[%u], taskId(sqeId)[%u].",
            0, exceptionInfo->sqId, sqeId);
        return HCCL_E_PARA;
    }

    // 每个通信域仅首次上报（N秒快恢时重置）
    if (!aicpuComm->IsErrorReported()) {
        // 1) errorMessage上报
        // ErrorMessageReport errMsgInfo{};
        // auto ret = GenerateErrorMessageReport(aicpuComm, curTask, errMsgInfo);
        // if (ret != HCCL_SUCCESS) {
        //     THROW<InvalidParamsException>("GenerateErrorMessageReport failed.");
        // }
        // ret = SendErrorMessageReportToHost(aicpuComm, errMsgInfo);
        // if (ret != HCCL_SUCCESS) {
        //     THROW<InvalidParamsException>("SendErrorMessageReportToHost Failed.");
        // }

        // // 2) send mbox to tsfw
        // CHK_RET_THROW(InternalException,
        //     StringFormat("[TaskExceptionHandlerLite][SendTaskExceptionByMBox]group[%s]:"
        //     "Send task exception by mailBox failed, streamId[%d]",
        //     aicpuComm->GetId().c_str(), exceptionInfo->streamId), SendTaskExceptionByMBox(aicpuComm, exceptionInfo));

        aicpuComm->SetErrorReported(true);
    }

    // HCCL_ERROR("[TaskExceptionHandlerLite][%s]Task from HCCL run failed.", __func__);
    // if (curTask->taskParam_.taskType == TaskParamType::TASK_NOTIFY_WAIT) {
    //     PrintTaskContextInfo(exceptionInfo->sqId, sqeId);
    // }
    // HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, base information is %s.", curTask->GetBaseInfo().c_str());
    // HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, para information is %s.", curTask->GetParaInfo().c_str());
    // HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, groupRank information is %s.",
    //     GetGroupRankInfo(*curTask).c_str());
    // HCCL_ERROR("[TaskExceptionHandlerLite]Task run failed, opData information is %s.", curTask->GetOpInfo().c_str());
}
}