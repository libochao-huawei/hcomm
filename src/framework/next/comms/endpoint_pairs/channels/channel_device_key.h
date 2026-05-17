/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CHANNEL_DEVICE_KEY_H
#define CHANNEL_DEVICE_KEY_H

#include <cstddef>
#include <cstdint>
#include <functional>

#include "hcomm_res_defs.h"

namespace hcomm {

struct ChannelDeviceKey {
    int32_t deviceId;
    ChannelHandle handle;

    bool operator==(const ChannelDeviceKey& other) const
    {
        return deviceId == other.deviceId && handle == other.handle;
    }
};

struct ChannelDeviceKeyHash {
    std::size_t operator()(const ChannelDeviceKey& key) const
    {
        return std::hash<int32_t>()(key.deviceId) ^ (std::hash<ChannelHandle>()(key.handle) << 1);
    }
};

} // namespace hcomm

#endif // CHANNEL_DEVICE_KEY_H
