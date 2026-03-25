/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ns_recovery_lite.h"

namespace hccl
{

NsRecoveryLite::NsRecoveryLite(const HDCommunicatePtr& kfcControlTransferH2D, const HDCommunicatePtr& kfcStatusTransferD2H)
{
    hdcHandler_ = std::make_unique<HcclAicpuHdcHandler>(kfcControlTransferH2D, kfcStatusTransferD2H);
}

Hccl::KfcCommand NsRecoveryLite::BackGroundGetCmd()
{
    std::unique_lock<std::mutex> lock(hdcShmLock_);
    return hdcHandler_->GetKfcCommand();
}

void NsRecoveryLite::BackGroundSetStatus(Hccl::KfcStatus status, Hccl::KfcErrType errorCode)
{
    std::unique_lock<std::mutex> lock(hdcShmLock_);
    hdcHandler_->SetKfcExecStatus(status, errorCode);
}

void NsRecoveryLite::ResetErrorReported() 
{
    isErrorReported_ = false;
}
void NsRecoveryLite::SetNeedClean(bool flag)
{
    needClean_ = flag;
}
bool NsRecoveryLite::IsNeedClean() const
{
    return needClean_;
}
void NsRecoveryLite::SetIsSuspended(bool status)
{
    isSuspended_ = status;
}
bool NsRecoveryLite::IsSuspended() const
{
    return isSuspended_;
}

}