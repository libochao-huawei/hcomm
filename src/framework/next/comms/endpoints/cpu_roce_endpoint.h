/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ROCE_ENDPOINT_H
#define ROCE_ENDPOINT_H

#include <memory>
#include <vector>
#include <string>
#include "endpoint.h"

namespace hcomm {
/**
 * @note 职责：Host CPU通信引擎+RoCE协议的通信设备Endpoint，管理通信设备上下文，以及设备上的注册内存。
 */
class CpuRoceEndpoint : public Endpoint {
public:
    explicit CpuRoceEndpoint(const EndpointDesc &endpointDesc);

    ~CpuRoceEndpoint() = default;

    HcclResult Init() override;

    HcclResult ServerSocketListen() override;

    HcclResult RegisterMemory(HcommMem mem, const char *memTag, void **memHandle) override;
    HcclResult UnregisterMemory(void* memHandle) override;
    HcclResult MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen) override;
    HcclResult MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem) override;
    HcclResult MemoryUnimport(const void *memDesc, uint32_t descLen) override;
    HcclResult GetAllMemHandles(void **memHandles, uint32_t *memHandleNum) override;

private:
    std::unordered_map<Hccl::IpAddress, std::shared_ptr<Hccl::Socket>> &GetServerSocketMap();
};
}
#endif // ROCE_ENDPOINT_H
