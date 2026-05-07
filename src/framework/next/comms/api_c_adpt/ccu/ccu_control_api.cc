/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_control_api.h"

#include <vector>

#include "ccu_primitives.h"

#include "ccu_log.h"
#include "ccu_types.h"

#include "hcom_common.h"
#include "exception_handler.h"

#include "ccu_kernel_mgr.h"
#include "ccu_instance_mgr.h"

#include "thread.h"
#include "rt_external.h"

#include "env_config/env_config.h" // 暂时引用orion的环境变量处理模块

#include "ccu_common.h"

CcuResult HcommCcuInsCreate(const void *resDesc, uint32_t descNum, CcuInsHandle *insHandle)
{
    CCU_CHK_PTR_NULL(resDesc);
    CCU_CHK_PTR_NULL(insHandle);
    if (descNum != 1) {
        HCCL_ERROR("[%s] failed, desc num[%u] is more than expected[1].",
            __func__, descNum);
        return CcuResult::CCU_E_PARA;
    }

    auto *resDescPtr = static_cast<const CcuResDesc *>(resDesc);
    if (resDescPtr->dieId != hcomm::CCU_MAX_IODIE_NUM) {
        HCCL_ERROR("[%s] failed, ccu instance cannot be created with die id now.", __func__);
        return CcuResult::CCU_E_PARA;
    }

    const uint32_t devLogicId = HcclGetThreadDeviceId();
    CCU_CHK_RET(hcomm::CcuInstanceMgr::GetInstance(devLogicId).Create(resDescPtr->insType, *insHandle));

    return CcuResult::CCU_SUCCESS;
}

/**
 * @brief 关闭CCU特性，解初始化CCU平台层
 *
 * @param deviceLogicId 设备逻辑ID
 * @return HcclResult 返回HcclResult类型的结果
 * @note 资源不足时返回HCCL_E_UNAVIL，其余非HCCL_SUCCESS结果属于错误
 */
CcuResult HcommCcuInsDestroy(CcuInsHandle insHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    CCU_CHK_RET(hcomm::CcuInstanceMgr::GetInstance(devLogicId).Destroy(insHandle));

    return CcuResult::CCU_SUCCESS;
}

CcuResult HcommCcuKernelRegisterStart(CcuInsHandle insHandle)
{
    // todo: 需要考虑怎么拦截start调用之前没有end
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto *ccuIns = hcomm::CcuInstanceMgr::GetInstance(devLogicId).Get(insHandle);
    CCU_CHK_PTR_NULL(ccuIns);

    CCU_CHK_RET(ccuIns->Reset());
    return CcuResult::CCU_SUCCESS;
}

CcuResult HcommCcuKernelRegister(CcuInsHandle insHandle,
    const char *kernelFuncName, const void *kernelFunc,
    const void *kernelArg, CcuKernelHandle *kernelHandle)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    HcclUs startut = TIME_NOW();

    CCU_CHK_PTR_NULL(kernelFuncName);
    CCU_CHK_PTR_NULL(kernelFunc);
    CCU_CHK_PTR_NULL(kernelArg);
    CCU_CHK_PTR_NULL(kernelHandle);

    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto *ccuIns = hcomm::CcuInstanceMgr::GetInstance(devLogicId).Get(insHandle);
    CCU_CHK_PTR_NULL(ccuIns);

    auto *resPack = ccuIns->GetResPack();
    CCU_CHK_PTR_NULL(resPack);

    auto ccuKernelFunc = reinterpret_cast<CcuKernelFunc>(kernelFunc);
    const auto ccuKernelArg = const_cast<CcuKernelArg>(kernelArg);

    CcuKernelHandle newHandle{0};
    CCU_EXCEPTION_HANDLE_BEGIN
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    CCU_CHK_RET(kernelMgr.Register(*resPack, kernelFuncName,
        ccuKernelFunc, ccuKernelArg, newHandle));
    CCU_CHK_RET(ccuIns->SaveKernel(newHandle));
    CCU_EXCEPTION_HANDLE_END

    *kernelHandle = newHandle;
    HCCL_INFO("[%s] success, take time [%lld]us.",
        __func__, DURATION_US(TIME_NOW() - startut));
    return CcuResult::CCU_SUCCESS;
}

CcuResult HcommCcuKernelRegisterEnd(CcuInsHandle insHandle)
{
    // todo: 需要拦截是否start
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto *ccuIns = hcomm::CcuInstanceMgr::GetInstance(devLogicId).Get(insHandle);
    CCU_CHK_PTR_NULL(ccuIns);
    const auto &newKernels = ccuIns->GetUntranslatedKernels();

    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    // 当前翻译内部流程可能抛异常
    CCU_EXCEPTION_HANDLE_BEGIN
    CCU_CHK_RET(kernelMgr.Translate(newKernels));
    CCU_EXCEPTION_HANDLE_END

    return CcuResult::CCU_SUCCESS;
}

static HcclResult LaunchCcuTasks(const std::vector<hcomm::CcuTaskParam> &params,
    const aclrtStream stream)
{
    const uint32_t execTimeOutSec = Hccl::EnvConfig::GetInstance().GetRtsConfig().GetExecTimeOut();
    for (auto it = params.begin(); it != params.end(); ++it) {
        rtCcuTaskInfo_t taskInfo{};
        taskInfo.dieId       = it->dieId;
        taskInfo.missionId   = it->missionId;
        taskInfo.instStartId = it->instStartId;
        taskInfo.instCnt     = it->instCnt;
        taskInfo.key         = it->key;
        taskInfo.argSize     = it->argSize;
        taskInfo.timeout     = execTimeOutSec;
        std::copy(std::begin(it->args), std::end(it->args), std::begin(taskInfo.args));
        
        HCCL_INFO("[%s] start ccu task, dieId[%u] missionId[%u] instStartId[%u] instCnt[%u], "
            "argSize[%u], timeout[%u]s", __func__, taskInfo.dieId, taskInfo.missionId,
            taskInfo.instStartId, taskInfo.instCnt, taskInfo.argSize, taskInfo.timeout);
 
        for (std::size_t i = 0; i < taskInfo.argSize; i++) { // args 大小为 13
            constexpr std::size_t TOKEN_VALUE_INDEX = 2; // 与算法约束token index为 2
            if (i == TOKEN_VALUE_INDEX) { continue; }
            HCCL_INFO("[%s] arg[%lu] = %lu", __func__, i, taskInfo.args[i]);
        }

        auto ret = rtCCULaunch(&taskInfo, stream);
        if (ret != RT_ERROR_NONE) {
            HCCL_ERROR("[%s] failed to launch ccu, ret[%d]", __func__, ret);
            return HcclResult::HCCL_E_RUNTIME;
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

CcuResult HcommCcuKernelLaunch(ThreadHandle threadHandle,
    CcuKernelHandle kernelHandle, const void *taskArgs, uint32_t argSize)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    const auto &startus = TIME_NOW();

    CHK_PRT_RET(threadHandle == 0,
        HCCL_ERROR("[%s] failed, thread handle is empty.", __func__),
        CcuResult::CCU_E_PARA);

    CHK_PRT_RET(kernelHandle == 0,
        HCCL_ERROR("[%s] failed, kernel handle is empty.", __func__),
        CcuResult::CCU_E_PARA);

    const auto *rtsThread = reinterpret_cast<hccl::Thread *>(threadHandle);
    const auto *threadStream = rtsThread->GetStream();
    CCU_CHK_PTR_NULL(threadStream);
    auto *streamPtr = threadStream->ptr();
    CCU_CHK_PTR_NULL(streamPtr);

    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    auto *kernel = kernelMgr.GetKernel(kernelHandle);
    CCU_CHK_PTR_NULL(kernel);

    CCU_EXCEPTION_HANDLE_BEGIN
    std::vector<hcomm::CcuTaskParam> taskParams{};
    // todo: 需要处理dfx功能
    auto ret = kernel->GeneTaskParams(static_cast<const uint64_t *>(taskArgs), argSize, taskParams);
    CHK_PRT_RET(ret != CcuResult::CCU_SUCCESS,
        HCCL_ERROR("[%s] failed, threadHandle[0x%llx] kernelHandle[0x%llx].",
            __func__, threadHandle, kernelHandle),
        ret);

    if (taskParams.empty()) {
        HCCL_INFO("[%s] passed, ccu params are empty.", __func__);
        return CcuResult::CCU_SUCCESS;
    }

    CCU_CHK_RET(LaunchCcuTasks(taskParams, streamPtr));
    CCU_EXCEPTION_HANDLE_END
    HCCL_INFO("[%s] success, take time [%lld]us.",
        __func__, DURATION_US(TIME_NOW() - startus));
    return CcuResult::CCU_SUCCESS;
}