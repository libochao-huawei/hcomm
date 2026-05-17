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

#include "hccl_common.h"
#include "log.h"
#include "base_comm_res_mgr.h"

namespace hcomm {

HcommResMgr& HcommResMgr::GetInstance(const uint32_t devicePhyId)
{
    uint32_t devPhyId = devicePhyId;
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[HcommResMgr][%s] use the backup device, devPhyId[%u] should be "
            "less than %u.", __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
        devPhyId = MAX_MODULE_DEVICE_NUM;
    }

    // 某个设备的BaseCommRes实例只能创建一次，后续调用GetOrCreate会返回已存在的实例
    (void)BaseCommResMgr::Instance().GetOrCreate(devPhyId);

    static HcommResMgr hcommResMgrs[MAX_MODULE_DEVICE_NUM + 1];
    hcommResMgrs[devPhyId].devPhyId_ = devPhyId;

    return hcommResMgrs[devPhyId];
}

HcommResMgr::HcommResMgr()
{
}

HcommResMgr::~HcommResMgr()
{
}

} // namespace hcomm
