/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcom_common.h"
#include "ccu_kernel.h"
#include "../comms/ccu/ccu_kernel/ccu_kernel_mgr.h"
#include "rt_external.h"
#include "hccl_ccu_res.h"

HcclResult HcclCcuKernelRegisterCStyle(
    HcclComm comm, // comm 调试保留，后续删除
    CcuKernelHandle *kernelHandle, CcuInsHandle ccuInsHandle,
    char *kernelFuncName, void *ccuKernelFunc, void *kernelArg)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    HcclUs startut = TIME_NOW();

    CHK_PTR_NULL(comm); // todo: 后续comm需要去除
    CHK_PTR_NULL(kernelHandle);
    CHK_PTR_NULL(ccuInsHandle);
    CHK_PTR_NULL(kernelFuncName);
    CHK_PTR_NULL(ccuKernelFunc);
    CHK_PTR_NULL(kernelArg);

    // todo: 后续通信域相关资源管理要转为ccu ins handle管理
    auto *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto *collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    auto* myRank = collComm->GetMyRank();
    CHK_PTR_NULL(myRank);

    auto *ccuContainer = myRank->GetCcuResContainer();
    CHK_PTR_NULL(ccuContainer);

    auto *resPack = ccuContainer->GetResPack();
    CHK_PTR_NULL(resPack);

    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    CcuKernelHandle newHandle{0};
    // 当前注册内部流程可能抛异常
    EXCEPTION_HANDLE_BEGIN
    CHK_RET(kernelMgr.RegisterCStyle(*resPack, ccuInsHandle, newHandle));
    EXCEPTION_HANDLE_END
    CHK_RET(ccuContainer->SaveCcuKernel(newHandle));
    *kernelHandle = newHandle;
    HCCL_INFO("[%s] success, take time [%lld]us.",
        __func__, DURATION_US(TIME_NOW() - startut));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult HcclCcuKernelRegisterFinishCStyle(
    HcclComm comm, // todo: 当前调试预留，后续转为ccu ins handle
    CcuInsHandle ccuInsHandle)
{
    HCCL_RUN_INFO("Entry-%s", __func__);
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(ccuInsHandle);

    auto *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto *collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    auto* myRank = collComm->GetMyRank();
    CHK_PTR_NULL(myRank);

    auto *ccuContainer = myRank->GetCcuResContainer();
    CHK_PTR_NULL(ccuContainer);

    const auto &newKernels = ccuContainer->GetUntranslatedKernels();

    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto &kernelMgr = hcomm::CcuKernelMgr::GetInstance(devLogicId);
    // 当前翻译内部流程可能抛异常
    EXCEPTION_HANDLE_BEGIN
    CHK_RET(kernelMgr.TranslateCStyle(newKernels, ccuInsHandle));
    EXCEPTION_HANDLE_END

    CHK_RET(ccuContainer->ResetResPack());
    return HcclResult::HCCL_SUCCESS;
}