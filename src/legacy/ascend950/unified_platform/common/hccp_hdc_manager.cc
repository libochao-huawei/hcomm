/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp_hdc_manager.h"
#include "orion_adapter_tsd.h"
#include "orion_adapter_rts.h"
#include "socket_handle_manager.h"
#include "rdma_handle_manager.h"

namespace Hccl {

HccpHdcManager &HccpHdcManager::GetInstance()
{
    static HccpHdcManager hccpHdcManager;
    return hccpHdcManager;
}

void HccpHdcManager::Init(u32 deviceLogicId)
{
    std::unique_lock<std::mutex> lock(managerMutex);
    if (instances.count(deviceLogicId) != 0) {
        return;
    }

    HrtOpenTsdProcess(deviceLogicId);

    HRaInitConfig cfg;
    cfg.phyId = HrtGetDevicePhyIdByIndex(deviceLogicId);
    cfg.mode  = HrtNetworkMode::HDC;
    HrtRaInit(cfg);

    instances.insert(deviceLogicId);
}

void HccpHdcManager::DestroyAll()
{
    std::unique_lock<std::mutex> lock(managerMutex);
    for (auto deviceLogicId : instances) {
        HCCL_INFO("HccpHdcManager deinit");

        HRaInitConfig cfg;
        cfg.phyId = HrtGetDevicePhyIdByIndex(deviceLogicId);
        cfg.mode  = HrtNetworkMode::HDC;
        DECTOR_TRY_CATCH("HccpHdcManager", HrtRaDeInit(cfg));
        DECTOR_TRY_CATCH("HccpHdcManager", HrtResetDevice(deviceLogicId));
    }
    instances.clear();
}

HccpHdcManager::~HccpHdcManager()
{
    DECTOR_TRY_CATCH("HccpHdcManager", DestroyAll());
}

} // namespace Hccl
