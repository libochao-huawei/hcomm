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
    std::lock_guard<std::mutex> lock(managerMutex_);

    if (instances_.count(deviceLogicId) != 0) {
        instances_[deviceLogicId].Ref();
        HCCL_INFO("[HccpHdcManager::%s] deviceLogicId[%u] ra has initialized, ref[%u].",
                   __func__, deviceLogicId, instances_[deviceLogicId].Count());
        return;
    }

    HrtOpenTSDProcess(deviceLogicId);

    HRaInitConfig cfg;
    cfg.phyId = HrtGetDevicePhyIdByIndex(deviceLogicId);
    cfg.mode  = HrtNetworkMode::HDC;
    HrtRaInit(cfg);

    instances_[deviceLogicId].Ref();
    HCCL_INFO("[HccpHdcManager::%s] deviceLogicId[%u] ra init success.", __func__, deviceLogicId);
}

void HccpHdcManager::DeInit(u32 deviceLogicId)
{
    std::lock_guard<std::mutex> lock(managerMutex_);

    if (isDestroy) {
        HCCL_WARNING("[HccpHdcManager::%s] HccpHdcManager has been destroy", __func__);
        return;
    }

    if (instances_.count(deviceLogicId) == 0) {
        HCCL_WARNING("[HccpHdcManager::%s] deviceLogicId[%u] not ra init", __func__, deviceLogicId);
        return;
    }

    instances_[deviceLogicId].Unref();
    u32 count = instances_[deviceLogicId].Count();
    HCCL_INFO("[HccpHdcManager::%s] deviceLogicId[%u] release one, ref[%u].", __func__, deviceLogicId, count);

    if (count == 0) {
        HRaInitConfig cfg;
        cfg.phyId = HrtGetDevicePhyIdByIndex(deviceLogicId);
        cfg.mode  = HrtNetworkMode::HDC;
        HrtRaDeInit(cfg);
        instances_.erase(deviceLogicId);
        HCCL_INFO("[HccpHdcManager::%s] deviceLogicId[%u] ra deinit success.", __func__, deviceLogicId);
    }
}

void HccpHdcManager::DeInitAll()
{
    std::lock_guard<std::mutex> lock(managerMutex_);
    isDestroy = true;

    for (auto const &instance : instances_) {
        u32 count = instance.second.Count();
        CHK_PRT_CONT(count != 0, HCCL_WARNING("[HccpHdcManager::%s] release is not as expected, "
                        "deviceLogicId[%u] ref[%u]", __func__, instance.first, count));
        HRaInitConfig cfg;
        cfg.phyId = HrtGetDevicePhyIdByIndex(instance.first);
        cfg.mode  = HrtNetworkMode::HDC;
        HrtRaDeInit(cfg);
        HCCL_INFO("[HccpHdcManager::%s] deviceLogicId[%u] ra deinit success.", __func__, instance.first);
    }

    instances_.clear();
}

HccpHdcManager::~HccpHdcManager()
{
    DECTOR_TRY_CATCH("HccpHdcManager", DeInitAll());
}

} // namespace Hccl
