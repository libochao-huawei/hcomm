/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "cluster_monitor_c_adpt.h"
#include "op_base.h"
#include "coll_comm_mgr.h"

using namespace hccl;

/**
 * @note 职责：集群维测的C接口声明（暂未对外的接口）
 */

HcclResult HcclRegisterToClusterMonitor(HcclComm comm)
{
    HCCL_INFO("[%s] START, comm[%p].", __func__, comm);
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    if (!hcclComm->IsCommunicatorV2()) {
        HCCL_ERROR("[%s] comm is NOT_SUPPORT", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    CHK_RET(CollCommMgr::GetInstance()->GetClusterMonitor(collComm->GetDeviceLogicId()).RegisterToClusterMonitor(comm));
    HCCL_RUN_INFO("%s Success", __func__);
    return HCCL_SUCCESS;
}

HcclResult HcclUnRegiterToClusterMonitor(HcclComm comm)
{
    return HCCL_SUCCESS;
}