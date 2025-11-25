/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ENDPOINT_MGR_H
#define ENDPOINT_MGR_H

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "endpoint.h"

namespace hcomm {
/**
 * @note 职责：EndPoint管理器，支持不同类型的EndPoint的创建和销毁管理。
 */
class EndPointMgr {
public:
    EndPointMgr();
    ~EndPointMgr();

    // 创建端点
    HcclResult CreateEndPoint(const EndPointInfo &endpoint, EndPointHandle* handle);

    // 销毁端点
    HcclResult DestroyEndPoint(EndPointHandle handle);

    // 获取端点
    std::shared_ptr<EndPoint> GetEndPoint(EndPointHandle handle);

    // 注册内存到端点
    HcclResult RegisterMemory(EndPointHandle handle, const std::vector<MemHandle>& memHandles);

    // 获取所有注册的内存信息
    HcclResult GetAllRegisteredMemory(std::vector<MemRegion>& memRegions);

private:
    std::unordered_map<EndPointHandle, std::shared_ptr<EndPoint>> endPoints_{};
    std::mutex mutex_{};
};
}

#endif // ENDPOINT_MGR_H
