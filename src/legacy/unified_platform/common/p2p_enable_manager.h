/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_P2P_ENABLE_MANAGER_H
#define HCCLV2_P2P_ENABLE_MANAGER_H

#include <set>
#include <mutex>
#include <utility>
<<<<<<< Updated upstream
=======
#include <map>
#include <functional>
#include <thread>
#include <array>
#include <vector>
>>>>>>> Stashed changes

#include "types.h"

namespace Hccl {

class P2PEnableManager {
public:
    static P2PEnableManager &GetInstance();

    // 测试使用
    std::set<std::pair<u32, u32>> GetSet()
    {
        return devicePairs;
    }

    ~P2PEnableManager();

private:
    std::set<std::pair<u32, u32>> devicePairs;

    P2PEnableManager() = default;

    P2PEnableManager(const P2PEnableManager &other) = delete;

    P2PEnableManager &operator=(const P2PEnableManager &other) = delete;
<<<<<<< Updated upstream
=======

    HcclResult EnableP2P(uint32_t localDeviceLogicID, uint32_t remoteDevicePhysicID);
    HcclResult WaitP2PEnabled(uint32_t localDeviceLogicID, uint32_t remoteDevicePhysicID);
    HcclResult WaitP2PConnected(int32_t localDeviceLogicID, uint32_t remoteDevicePhysicID) const;
    HcclResult DisableP2P(uint32_t localDeviceLogicID, uint32_t remoteDevicePhysicID);

    std::array<std::map<uint32_t, P2PConnectionInfo>, MAX_MODULE_DEVICE_NUM> connectionsInfo_;
    std::array<std::mutex, MAX_MODULE_DEVICE_NUM> connectionsLock_;
>>>>>>> Stashed changes
};
} // namespace Hccl

#endif // HCCLV2_P2P_ENABLE_MANAGER_H
