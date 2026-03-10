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
#include "hccl_common.h"
#include "error_message_v2.h"
#include "orion_adapter_hccp.h"
#include "rdma_handle_manager.h"


namespace hcomm {
using RdmaHandle = void*;
using GetAicpuTaskExceptionCallBackHcomm = std::function<Hccl::ErrorMessageReport()>; 
class TaskExceptionHost {
public:
    // 构造函数使用初始化列表初始化devId_
    explicit TaskExceptionHost(int deviceId);
    ~TaskExceptionHost();

    // 获取设备ID
    void        Register() const;                                // 向rts注册异常处理方法
    void        UnRegister() const;                              // 向rts注销异常处理方法
    static void Process(rtExceptionInfo_t *exceptionInfo); // 处理异常信息
    static void PrintAicpuErrorMessage(rtExceptionInfo_t *exceptionInfo);

private:
    static std::string GetGroupRankInfo(const Hccl::TaskInfo& taskInfo);
    static void ProcessException(rtExceptionInfo_t* exceptionInfo, const Hccl::TaskInfo& taskInfo);
    static void PrintTaskContextInfo(uint32_t deviceId, uint32_t streamId, uint32_t taskId);

    static void PrintGroupErrorMessage(Hccl::ErrorMessageReport &errorMessage, Hccl::TaskInfo &exceptionTaskInfo, std::string &groupRankContent, std::string &stageErrInfo);
    static void PrintOpDataErrorMessage(u32 deviceId, Hccl::ErrorMessageReport &errorMessage, std::string &stageErrInfo);

private:
    uint32_t devId_; // 当前设备id
};

class TaskExceptionHostManager {
public:
    // 获取指定位置的异常处理器
    static TaskExceptionHost *GetHandler(size_t devId);

private:
    TaskExceptionHostManager();
    // 私有析构函数，负责释放数组中所有单例实例的内存
    ~TaskExceptionHostManager();
    // 私有拷贝构造函数和赋值运算符，防止对象被拷贝
    TaskExceptionHostManager(const TaskExceptionHostManager &)            = delete;
    TaskExceptionHostManager &operator=(const TaskExceptionHostManager &) = delete;

private:
    // 全局静态数组，存储异常处理器指针
    static std::array<TaskExceptionHost *, MAX_MODULE_DEVICE_NUM> handlers_;
};

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
extern void RegisterGetAicpuTaskExceptionCallBackV2(s32 streamId, u32 deviceLogicId, GetAicpuTaskExceptionCallBackHcomm p1);
#ifdef __cplusplus
}
#endif // __cplusplus

const std::string LOG_KEYWORDS_TIMEOUT = "Timeout";                       // 算子执行阶段超时
const std::string LOG_KEYWORDS_RUN_FAILED = "RunFailed";                  // 算子执行阶段失败，如SDMA ERROR
const std::string LOG_KEYWORDS_TASK_EXEC = "TaskExecStage";               // 算子执行阶段异常
const std::string LOG_KEYWORDS_AICPU = "AICPU";
} // namespace Hccl

#endif // HCCL_TASK_EXCEPTION_HANDLER_H