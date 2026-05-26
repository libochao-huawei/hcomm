/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCL_COMM_TASKEXCEPTION_LIFT_H
#define HCCL_COMM_TASKEXCEPTION_LIFT_H

#include "daemon_func.h"
#include "mirror_task_manager.h"
#include "coll_comm_aicpu.h"
#include "aicpu_hccl_sqcq.h"
#include "error_message_v2.h"

namespace hcomm {

class HcclCommTaskExceptionLite : public Hccl::DaemonFunc {
public:
    static HcclCommTaskExceptionLite &GetInstance();
    void Init(u32 devId);
    void Call() override;

private:
    HcclCommTaskExceptionLite() = default;
    ~HcclCommTaskExceptionLite() = default;

    HcclResult ProcessCqe(CollCommAicpu *aicpuComm, const rtLogicCqReport_t &exceptionInfo);
    HcclResult ReportErrMsg(CollCommAicpu *aicpuComm, const rtLogicCqReport_t &exceptionInfo);
    HcclResult PrintTaskException(CollCommAicpu *aicpuComm, u32 sqId, uint16_t taskId, uint16_t streamId);
    HcclResult PrintThreadTaskInfo(CollCommAicpu *aicpuComm, u32 sqId, uint16_t taskId, uint16_t streamId);

    HcclResult HandleExceptionCqe();
    HcclResult GetThreadCqe(hccl::Thread* thread, rtLogicCqReport_t &cqeException, CqeStatus &cqeStatus);
    HcclResult GenerateErrorMessageReport(CollCommAicpu *aicpuComm, const Hccl::TaskInfo& taskInfo,
        const rtLogicCqReport_t &exceptionInfo, Hccl::ErrorMessageReport &errMsgInfo);
    void GetErrMsgInfo(const Hccl::TaskInfo& taskInfo, Hccl::ErrorMessageReport &errMsgInfo,
        const rtLogicCqReport_t &exceptionInfo);
    HcclResult SendTaskExceptionByMBox(const u32 notifyId, const u32 tsId, const rtLogicCqReport_t &exceptionInfo);
    uint16_t SwitchUBCqeErrCodeToTsErrCode(u32 cqeErrCode);
    uint16_t SwitchSdmaCqeErrCodeToTsErrCode(u32 cqeErrCode);
    HcclResult PrintTaskContextInfo(CollCommAicpu *aicpuComm, u32 sqId, u32 taskId);
    std::string GetGroupInfo(const Hccl::TaskInfo& taskInfo);
    u32 GetSqeId(uint16_t taskId, uint16_t streamId);

    void PrintEid(const Hccl::TaskInfo& taskInfo);

    bool stopCall_{false};
    u32 devId_{INVALID_UINT};
    bool printAllThreads_{false};
    std::unordered_map<u32, u32> threadsPrinted_; // sqId -> sqeId, 记录已经打印过taskException的流信息
};

}

#endif