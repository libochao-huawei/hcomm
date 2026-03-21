/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCL_COMM_TASKEXCEPTION_H
#define HCCL_COMM_TASKEXCEPTION_H

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

namespace hcomm {
using RdmaHandle = void*;
using GetAicpuTaskExceptionCallBackHcomm = std::function<Hccl::ErrorMessageReport()>; 

struct CcuHostParam {
    CcuErrorInfo ccuErrorInfo;
    TaskInfo taskInfo;
    uint32_t deviceId;
};
constexpr u32 CCU_COSTOM_ARGS_LEN = 32;
struct ParaCcu {
    u8  dieId;
    u8  missionId;
    u8  execMissionId;
    u32 instrId;
    u64 costumArgs[CCU_COSTOM_ARGS_LEN];
    u64 executeId;
    u64 ccuKernelHandle{0};
};
class CcuTaskException {
public:
    CcuTaskException() = default;
    ~CcuTaskException();

    HcclResult        Register() ;                                // 向rts注册异常处理方法
    HcclResult        UnRegister() ;                              // 向rts注销异常处理方法

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
    void GetCcuErrorMsg(int32_t deviceId, uint16_t missionStatus, const ParaCcu &ccuTaskParam,
    std::vector<CcuErrorInfo> &errorInfo)


private:
    bool isRegistered_ {false};
};

class TaskExceptionHostManager {
public:
    // 获取指定位置的异常处理器
    static TaskExceptionHost *GetHandler(size_t devId);
    static void RegisterGetAicpuTaskExceptionCallBack(s32 streamId, u32 deviceLogicId, GetAicpuTaskExceptionCallBackHcomm p1);

private:
    TaskExceptionHostManager();
    // 私有析构函数，负责释放数组中所有单例实例的内存
    ~TaskExceptionHostManager();
    // 私有拷贝构造函数和赋值运算符，防止对象被拷贝
    TaskExceptionHostManager(const TaskExceptionHostManager &)            = delete;
    TaskExceptionHostManager &operator=(const TaskExceptionHostManager &) = delete;
};
} // namespace hccl

#endif // HCCL_TASK_EXCEPTION_HANDLER_H