/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_HOST_SOCKET_HANDLE_MANAGER_H
#define HCCLV2_HOST_SOCKET_HANDLE_MANAGER_H

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <thread>
#include "sal.h"
#include "socket.h"
#include "orion_adapter_hccp.h"
#include "socket_handle_manager.h"
#include "ip_address.h"
#include "referenced.h"

namespace Hccl {

// socketHandle的计数器
using hostSocketHandleRef = std::pair<SocketHandle, Referenced>;

class HostSocketHandleManager {
public:
    static HostSocketHandleManager &GetInstance();

    SocketHandle Create(DevId devicePhyId, const IpAddress &hostIp);
    SocketHandle Get(DevId devicePhyId, const IpAddress &hostIp);
    void         Destroy(DevId devicePhyId, const IpAddress &hostIp);
    void StartUsing() {
        userCount.fetch_add(1, std::memory_order_relaxed);
    }
    
    void StopUsing() {
        userCount.fetch_sub(1, std::memory_order_relaxed);
    }
    
    // 等待所有用户完成，带超时
    bool WaitForNoUsers(int timeoutMs = 5000) {
        auto start = std::chrono::steady_clock::now();
        
        while (userCount.load(std::memory_order_acquire) > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            
            if (elapsed.count() > timeoutMs) {
                return false;  // 超时
            }
            
            SaluSleep(ONE_MILLISECOND_OF_USLEEP);
        }
        
        return true;  // 成功等待
    }

private:
    std::atomic<int> userCount{0};

    std::vector<unordered_map<string, hostSocketHandleRef>> hostSocketHandleMap;
    
    std::mutex socketHandleLock;

    HostSocketHandleManager();

    ~HostSocketHandleManager();

    void DestroyAll();

    HostSocketHandleManager(const HostSocketHandleManager &hostSocketHandleManager) = delete;

    HostSocketHandleManager &operator=(const HostSocketHandleManager &hostSocketHandleManager) = delete;
};

} // namespace Hccl

#endif // HCCLV2_HOST_SOCKET_HANDLE_MANAGER_H