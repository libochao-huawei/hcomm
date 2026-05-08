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
#include "coll_comm.h"

namespace hcomm {
using RdmaHandle = void*;
using GetAicpuTaskExceptionCallBackHcomm = std::function<Hccl::ErrorMessageReport()>; 

using GetAicpuCqeErrInfoCallBackHcomm = void (*)(u32 RemoteLocalId, u32 LocDeviceId, uint16_t status, std::string LocalEid, std::string RemoteEid, std::string RemoteInsId); // 获取远端rankId的回调函数类型
void RegisterGetAicpuCqeErrInfoCallBackHcomm(GetAicpuCqeErrInfoCallBackHcomm); // 注册获取远端rankId的回调函数   

using AicpuGetErrStatusVecCallBack = std::vector<std::string> (*)(s32 deviceLogicID);
void RegisterAicpuGetErrStatusVecCallBack(AicpuGetErrStatusVecCallBack);

class TaskExceptionHost {
public:
    TaskExceptionHost() = default;
    ~TaskExceptionHost();

    HcclResult        Register() ;                                // 向rts注册异常处理方法
    HcclResult        UnRegister() ;                              // 向rts注销异常处理方法
    static void Process(rtExceptionInfo_t *exceptionInfo); // 处理异常信息
    static void PrintAicpuErrorMessage(rtExceptionInfo_t *exceptionInfo, const Hccl::TaskInfo& taskInfo);

private:
    static std::string GetGroupRankInfo(const Hccl::TaskInfo& taskInfo);
    static void ProcessException(rtExceptionInfo_t* exceptionInfo, const Hccl::TaskInfo& taskInfo);
    static void PrintTaskContextInfo(uint32_t deviceId, uint32_t streamId, uint32_t taskId);

    static void PrintGroupErrorMessage(Hccl::ErrorMessageReport &errorMessage, Hccl::TaskInfo &exceptionTaskInfo, std::string &groupRankContent, std::string &stageErrInfo);
    static void PrintOpDataErrorMessage(u32 deviceId, Hccl::ErrorMessageReport &errorMessage, std::string &stageErrInfo);
    static HcclResult PrintUbRegisters(s32 devLogicId, RdmaHandle rdmaHandle);
    static void ClusterMoniterGetAicpuCqeErrInfo(u32 RemoteDeviceId, u32 LocDeviceId, uint16_t status, std::string LocalEid, std::string RemoteEid, std::string RemoteInsId);
    static void GetAicpuCqeErrInfo(rtExceptionInfo_t* exceptionInfo, const Hccl::ErrorMessageReport &errorMessage, const Hccl::TaskInfo& taskInfo);
    static void GetAicpuCqeErrRemoteLocalIdByRankId(hccl::CollComm* collComm, uint32_t rankid, u32 &RemoteLocalId);
    static void GetAicpuCqeErrNetInstanceByRankId(hccl::CollComm* collComm, uint32_t rankid, std::string &netInstanceId);
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

#endif