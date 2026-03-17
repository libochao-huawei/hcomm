/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_primitives.h"

#include "ccu_log.h"
#include "ccu_types.h"
#include "hcom_common.h"
#include "exception_handler.h"

#include "ccu_kernel_mgr.h"
#include "ccu_instance_mgr.h"

CcuResult HcommCcuInsCreate(void *resDesc, CcuInsHandle *insHandle)
{
    CCU_CHK_PTR_NULL(resDesc);
    CCU_CHK_PTR_NULL(insHandle);

    // 当前该接口未对外提供，内部使用优先基于instance类型
    auto *insType = static_cast<CcuInstanceType *>(resDesc);
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    CCU_CHK_RET(hcomm::CcuInstanceMgr::GetInstance(devLogicId).Create(*insType, *insHandle));

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
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto *ccuIns = hcomm::CcuInstanceMgr::GetInstance(devLogicId).Get(insHandle);
    CCU_CHK_PTR_NULL(ccuIns);

    CCU_CHK_RET(ccuIns->Reset());
    return CcuResult::CCU_SUCCESS;
}

CcuResult HcommCcuKernelRegister(CcuInsHandle insHandle,
    char *kernelFuncName, void *kernelFunc, void *kernelArg,
    CcuKernelHandle *kernelHandle)
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
    auto ccuKernelArg = static_cast<CcuKernelArg>(kernelArg);

    CcuKernelHandle newHandle{0};
    // todo: 需要处理抛异常，当前hccl result与ccu result转换不方便
    // EXCEPTION_HANDLE_BEGIN
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    CCU_CHK_RET(kernelMgr.Register(*resPack, kernelFuncName,
        ccuKernelFunc, ccuKernelArg, newHandle));
    CCU_CHK_RET(ccuIns->SaveKernel(newHandle));
    // EXCEPTION_HANDLE_END

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
    // todo: 需要处理抛异常，当前hccl result与ccu result转换不方便
    // 当前翻译内部流程可能抛异常
    // EXCEPTION_HANDLE_BEGIN
    CCU_CHK_RET(kernelMgr.Translate(newKernels));
    // EXCEPTION_HANDLE_END

    return CcuResult::CCU_SUCCESS;
}