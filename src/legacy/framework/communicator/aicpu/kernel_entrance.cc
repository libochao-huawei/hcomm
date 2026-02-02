/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "kernel_entrance.h"
#include "aicpu_comm_destroy_func.h"
#include "communicator_impl_lite_manager.h"
#include "ub_conn_lite_mgr.h"
#include "aicpu_daemon_service.h"
#include "task_exception_func.h"
#include "ns_recovery_handler_func.h"
#include "task_exception_handler_lite.h"
#include "log.h"
#include "inc/aicpu_utils.h"
extern "C" {
using namespace Hccl;

uint32_t HcclKernelEntrance(void *args)
{
    if (args == nullptr) {
        HCCL_ERROR("HcclKernelEntrance Args is null.");
        return 1;
    }
    
    auto *kernelParam = reinterpret_cast<HcclKernelParamLite *>(args);
    AicpuUtils::GetInstance().CreateSingleInstance(args);
    NsRecoveryHandlerFunc::GetInstance();
    CHK_RET(DlHalFunction::GetInstance().DlHalFunctionInit());

    u32 commIdIndex = kernelParam->comm.idIndex;
    HCCL_RUN_INFO("HcclKernelEntrance begin, OpType[%s] algName[%s] commIdIndex[%u] commId[%s] opTag[%s], devPhyId[%u] myRank[%u] rankSie[%u]",
        kernelParam->op.algOperator.opType.Describe().c_str(), kernelParam->algName, commIdIndex, kernelParam->comm.commId,
        kernelParam->opTag, kernelParam->comm.devPhyId, kernelParam->comm.myRank, kernelParam->comm.rankSie);
    Hccl::CommunicatorImplLite *communicatorImplLite = CommunicatorImplLiteMgr::GetInstance().Get(commIdIndex);
    if (communicatorImplLite == nullptr) {
        HCCL_ERROR("HcclKernelEntrance communicatorImplLite is null.");
        return 1;
    }

    CHK_RET(AicpuUtils::GetInstance().WaitCommFree(communicatorImplLite, __func__));
    if (communicatorImplLite->LoadWithOpBasedMode(kernelParam) != 0) {
        HCCL_ERROR("HcclKernelEntrance LoadWithOpBasedMode failed.");
        return 1;
    }

    HCCL_INFO("HcclKernelEntrance success.");
    unique_lock<std::mutex> aicpuLock(communicatorImplLite->GetAicpuMc2Mutex());
    communicatorImplLite->SetIsUsed(false);
    aicpuLock.unlock();
    return 0;
}

uint32_t HcclUpdateCommKernelEntrance(void *args)
{
    if (args == nullptr) {
        HCCL_ERROR("[NsRecovery] HcclUpdateCommKernelEntrance Args is null.");
        return 1;
    }
 
    auto *kernelParam = reinterpret_cast<HcclKernelParamLite *>(args);
    u32 commIdIndex = kernelParam->comm.idIndex;
    HCCL_INFO("[NsRecovery] HcclUpdateCommKernelEntrance begin, commIdIndex[%u]", commIdIndex);
 
    Hccl::CommunicatorImplLite *communicatorImplLite = CommunicatorImplLiteMgr::GetInstance().Get(commIdIndex);
    if (communicatorImplLite == nullptr) {
        HCCL_ERROR("HcclUpdateCommKernelEntrance communicatorImplLite is null.");
        return 1;
    }
    CHK_RET(AicpuUtils::GetInstance().WaitCommFree(communicatorImplLite, __func__));
    communicatorImplLite->UpdateComm(kernelParam);
    unique_lock<std::mutex> aicpuLock(communicatorImplLite->GetAicpuMc2Mutex());
    communicatorImplLite->SetIsUsed(false);
    aicpuLock.unlock();
    HCCL_INFO("[NsRecovery] HcclUpdateCommKernelEntrance success.");
    return 0;
}
}
