/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_instance.h"

#include "log.h"

#include "hcom_common.h"
#include "exception_handler.h"

#include "ccu_dev_mgr.h"
#include "ccu_res_pack.h"
#include "ccu_kernel_mgr.h"

namespace hcomm {

CcuInstance::~CcuInstance()
{
    // 主动释放资源保证时序，不得随意调整顺序
    for (auto &kernelHandle : kernelHandles_) {
        if (kernelHandle != 0) {
            (void)CcuKernelMgr::GetInstance(devLogicId_).UnRegister(kernelHandle);
            kernelHandle = 0;
        }
    }
    kernelHandles_.clear();

    resPack_ = nullptr; // 释放通信域持有CCU资源
    if (ccuDrvHandle_) {
        ccuDrvHandle_ = nullptr; // 先减少引用计数，再尝试关闭
        (void)CcuDeinitFeature(devLogicId_);
        // 尝试关闭CCU功能，最后一个通信域调用时会关闭CCU驱动
    }
}

HcclResult CcuInstance::Init()
{
    if (insType_ == CcuInstanceType::CCU_INVALID) {
        HCCL_ERROR("[CcuInstance][%s] failed, CcuInstanceType[%d] is invalid.",
            __func__, insType_);
        return HcclResult::HCCL_E_PARA;
    }

    devLogicId_ = HcclGetThreadDeviceId();

    if (!ccuDrvHandle_) {
        CHK_RET(CcuInitFeature(devLogicId_, ccuDrvHandle_));
    }

    if (!resPack_) {
        resPack_.reset(new (std::nothrow) CcuResPack(insType_));
        CHK_PTR_NULL(resPack_);
        CHK_RET(resPack_->Init());
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuInstance::ResetResPack()
{
    if (!resPack_) {
        return HcclResult::HCCL_SUCCESS;
    }

    untranslatedKernelHandles_.clear();
    CHK_RET(resPack_->Reset());
    return HcclResult::HCCL_SUCCESS;
}

CcuResPack *CcuInstance::GetResPack()
{
    return resPack_.get();
}

HcclResult CcuInstance::SaveCcuKernel(const CcuKernelHandle kernelHandle)
{
    EXCEPTION_HANDLE_BEGIN
    kernelHandles_.push_back(kernelHandle);
    untranslatedKernelHandles_.push_back(kernelHandle);
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

const std::vector<CcuKernelHandle> &CcuInstance::GetUntranslatedKernels()
{
    return untranslatedKernelHandles_;
}

} // namespace hcomm