/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcomm_res_mgr.h"
#include "base_comm_res_mgr.h"
#include "hccl_common.h"

namespace hcomm {

HcommResMgr& HcommResMgr::GetInstance(const uint32_t devicePhyId)
{
    uint32_t devPhyId = devicePhyId;
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[HcommResMgr][%s] use the backup device, devPhyId[%u] should be "
            "less than %u.", __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
        devPhyId = MAX_MODULE_DEVICE_NUM; // 使用备份设备
    }

    static HcommResMgr hcommResMgrs[MAX_MODULE_DEVICE_NUM + 1];
    HcommResMgr& mgr = hcommResMgrs[devPhyId];

    {
        // 写过程确保线程安全
        std::lock_guard<std::mutex> lock(mgr.mutex_);
        if (mgr.baseCommRes_ == nullptr) {
        BaseCommResMgr& baseCommResMgr = BaseCommResMgr::Instance();
        mgr.baseCommRes_ = baseCommResMgr.GetOrCreate(devPhyId);
        if (mgr.baseCommRes_ == nullptr) {
            HCCL_ERROR("[HcommResMgr][%s] GetOrCreate BaseCommRes failed for devPhyId=%u",
                __func__, devPhyId);
        }
    }
    }

    return mgr;
}

} // namespace hcomm