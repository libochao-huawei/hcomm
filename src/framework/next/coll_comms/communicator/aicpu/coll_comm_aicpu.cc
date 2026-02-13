/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_comm_aicpu.h"

HcclResult CollCommAicpu::InitAicpuIndOp(CommAicpuParam *commAicpuParam) {
    if (indOpCommInitialized_) {
        HCCL_RUN_INFO("[%s][InitAicpuIndOp]Group[%s] already initialized, skip reinit", __func__,
            identifier_.c_str());
        return HCCL_SUCCESS;
    }
    CHK_PTR_NULL(commAicpuParam);
    topoInfo_.deviceLogicId = commAicpuParam->deviceLogicId;
    topoInfo_.devicePhyId = commAicpuParam->devicePhyId;
    topoInfo_.deviceType = static_cast<DevType>(commAicpuParam->deviceType);
    identifier_ = std::string(commAicpuParam->hcomId);
    topoInfo_.userRankSize = commAicpuParam->userRankSize;
    topoInfo_.userRank = commAicpuParam->userRank; 
    notifys_.reserve(LOCAL_NOTIFY_MAX_NUM);
    notifySize_ = NOTIFY_SIZE_EIGHT;

    CHK_RET(hrtSetWorkModeAicpu(true)); // ??
    CHK_RET(hrtSetlocalDevice(topoInfo_.deviceLogicId)); // ??
    CHK_RET(hrtSetlocalDeviceType(topoInfo_.deviceType)); // ??
    CHK_RET(hrtDrvGetLocalDevIDByHostDevID(topoInfo_.devicePhyId, &devId_)); // ??
    CHK_RET(taskExecption_.Init(devId_, localUserRank_, identifier_)); // ??
    // CHK_RET(RegisterProfCallBack());

    HCCL_INFO("[HcclCommAicpu][InitAicpuIndOp] InitAicpuIndOpV2 start");
    indOpCommInitialized_ = true;
    return HCCL_SUCCESS;
}