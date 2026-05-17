/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcomm_dev_phy_id.h"

#include "hccl_common.h"
#include "adapter_hal_pub.h"
#include "log.h"

namespace hcomm {

uint32_t ResolveDevPhyIdFromEndpointDesc(const EndpointDesc& desc)
{
    if (desc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        uint32_t id = desc.loc.device.devPhyId;
        if (id >= MAX_MODULE_DEVICE_NUM) {
            HCCL_WARNING("[ResolveDevPhyIdFromEndpointDesc] devPhyId[%u] out of range, use backup slot", id);
            return MAX_MODULE_DEVICE_NUM;
        }
        return id;
    }
    return MAX_MODULE_DEVICE_NUM;
}

uint32_t ResolveDevPhyIdFromCurrentRtDevice()
{
    int32_t deviceId = 0;
    HcclResult ret = hrtGetDevice(&deviceId);
    if (ret != HCCL_SUCCESS || deviceId < 0 || static_cast<uint32_t>(deviceId) >= MAX_MODULE_DEVICE_NUM) {
        return MAX_MODULE_DEVICE_NUM;
    }
    return static_cast<uint32_t>(deviceId);
}

} // namespace hcomm
