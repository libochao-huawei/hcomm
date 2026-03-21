/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CCU_TASKEXCEPTION_H
#define CCU_TASKEXCEPTION_H

#include <array>
#include "types.h"
#include "hccl_types.h"
#include "orion_adapter_rts.h"
#include "global_mirror_tasks.h"
#include "error_message_v2.h"
#include "orion_adapter_hccp.h"
#include "rdma_handle_manager.h"
#include "ccu_error_info.h"
#include "rank_pair.h"
#include "ccu_error_info_v1.h"

namespace hcomm {
using RdmaHandle = void*;

struct CcuHostParam {
    CcuErrorInfo ccuErrorInfo;
    Hccl::TaskInfo taskInfo;
    uint32_t deviceId;
};

class CcuTaskException {
public:
    CcuTaskException() = default;
    ~CcuTaskException() = default;

private:
    static std::string GetGroupRankInfo(const Hccl::TaskInfo& taskInfo);
    static void ProcessException(rtExceptionInfo_t* exceptionInfo, const Hccl::TaskInfo& taskInfo);
    static void PrintTaskContextInfo(uint32_t deviceId, uint32_t streamId, uint32_t taskId);

    static void PrintGroupErrorMessage(Hccl::ErrorMessageReport &errorMessage, Hccl::TaskInfo &exceptionTaskInfo, std::string &groupRankContent, std::string &stageErrInfo);
    static void PrintOpDataErrorMessage(u32 deviceId, Hccl::ErrorMessageReport &errorMessage, std::string &stageErrInfo);
    static HcclResult PrintUbRegisters(s32 devLogicId, RdmaHandle rdmaHandle);
    static HcclResult PrintCcuUbRegisters(s32 devLogicId, const Hccl::ParaCcu &ccuTaskParam);

    static void ProcessCcuException(const rtExceptionInfo_t* exceptionInfo, const Hccl::TaskInfo& taskInfo);
 	static void PrintCcuErrorInfo(uint32_t deviceId, uint16_t status, const Hccl::TaskInfo& taskInfo);
    static void PrintCcuErrorLog(const std::vector<CcuErrorInfo>& errorInfos, const Hccl::TaskInfo& taskInfo, u32 deviceId);

    static std::string GetCcuErrorMsgByType(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgLoop(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgMission(const CcuErrorInfo& ccuErrorInfo);
    static std::string GetCcuErrorMsgDefault(const CcuErrorInfo& ccuErrorInfo);
    static std::string GetCcuErrorMsgLoopGroup(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgLocPostSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgLocWaitSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgRemPostSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgRemWaitSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgRemPostVar(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgRemWaitGroup(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgPostSharedSem(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgRead(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgWrite(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgLocalCpy(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgLocalReduce(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgBufRead(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgBufWrite(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgBufLocRead(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgBufLocWrite(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuErrorMsgBufReduce(const CcuErrorInfo &ccuErrorInfo, const Hccl::TaskInfo &taskInfo, u32 deviceId);

    static HcclResult GetCcuChannelHandleById(u32 deviceId, u16 channelId, const Hccl::TaskInfo &taskInfo, u64& channelHandle);
    static RankId GetRankIdByChannelId(uint16_t channelId, const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::pair<Hccl::IpAddress, Hccl::IpAddress> GetAddrPairByChannelId(uint16_t channelId,
        const Hccl::TaskInfo &taskInfo, u32 deviceId);
    static std::string GetCcuLenErrorMsg(const uint64_t len);
    void GetCcuErrorMsg(int32_t deviceId, uint16_t missionStatus, const Hccl::ParaCcu &ccuTaskParam,
        std::vector<CcuErrorInfo> &errorInfo);
    static void PrintPanicLogInfo(const uint8_t *panicLog);
};
} // namespace hcomm

#endif