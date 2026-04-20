/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AICPUTS_ROCE_ENDPOINT_H
#define AICPUTS_ROCE_ENDPOINT_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <unordered_map>
#include "hccl_mem_defs.h"
#include "endpoint.h"
#include "hccl_socket.h"

namespace hcomm {
/** 每监听端口一条：多 endpoint 共端口时 refCount 递增，最后一个析构时再销毁 socket */
struct AicpuTsListenSocketSlot {
    std::shared_ptr<hccl::HcclSocket> socket{};
    uint32_t refCount{0U};
};

struct AicpuTsNetDevSlot {
    HcclNetDev netDev{nullptr};
    uint32_t refCount{0U};
};

/**
 * @note 职责：AICPU_TS通信引擎+RoCE协议的通信设备EndPoint，管理通信设备上下文，以及设备上的注册内存。
 * 通过进程级表按 devicePhyId 引用计数共享 HcclNetDev，传给 AicpuTsRoceRegedMemMgr；
 * 初始化时调用 hccl::HcclSocket 进行监听。
 */
class AicpuTsRoceEndpoint : public Endpoint {
public:
    explicit AicpuTsRoceEndpoint(const EndpointDesc &endpointDesc);
    ~AicpuTsRoceEndpoint() override;

    HcclResult Init() override;

    HcclResult ServerSocketListen(const uint32_t port) override;

    HcclResult RegisterMemory(HcommMem mem, const char *memTag, void **memHandle) override;
    HcclResult UnregisterMemory(void* memHandle) override;
    HcclResult MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen) override;
    HcclResult MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem) override;
    HcclResult MemoryUnimport(const void *memDesc, uint32_t descLen) override;
    HcclResult GetAllMemHandles(void **memHandles, uint32_t *memHandleNum) override;

    HcclNetDev GetNetDev() const { return netDev_; }

    /** 在监听端口上 Accept 一条已建立的 RoCE 控制面连接，供 AicpuTsRoceChannel / TransportIbverbs 使用 */
    static HcclResult AcceptDataSocket(uint32_t port, const std::string &tag,
        std::shared_ptr<hccl::HcclSocket> &outConnected, uint32_t acceptTimeoutMs = 0);

    /** 向该端口上的监听 Socket 添加白名单（仅 SERVER 建链前调用，语义对齐 HcclSocketManager::AddWhiteList） */
    static HcclResult AddListenSocketWhiteList(uint32_t port, const std::vector<SocketWlistInfo> &wlistInfos);

private:
    static std::unordered_map<uint32_t, AicpuTsListenSocketSlot> &GetServerSocketMap();
    static std::mutex &ListenSocketMapMutex();
    void ReleaseListenSocketRefs();

    static std::unordered_map<uint32_t, AicpuTsNetDevSlot> &GetNetDevMap();
    static std::mutex &NetDevMapMutex();
    HcclResult AcquireSharedNetDev(uint32_t devicePhyId, const HcclNetDevInfos &info);
    void ReleaseSharedNetDev();

    HcclNetDev netDev_{nullptr};
    uint32_t netDevRefPhyId_{UINT32_MAX};
    std::shared_ptr<hccl::HcclSocket> serverSocket_{nullptr};
    std::vector<uint32_t> listenRefPorts_{};
};
}
#endif // AICPUTS_ROCE_ENDPOINT_H
