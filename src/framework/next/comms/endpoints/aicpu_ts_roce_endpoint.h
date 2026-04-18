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

#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "hccl_mem_defs.h"
#include "hccl_net_dev.h"
#include "endpoint.h"
#include "hccl_socket.h"

namespace hcomm {
/** 每监听端口一条：多 endpoint 共端口时 refCount 递增，最后一个析构时再销毁 socket */
struct AicpuTsListenSocketSlot {
    std::shared_ptr<hccl::HcclSocket> socket{};
    uint32_t refCount{0U};
};

/**
 * 同一进程内相同 devPhyId 共用一条 HcclNetDev；首个成功 HcclNetDevOpen 的 HcclNetDevInfos（含地址）决定
 * NetDevContext，后续 endpoint 仅递增 refCount。若需按地址区分，应扩展 map 键。
 */
struct AicpuTsNetDevSlot {
    HcclNetDev netDev{nullptr};
    uint32_t refCount{0U};
};

/**
 * @note 职责：AICPU_TS通信引擎+RoCE协议的通信设备EndPoint，管理通信设备上下文，以及设备上的注册内存。
 * HcclNetDev 按 devPhyId 进程内共享引用计数；传给 AicpuTsRoceRegedMemMgr 进行注册注销；
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
    static constexpr uint32_t kInvalidNetDevPhyId = 0xFFFFFFFFu;

    static std::unordered_map<uint32_t, AicpuTsListenSocketSlot> &GetServerSocketMap();
    static std::mutex &ListenSocketMapMutex();
    static std::unordered_map<uint32_t, AicpuTsNetDevSlot> &GetNetDevPhyIdMap();
    /** 与 ListenSocketMapMutex 分离，避免 Init 中持锁后进入 ServerSocketListen 同线程死锁 */
    static std::mutex &NetDevPhyIdMapMutex();
    HcclResult AcquireNetDev(uint32_t devPhyId, const HcclNetDevInfos &info, HcclNetDev *outNetDev);
    void ReleaseNetDev(uint32_t devPhyId);
    void ReleaseListenSocketRefs();

    HcclNetDev netDev_{nullptr};
    /** 参与共享表时的物理设备 ID；未持引用时为 kInvalidNetDevPhyId */
    uint32_t netDevPhyId_{kInvalidNetDevPhyId};
    std::shared_ptr<hccl::HcclSocket> serverSocket_{nullptr};
    /** 仅本对象成功 Listen 的端口；析构时只对这些端口减 ref / 可能销毁 socket，与其它 endpoint 的端口无关 */
    std::vector<uint32_t> listenRefPorts_{};
};
}
#endif // AICPUTS_ROCE_ENDPOINT_H
