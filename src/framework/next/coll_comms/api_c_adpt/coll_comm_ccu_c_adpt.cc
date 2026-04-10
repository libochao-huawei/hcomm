/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_control_api.h"

#include "hccl_comm_pub.h"

#include "env_config.h"
#include "exception_handler.h"

/**
 * @note 职责：集合通信的通信域CCU管理的C接口的C到C++适配
 */
HcclResult HcclCommQueryCcuIns(HcclComm comm,
    CcuInsHandle *ccuInsHandles, uint32_t *insNum)
{
    EXCEPTION_HANDLE_BEGIN

    HcclUs startut = TIME_NOW();

    CHK_PTR_NULL(comm);
    auto *hcclComm = static_cast<hccl::hcclComm *>(comm);
    const auto &commId = hcclComm->GetIdentifier();
    HCCL_INFO("[%s] CommId[%s] query ccu instance.", __func__, commId.c_str());

    CHK_PTR_NULL(ccuInsHandles);
    CHK_PTR_NULL(insNum);

     // CCU只支持A5代际
    if (!hcclComm->IsCommunicatorV2()) {
        HCCL_WARNING("[%s] is not supported.", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    auto *collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    auto *myRank = collComm->GetMyRank();
    CHK_PTR_NULL(myRank);

    // 非CCU通信域允许查询，不认为是错误
    auto ccuInsHandle = myRank->GetCcuInstance();
    if (ccuInsHandle == 0) {
        auto opExpansionMode = myRank->GetOpExpansionMode();
        HCCL_WARNING("[%s] failed to get ccu instance, commId[%s] op expansion mode[%d].",
            __func__, commId.c_str(), opExpansionMode);
        return HcclResult::HCCL_E_UNAVAIL;
    }

    ccuInsHandles[0] = ccuInsHandle;
    *insNum = 1;
    HCCL_INFO("[%s] success, take time [%lld]us.",
        __func__, DURATION_US(TIME_NOW() - startut));
    
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

// static HcclResult LaunchCcuTasks(const std::vector<hcomm::CcuTaskParam> &params, const aclrtStream stream, Hccl::TaskParam &taskParam)
// {
//     taskParam.beginTime = Hccl::DlProfFunction::GetInstance().dlMsprofSysCycleTime();
//     constexpr uint32_t defaultTimeOutSec = 120; // 当前未支持从环境变量配置
//     for (auto it = params.begin(); it != params.end(); ++it) {
//         rtCcuTaskInfo_t taskInfo{};
//         taskInfo.dieId       = it->dieId;
//         taskInfo.missionId   = it->missionId;
//         taskInfo.instStartId = it->instStartId;
//         taskInfo.instCnt     = it->instCnt;
//         taskInfo.key         = it->key;
//         taskInfo.argSize     = it->argSize;
//         taskInfo.timeout     = defaultTimeOutSec;
//         std::copy(std::begin(it->args), std::end(it->args), std::begin(taskInfo.args));
        
//         HCCL_INFO("[%s] start ccu task, dieId[%u] missionId[%u] instStartId[%u] instCnt[%u], "
//             "argSize[%u], timeout[%u]s", __func__, taskInfo.dieId, taskInfo.missionId,
//             taskInfo.instStartId, taskInfo.instCnt, taskInfo.argSize, taskInfo.timeout);
 
//         for (std::size_t i = 0; i < taskInfo.argSize; i++) { // args 大小为 13
//             constexpr std::size_t TOKEN_VALUE_INDEX = 2; // 与算法约束token index为 2
//             if (i == TOKEN_VALUE_INDEX) { continue; }
//             HCCL_INFO("[%s] arg[%lu] = %lu", __func__, i, taskInfo.args[i]);
//             taskParam.taskPara.Ccu.costumArgs[i] = taskInfo.args[i];
//         }

//         auto ret = rtCCULaunch(&taskInfo, stream);
//         if (ret != RT_ERROR_NONE) {
//             HCCL_ERROR("[%s] failed to launch ccu, ret[%d]", __func__, ret);
//             return HcclResult::HCCL_E_RUNTIME;
//         }
//     }
//     taskParam.endTime = Hccl::DlProfFunction::GetInstance().dlMsprofSysCycleTime();

//     return HcclResult::HCCL_SUCCESS;
// }

// HcclResult SaveDfxTaskInfo(const HcclComm comm, const Hccl::TaskParam &taskParam)
// {
//     uint32_t taskId = INVALID_UINT;
//     uint32_t streamId = INVALID_UINT;
//     CHK_RET(hrtGetTaskIdAndStreamID(taskId, streamId));

//     CHK_PRT_RET(comm == nullptr, HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
//     auto hcclComm = static_cast<hccl::hcclComm*>(comm);
//     CHK_PTR_NULL(hcclComm);
//     if (!hcclComm->IsCommunicatorV2()) {
//         HCCL_ERROR("[%s] comm is NOT_SUPPORT", __func__);
//         return HCCL_E_NOT_SUPPORT;
//     }
//     hccl::CollComm* collComm = hcclComm->GetCollComm();
//     CHK_PTR_NULL(collComm);
//     hccl::HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
//     CHK_PTR_NULL(hcclCommDfx);

//     CHK_RET(hcclCommDfx->AddTaskInfoCallback(streamId, taskId, taskParam, INVALID_U64));
//     return HCCL_SUCCESS;
// }

// HcclResult HcclReportCcuProfilingInfo(const ThreadHandle threadHandle, uint64_t execId, void *streamProfilingInfos, size_t infoNum,
//                                         const HcclComm comm, Hccl::TaskParam &taskParam, bool isMaster)
// {
//     if (infoNum == 0) {
//         HCCL_INFO("There is no ccu profiling info.");
//         return HCCL_SUCCESS;
//     }
//     CHK_PTR_NULL(streamProfilingInfos);
//     CHK_PRT_RET(comm == nullptr, HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
//     auto hcclComm = static_cast<hccl::hcclComm*>(comm);
//     CHK_PTR_NULL(hcclComm);

//     // 将 void* 转换为 CcuProfilingInfo 数组指针
//     hcomm::CcuProfilingInfo* profilingArray = reinterpret_cast<hcomm::CcuProfilingInfo*>(streamProfilingInfos);
    
//     // 设置任务参数的基本信息
//     taskParam.taskPara.Ccu.dieId     = profilingArray[0].dieId;
//     taskParam.taskPara.Ccu.missionId = profilingArray[0].missionId;
//     taskParam.taskPara.Ccu.execMissionId = profilingArray[0].missionId;
//     taskParam.taskPara.Ccu.instrId   = profilingArray[0].instrId;
//     taskParam.taskPara.Ccu.executeId = execId; // TODO: 传入是kernelHandle，不建议赋值给executeId
//     taskParam.taskPara.Ccu.ccuKernelHandle = execId;
//     taskParam.isMaster = isMaster;
//     HCCL_INFO("[%s]dieId[%u], missionId[%u], execMissionId[%u], instrId[%u], executeId[%u], ccuKernelHandle[%u]",
//         __func__, taskParam.taskPara.Ccu.dieId, taskParam.taskPara.Ccu.missionId, taskParam.taskPara.Ccu.execMissionId,
//         taskParam.taskPara.Ccu.instrId, taskParam.taskPara.Ccu.executeId, taskParam.taskPara.Ccu.ccuKernelHandle);

//     // 处理每个性能信息条目
//     for (size_t i = 0; i < infoNum; ++i) {
//         hcomm::CcuProfilingInfo& profInfo = profilingArray[i];
//         for (int idx = 0; idx < hcomm::CCU_MAX_CHANNEL_NUM; idx++) {
//             if (profInfo.channelId[idx] == hcomm::INVALID_VALUE_CHANNELID) {
//                 break;
//             }
//             // 获取 remoteRankId
//             u32 remoteRankId = hcomm::INVALID_RANKID;
//             HcclResult ret = hccl::HcclCommDfx::GetChannelRemoteRankId(
//                 hcclComm->GetIdentifier(), profInfo.channelHandle[idx], remoteRankId);
//             if (ret != HCCL_SUCCESS) {
//                 HCCL_ERROR("[%s] Failed to get remote rank for channelHandle[0x%llx], using default 0",
//                     __func__, profInfo.channelHandle[idx]);
//                 return HCCL_E_PARA;
//             }
//             profInfo.remoteRankId[idx] = remoteRankId;
//             HCCL_INFO("[%s]idx[%u]: channelId[%u], remoteRankId[%u], channelHandle[0x%llx]",
//                 __func__, idx, profInfo.channelId[idx], profInfo.remoteRankId[idx], profInfo.channelHandle[idx]);
//         }
//     }
    
//     // 转换函数：将 hcomm::CcuProfilingInfo 转换为 Hccl::CcuProfilingInfo
//     auto convertToHccl = [](const hcomm::CcuProfilingInfo& src) -> Hccl::CcuProfilingInfo {
//         Hccl::CcuProfilingInfo dst;
//         dst.name = src.name;
//         dst.type = src.type;
//         dst.dieId = src.dieId;
//         dst.missionId = src.missionId;
//         dst.instrId = src.instrId;
//         dst.reduceOpType = src.reduceOpType;
//         dst.inputDataType = src.inputDataType;
//         dst.outputDataType = src.outputDataType;
//         dst.dataSize = src.dataSize;
//         dst.ckeId = src.ckeId;
//         dst.mask = src.mask;
//         HCCL_INFO("src.name %s, dst.name %s", src.name.c_str(), dst.name.c_str());
//         (void)memcpy_s(dst.channelId, sizeof(dst.channelId), src.channelId, sizeof(src.channelId));
//         (void)memcpy_s(dst.remoteRankId, sizeof(dst.remoteRankId), src.remoteRankId, sizeof(src.remoteRankId));
//         return dst;
//     };

//     // 转换所有性能信息
//     std::vector<Hccl::CcuProfilingInfo> converted;
//     converted.reserve(infoNum);

//     for (size_t i = 0; i < infoNum; ++i) {
//         converted.push_back(convertToHccl(profilingArray[i]));
//     }
    
//     // 构建shared_ptr并保存到任务参数
//     taskParam.ccuDetailInfo = std::make_shared<std::vector<Hccl::CcuProfilingInfo>>(std::move(converted));
//     HCCL_DEBUG("[%s]dieId[%u]", __func__, taskParam.taskPara.Ccu.dieId);
//     CHK_RET(SaveDfxTaskInfo(comm, taskParam));
//     return HCCL_SUCCESS;
// }

// HcclResult HcclCcuKernelLaunch(HcclComm comm, const ThreadHandle threadHandle,
//     const CcuKernelHandle kernelHandle, void *taskArgs)
// {
//     // 性能关键路径，禁止打印算子粒度频次的日志
//     (void)comm;
//     CHK_PTR_NULL(taskArgs);
//     CHK_PRT_RET(threadHandle == 0, HCCL_ERROR("[%s] failed, thread handle is empty.", __func__), HCCL_E_PARA);

//     const Thread *rtsThread = reinterpret_cast<Thread *>(threadHandle); 
//     const auto *threadStream = rtsThread->GetStream();
//     CHK_PTR_NULL(threadStream);
//     auto *streamPtr = threadStream->ptr();
//     CHK_PTR_NULL(streamPtr);

//     auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(HcclGetThreadDeviceId());
//     auto *kernel = kernelMgr.GetKernel(kernelHandle);
//     CHK_PTR_NULL(kernel);

//     EXCEPTION_HANDLE_BEGIN
//     const hcomm::CcuTaskArg *ccuTaskArgs = reinterpret_cast<hcomm::CcuTaskArg *>(taskArgs);
//     std::vector<hcomm::CcuTaskParam> ccuParams{};
//     // todo: 需要切到新流程
//     // auto ret = kernel->GeneTaskParam(*ccuTaskArgs, ccuParams);
//     // CHK_PRT_RET(ret != HcclResult::HCCL_SUCCESS,
//     //     HCCL_ERROR("[%s] failed, kernleHandle[0x%llx].", __func__, kernelHandle),
//     //     ret);

//     if (ccuParams.empty()) {
//         HCCL_INFO("[%s] passed, ccu params are empty.", __func__);
//         return HcclResult::HCCL_SUCCESS;
//     }
//     std::vector<hcomm::CcuProfilingInfo> allCcuProfilingInfo;
//     CHK_RET(kernel->GetCcuProfilingInfo(*ccuTaskArgs, allCcuProfilingInfo));
//     Hccl::TaskParam taskParam = {
//         .taskType  = Hccl::TaskParamType::TASK_CCU,
//         .beginTime = 0,
//         .endTime   = 0,
//         .isMaster = false,
//         .taskPara  = {
//             .Ccu = {
//                 .dieId         = 0,
//                 .missionId     = 0,
//                 .execMissionId = 0,
//                 .instrId       = 0,
//                 .costumArgs    = {},
//                 .executeId     = 0
//             }
//         },
//         .ccuDetailInfo  = nullptr
//     };
//     CHK_RET(LaunchCcuTasks(ccuParams, streamPtr, taskParam));
//     CHK_RET(HcclReportCcuProfilingInfo(threadHandle, kernelHandle, allCcuProfilingInfo.data(), allCcuProfilingInfo.size(),
//                                         comm, taskParam, rtsThread->GetMaster()));
//     EXCEPTION_HANDLE_END
//     return HcclResult::HCCL_SUCCESS;
// }