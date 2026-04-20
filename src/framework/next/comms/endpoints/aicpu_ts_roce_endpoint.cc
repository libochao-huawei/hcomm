/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aicpu_ts_roce_endpoint.h"
#include "hccl/hccl_res.h"
#include "endpoint_mgr.h"
#include "hccl_mem_defs.h"
#include "log.h"
#include "hccl_net_dev.h"
#include "reged_mems/aicpu_ts_roce_mem.h"
#include "adapter_rts_common.h"
#include "hccl_network.h"
#include "network_manager_pub.h"
#include "hccl_socket.h"
#include "exception_handler.h"
#include <exception>

namespace hcomm {
AicpuTsRoceEndpoint::AicpuTsRoceEndpoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
}

AicpuTsRoceEndpoint::~AicpuTsRoceEndpoint()
{
    regedMemMgr_.reset();
    ctxHandle_ = nullptr;
    ReleaseListenSocketRefs();
    ReleaseSharedNetDev();
}

void AicpuTsRoceEndpoint::ReleaseListenSocketRefs()
{
    std::lock_guard<std::mutex> lk(ListenSocketMapMutex());
    HCCL_ERROR("[ReleaseListenSocketRefs] netDevRefPhyId_[%u], listenRefPorts_.size[%zu]",
        netDevRefPhyId_, listenRefPorts_.size());

    std::vector<uint32_t> ports = std::move(listenRefPorts_);
    auto &sockMap = GetServerSocketMap();
    for (uint32_t port : ports) {
        auto it = sockMap.find(port);
        if (it == sockMap.end()) {
            HCCL_ERROR("[ReleaseListenSocketRefs] port[%u] not found in sockMap", port);
            continue;
        }
        HCCL_ERROR("[ReleaseListenSocketRefs] port[%u] refCount[%u] before decrement", port, it->second.refCount);
        if (it->second.refCount > 0U) {
            it->second.refCount--;
        }
        HCCL_ERROR("[ReleaseListenSocketRefs] port[%u] refCount[%u] after decrement, socket shared_ptr use_count[%ld]",
            port, it->second.refCount, it->second.socket.use_count());
        if (it->second.refCount == 0U) {
            HCCL_ERROR("[ReleaseListenSocketRefs] erasing port[%u] from sockMap", port);
            (void)sockMap.erase(it);
        }
    }
    HCCL_ERROR("[ReleaseListenSocketRefs] serverSocket_ use_count[%ld] before reset",
        serverSocket_.use_count());
    serverSocket_.reset();
}

std::mutex &AicpuTsRoceEndpoint::NetDevMapMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<uint32_t, AicpuTsNetDevSlot> &AicpuTsRoceEndpoint::GetNetDevMap()
{
    static std::unordered_map<uint32_t, AicpuTsNetDevSlot> netDevMap;
    return netDevMap;
}

HcclResult AicpuTsRoceEndpoint::AcquireSharedNetDev(uint32_t devicePhyId, const HcclNetDevInfos &info)
{
    std::lock_guard<std::mutex> lk(NetDevMapMutex());
    auto &netDevMap = GetNetDevMap();
    const auto it = netDevMap.find(devicePhyId);
    if (it != netDevMap.end()) {
        it->second.refCount++;
        netDev_ = it->second.netDev;
        netDevRefPhyId_ = devicePhyId;
        HCCL_INFO("[AicpuTsRoceEndpoint][%s] reuse HcclNetDev for devicePhyId[%u], ref[%u]", __func__, devicePhyId,
            it->second.refCount);
        return HCCL_SUCCESS;
    }

    HcclNetDev netDev = nullptr;
    const HcclResult ret = HcclNetDevOpen(&info, &netDev);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] HcclNetDevOpen failed, ret[%d]", __func__, ret);
        return ret;
    }
    netDevMap[devicePhyId] = AicpuTsNetDevSlot{ netDev, 1U };
    netDev_ = netDev;
    netDevRefPhyId_ = devicePhyId;
    return HCCL_SUCCESS;
}

void AicpuTsRoceEndpoint::ReleaseSharedNetDev()
{
    if (netDevRefPhyId_ == UINT32_MAX) {
        return;
    }
    const uint32_t key = netDevRefPhyId_;
    netDevRefPhyId_ = UINT32_MAX;
    HcclNetDev toClose = nullptr;
    {
        std::lock_guard<std::mutex> lk(NetDevMapMutex());
        auto &netDevMap = GetNetDevMap();
        const auto it = netDevMap.find(key);
        if (it == netDevMap.end()) {
            HCCL_ERROR("[AicpuTsRoceEndpoint][ReleaseSharedNetDev] missing slot for devicePhyId[%u]", key);
        } else {
            if (it->second.refCount > 0U) {
                it->second.refCount--;
            }
            if (it->second.refCount == 0U) {
                toClose = it->second.netDev;
                (void)netDevMap.erase(it);
            }
        }
    }
    netDev_ = nullptr;
    if (toClose != nullptr) {
        const HcclResult ret = HcclNetDevClose(toClose);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[AicpuTsRoceEndpoint][ReleaseSharedNetDev] HcclNetDevClose failed, ret[%d]", ret);
        }
    }
}

HcclResult AicpuTsRoceEndpoint::Init()
{
    HCCL_INFO("[%s] localEndpoint protocol[%d]", __func__, endpointDesc_.protocol);

    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_DEVICE) {
        HCCL_INFO("[AicpuTsRoceEndpoint][%s] AicpuTsRoceEndpoint not support host", __func__);
        return HCCL_E_NOT_SUPPORT;
    }

    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));
    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(devId, devPhyId));

    HcclNetDevInfos info;
    info.addr.protoType = HCCL_PROTO_TYPE_ROCE;
    CHK_RET(CommAddrTypeToHcclAddressType(endpointDesc_.commAddr.type, info.addr.type));
    if (endpointDesc_.commAddr.type == COMM_ADDR_TYPE_IP_V4) {
        info.addr.addr = endpointDesc_.commAddr.addr;
    } else {
        info.addr.addr6 = endpointDesc_.commAddr.addr6;
    }
    info.netdevDeployment = HCCL_NETDEV_DEPLOYMENT_DEVICE;
    info.devicePhyId = static_cast<int32_t>(devPhyId);
    HcclResult ret = AcquireSharedNetDev(devPhyId, info);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }

    auto *netDevCtx = static_cast<hccl::NetDevContext *>(netDev_);
    if (netDevCtx == nullptr) {
        ReleaseSharedNetDev();
        return HCCL_E_PTR;
    }
    const hccl::HcclIpAddress ipAddr = netDevCtx->GetLocalIp();
    RdmaHandle rdmaHandle = nullptr;
    ret = hccl::NetworkManager::GetInstance(netDevCtx->GetLogicId()).GetRdmaHandleByIpAddr(ipAddr, rdmaHandle);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]call trace: hcclRet -> %d", __func__, ret);
        ReleaseSharedNetDev();
        return ret;
    }
    ctxHandle_ = rdmaHandle;
    if (ctxHandle_ == nullptr) {
        HCCL_ERROR("[%s]errNo[0x%016llx]ptr [ctxHandle_] is nullptr, return HCCL_E_PTR", __func__,
            HCCL_ERROR_CODE(HCCL_E_PTR));
        ReleaseSharedNetDev();
        return HCCL_E_PTR;
    }
    HCCL_INFO("AicpuTsRoceEndpoint::%s success, devId[%u], ipAddr[%s], ctxHandle[%p]",
        __func__, devPhyId, ipAddr.GetReadableAddress(), ctxHandle_);

    try {
        regedMemMgr_ = std::make_shared<AicpuTsRoceRegedMemMgr>(netDev_);
    } catch (std::exception &e) {
        HCCL_ERROR("[%s]Failed, exception caught:%s", __func__, e.what());
        ctxHandle_ = nullptr;
        ReleaseSharedNetDev();
        return HCCL_E_PTR;
    }
    this->regedMemMgr_->rdmaHandle_ = this->ctxHandle_;

    constexpr uint32_t defaultPort = 16666;
    ret = ServerSocketListen(defaultPort);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]call trace: hcclRet -> %d", __func__, ret);
        regedMemMgr_.reset();
        ctxHandle_ = nullptr;
        ReleaseSharedNetDev();
        return ret;
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::ServerSocketListen(const uint32_t port)
{
    const uint32_t listenPort = (port != 0U) ? port : 60001U;

    {
        std::lock_guard<std::mutex> lk(ListenSocketMapMutex());
        auto &serverSocketMap = GetServerSocketMap();
        auto it = serverSocketMap.find(listenPort);
        if (it != serverSocketMap.end() && it->second.socket != nullptr) {
            it->second.refCount++;
            listenRefPorts_.push_back(listenPort);
            serverSocket_ = it->second.socket;
            HCCL_INFO("[AicpuTsRoceEndpoint::%s] reuse serverSocket for port[%u], ref[%u]", __func__, listenPort,
                it->second.refCount);
            return HCCL_SUCCESS;
        }
    }

    EXECEPTION_CATCH(
        serverSocket_ = std::make_shared<hccl::HcclSocket>(static_cast<HcclNetDevCtx>(netDev_), listenPort),
        return HCCL_E_PTR);
    CHK_PTR_NULL(serverSocket_);

    HcclResult ret = serverSocket_->Init();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] HcclSocket Init failed, ret[%d]", __func__, ret);
        serverSocket_.reset();
        return ret;
    }

    ret = serverSocket_->Listen();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] HcclSocket Listen failed, ret[%d]", __func__, ret);
        serverSocket_.reset();
        return ret;
    }

    std::lock_guard<std::mutex> lk(ListenSocketMapMutex());
    auto &serverSocketMap = GetServerSocketMap();
    auto it = serverSocketMap.find(listenPort);
    if (it != serverSocketMap.end() && it->second.socket != nullptr) {
        it->second.refCount++;
        listenRefPorts_.push_back(listenPort);
        serverSocket_ = it->second.socket;
        HCCL_INFO("[AicpuTsRoceEndpoint::%s] concurrent reuse port[%u], ref[%u]", __func__, listenPort,
            it->second.refCount);
        return HCCL_SUCCESS;
    }

    serverSocketMap[listenPort] = AicpuTsListenSocketSlot{ serverSocket_, 1U };
    listenRefPorts_.push_back(listenPort);
    HCCL_INFO("[AicpuTsRoceEndpoint][%s] listen on port[%u] success", __func__, listenPort);
    return HCCL_SUCCESS;
}

std::mutex &AicpuTsRoceEndpoint::ListenSocketMapMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<uint32_t, AicpuTsListenSocketSlot> &AicpuTsRoceEndpoint::GetServerSocketMap()
{
    static std::unordered_map<uint32_t, AicpuTsListenSocketSlot> serverSocketMap;
    return serverSocketMap;
}

HcclResult AicpuTsRoceEndpoint::AddListenSocketWhiteList(uint32_t port, const std::vector<SocketWlistInfo> &wlistInfos)
{
    if (wlistInfos.empty()) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] empty whitelist", __func__);
        return HCCL_E_PARA;
    }
    std::lock_guard<std::mutex> lk(ListenSocketMapMutex());
    auto &sockMap = GetServerSocketMap();
    const uint32_t listenPort = (port != 0U) ? port : 60001U;
    auto it = sockMap.find(listenPort);
    if (it == sockMap.end() || it->second.socket == nullptr) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] no listen socket for port[%u]", __func__, listenPort);
        return HCCL_E_NOT_FOUND;
    }
    std::vector<SocketWlistInfo> mutableCopy = wlistInfos;
    return it->second.socket->AddWhiteList(mutableCopy);
}

HcclResult AicpuTsRoceEndpoint::AcceptDataSocket(uint32_t port, const std::string &tag,
    std::shared_ptr<hccl::HcclSocket> &outConnected, uint32_t acceptTimeoutMs)
{
    std::lock_guard<std::mutex> lk(ListenSocketMapMutex());
    auto &sockMap = GetServerSocketMap();
    const uint32_t listenPort = (port != 0U) ? port : 60001U;
    auto it = sockMap.find(listenPort);
    if (it == sockMap.end() || it->second.socket == nullptr) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] no listen socket for port[%u]", __func__, listenPort);
        return HCCL_E_NOT_FOUND;
    }
    return it->second.socket->Accept(tag, outConnected, acceptTimeoutMs);
}

HcclResult AicpuTsRoceEndpoint::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    CHK_RET(this->regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::UnregisterMemory(void* memHandle)
{
    CHK_RET(this->regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    CHK_RET(this->regedMemMgr_->MemoryExport(this->endpointDesc_, memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    CHK_RET(this->regedMemMgr_->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    CHK_RET(this->regedMemMgr_->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    CHK_RET(this->regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}
}
