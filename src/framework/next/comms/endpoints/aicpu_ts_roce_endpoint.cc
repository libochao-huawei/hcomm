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
    ReleaseListenSocketRefs();
    regedMemMgr_.reset();
    ctxHandle_ = nullptr;
    if (netDevPhyId_ != kInvalidNetDevPhyId) {
        ReleaseNetDev(netDevPhyId_);
        netDevPhyId_ = kInvalidNetDevPhyId;
        netDev_ = nullptr;
    }
}

void AicpuTsRoceEndpoint::ReleaseListenSocketRefs()
{
    std::lock_guard<std::mutex> lk(ListenSocketMapMutex());
    std::vector<uint32_t> ports = std::move(listenRefPorts_);
    auto &sockMap = GetServerSocketMap();
    for (uint32_t port : ports) {
        auto it = sockMap.find(port);
        if (it == sockMap.end()) {
            continue;
        }
        if (it->second.refCount > 0U) {
            it->second.refCount--;
        }
        if (it->second.refCount == 0U) {
            (void)sockMap.erase(it);
        }
    }
    serverSocket_.reset();
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

    HcclNetDevInfos info{};
    info.addr.protoType = HCCL_PROTO_TYPE_ROCE;
    CHK_RET(CommAddrTypeToHcclAddressType(endpointDesc_.commAddr.type, info.addr.type));
    if (endpointDesc_.commAddr.type == COMM_ADDR_TYPE_IP_V4) {
        info.addr.addr = endpointDesc_.commAddr.addr;
    } else {
        info.addr.addr6 = endpointDesc_.commAddr.addr6;
    }
    info.netdevDeployment = HCCL_NETDEV_DEPLOYMENT_DEVICE;
    info.devicePhyId = static_cast<int32_t>(devPhyId);

    HcclNetDev acquired = nullptr;
    const HcclResult acquireRet = AcquireNetDev(devPhyId, info, &acquired);
    if (acquireRet != HCCL_SUCCESS) {
        return acquireRet;
    }
    netDev_ = acquired;
    netDevPhyId_ = devPhyId;

    auto *netDevCtx = static_cast<hccl::NetDevContext *>(netDev_);
    if (UNLIKELY(netDevCtx == nullptr)) {
        HCCL_ERROR("[%s]errNo[0x%016llx]ptr [netDevCtx] is nullptr, return HCCL_E_PTR", __func__,
            HCCL_ERROR_CODE(HCCL_E_PTR));
        ReleaseNetDev(netDevPhyId_);
        netDev_ = nullptr;
        netDevPhyId_ = kInvalidNetDevPhyId;
        return HCCL_E_PTR;
    }

    const hccl::HcclIpAddress ipAddr = netDevCtx->GetLocalIp();
    RdmaHandle rdmaHandle = nullptr;
    HcclResult ret =
        hccl::NetworkManager::GetInstance(netDevCtx->GetLogicId()).GetRdmaHandleByIpAddr(ipAddr, rdmaHandle);
    if (UNLIKELY(ret != HCCL_SUCCESS)) {
        if (ret == HCCL_E_AGAIN) {
            HCCL_WARNING("[%s]call trace: hcclRet -> %d", __func__, ret);
        } else {
            HCCL_ERROR("[%s]call trace: hcclRet -> %d", __func__, ret);
        }
        ReleaseNetDev(netDevPhyId_);
        netDev_ = nullptr;
        netDevPhyId_ = kInvalidNetDevPhyId;
        return ret;
    }
    ctxHandle_ = rdmaHandle;
    if (UNLIKELY(ctxHandle_ == nullptr)) {
        HCCL_ERROR("[%s]errNo[0x%016llx]ptr [ctxHandle_] is nullptr, return HCCL_E_PTR", __func__,
            HCCL_ERROR_CODE(HCCL_E_PTR));
        ReleaseNetDev(netDevPhyId_);
        netDev_ = nullptr;
        netDevPhyId_ = kInvalidNetDevPhyId;
        return HCCL_E_PTR;
    }

    HCCL_INFO("AicpuTsRoceEndpoint::%s success, devId[%u], ipAddr[%s], ctxHandle[%p]", __func__, devPhyId,
        ipAddr.GetReadableAddress(), ctxHandle_);

    try {
        regedMemMgr_ = std::make_shared<AicpuTsRoceRegedMemMgr>(netDev_);
    } catch (std::exception &e) {
        HCCL_ERROR("[%s]Failed, exception caught:%s", __func__, e.what());
        regedMemMgr_.reset();
        ReleaseNetDev(netDevPhyId_);
        netDev_ = nullptr;
        netDevPhyId_ = kInvalidNetDevPhyId;
        ctxHandle_ = nullptr;
        return HCCL_E_PTR;
    }
    regedMemMgr_->rdmaHandle_ = ctxHandle_;

    constexpr uint32_t defaultPort = 16666;
    ret = ServerSocketListen(defaultPort);
    if (UNLIKELY(ret != HCCL_SUCCESS)) {
        if (ret == HCCL_E_AGAIN) {
            HCCL_WARNING("[%s]call trace: hcclRet -> %d", __func__, ret);
        } else {
            HCCL_ERROR("[%s]call trace: hcclRet -> %d", __func__, ret);
        }
        regedMemMgr_.reset();
        ReleaseNetDev(netDevPhyId_);
        netDev_ = nullptr;
        netDevPhyId_ = kInvalidNetDevPhyId;
        ctxHandle_ = nullptr;
        return ret;
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::ServerSocketListen(const uint32_t port)
{
    {
        std::lock_guard<std::mutex> lk(ListenSocketMapMutex());
        auto &serverSocketMap = GetServerSocketMap();
        auto it = serverSocketMap.find(port);
        if (it != serverSocketMap.end() && it->second.socket != nullptr) {
            it->second.refCount++;
            listenRefPorts_.push_back(port);
            serverSocket_ = it->second.socket;
            HCCL_INFO("[AicpuTsRoceEndpoint::%s] reuse serverSocket for port[%u], ref[%u]", __func__, port,
                it->second.refCount);
            return HCCL_SUCCESS;
        }
    }

    EXECEPTION_CATCH(
        serverSocket_ = std::make_shared<hccl::HcclSocket>(static_cast<HcclNetDevCtx>(netDev_), port),
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
    auto it = serverSocketMap.find(port);
    if (it != serverSocketMap.end() && it->second.socket != nullptr) {
        it->second.refCount++;
        listenRefPorts_.push_back(port);
        serverSocket_ = it->second.socket;
        HCCL_INFO("[AicpuTsRoceEndpoint::%s] concurrent reuse port[%u], ref[%u]", __func__, port,
            it->second.refCount);
        return HCCL_SUCCESS;
    }

    serverSocketMap[port] = AicpuTsListenSocketSlot{ serverSocket_, 1U };
    listenRefPorts_.push_back(port);
    HCCL_INFO("[AicpuTsRoceEndpoint][%s] listen on port[%u] success", __func__, port);
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

std::mutex &AicpuTsRoceEndpoint::NetDevPhyIdMapMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<uint32_t, AicpuTsNetDevSlot> &AicpuTsRoceEndpoint::GetNetDevPhyIdMap()
{
    static std::unordered_map<uint32_t, AicpuTsNetDevSlot> netDevPhyIdMap;
    return netDevPhyIdMap;
}

HcclResult AicpuTsRoceEndpoint::AcquireNetDev(uint32_t devPhyId, const HcclNetDevInfos &info,
    HcclNetDev *outNetDev)
{
    CHK_PTR_NULL(outNetDev);
    std::lock_guard<std::mutex> lk(NetDevPhyIdMapMutex());
    auto &map = GetNetDevPhyIdMap();
    auto it = map.find(devPhyId);
    if (it != map.end() && it->second.netDev != nullptr) {
        it->second.refCount++;
        *outNetDev = it->second.netDev;
        HCCL_INFO("[AicpuTsRoceEndpoint::%s] reuse HcclNetDev for devPhyId[%u], ref[%u]", __func__, devPhyId,
            it->second.refCount);
        return HCCL_SUCCESS;
    }

    HcclNetDev netDev = nullptr;
    const HcclResult ret = HcclNetDevOpen(&info, &netDev);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] HcclNetDevOpen failed, ret[%d]", __func__, ret);
        *outNetDev = nullptr;
        return ret;
    }

    map[devPhyId] = AicpuTsNetDevSlot{ netDev, 1U };
    *outNetDev = netDev;
    HCCL_INFO("[AicpuTsRoceEndpoint::%s] HcclNetDevOpen devPhyId[%u] new slot ref[1]", __func__, devPhyId);
    return HCCL_SUCCESS;
}

void AicpuTsRoceEndpoint::ReleaseNetDev(uint32_t devPhyId)
{
    if (devPhyId == kInvalidNetDevPhyId) {
        return;
    }
    std::lock_guard<std::mutex> lk(NetDevPhyIdMapMutex());
    auto &map = GetNetDevPhyIdMap();
    auto it = map.find(devPhyId);
    if (it == map.end()) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][ReleaseNetDev] devPhyId[%u] not in map", devPhyId);
        return;
    }
    if (it->second.refCount > 0U) {
        it->second.refCount--;
    }
    if (it->second.refCount == 0U) {
        const HcclResult cret = HcclNetDevClose(it->second.netDev);
        if (cret != HCCL_SUCCESS) {
            HCCL_ERROR("[AicpuTsRoceEndpoint][ReleaseNetDev] HcclNetDevClose failed, ret[%d]", cret);
        }
        (void)map.erase(it);
    }
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
