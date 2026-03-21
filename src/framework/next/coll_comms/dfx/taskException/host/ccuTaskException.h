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
    static void PrintCcuErrorLog(const std::vector<Hccl::CcuErrorInfo>& errorInfos, const Hccl::TaskInfo& taskInfo);

    static std::string GetCcuErrorMsgByType(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgLoop(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgMission(const Hccl::CcuErrorInfo& ccuErrorInfo);
    static std::string GetCcuErrorMsgDefault(const Hccl::CcuErrorInfo& ccuErrorInfo);
    static std::string GetCcuErrorMsgLoopGroup(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgLocPostSem(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgLocWaitSem(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgRemPostSem(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgRemWaitSem(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgRemPostVar(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgRemWaitGroup(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgPostSharedSem(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgRead(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgWrite(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgLocalCpy(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgLocalReduce(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgBufRead(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgBufWrite(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgBufLocRead(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgBufLocWrite(const CcuHostParam &ccuHostParam);
    static std::string GetCcuErrorMsgBufReduce(const CcuHostParam &ccuHostParam);
    static RankId GetRankIdByChannelId(const CcuHostParam &ccuHostParam);
    static std::pair<Hccl::IpAddress, Hccl::IpAddress> GetAddrPairByChannelId(const CcuHostParam &ccuHostParam);
    static std::string GetCcuLenErrorMsg(const uint64_t len);
    void GenErrorInfoLoopGroup(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                            CcuRepContext &ctx, std::vector<CcuErrorInfo> &errorInfo);
    void GenErrorInfoByRepType(const ErrorInfoBase &baseInfo, shared_ptr<CcuRepBase> repBase,
                                std::vector<CcuErrorInfo> &errorInfo);
    void GenStatusInfo(const ErrorInfoBase &baseInfo, std::vector<CcuErrorInfo> &errorInfo);
    CcuMissionContext GetCcuMissionContext(int32_t deviceId, uint32_t dieId, uint32_t missionId);
    void GetCcuErrorMsg(int32_t deviceId, uint16_t missionStatus, const Hccl::ParaCcu &ccuTaskParam,
        std::vector<CcuErrorInfo> &errorInfo);
    static void PrintPanicLogInfo(const uint8_t *panicLog);
};
} // namespace hcomm

#endif