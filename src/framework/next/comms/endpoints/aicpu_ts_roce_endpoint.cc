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

namespace hcomm {
AicpuTsRoceEndpoint::AicpuTsRoceEndpoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
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
    info.devicePhyId = devPhyId;
    HcclNetDev netDev = nullptr;
    HcclResult ret = HcclNetDevOpen(&info, &netDev);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] HcclNetDevOpen failed, ret[%d]", __func__, ret);
        return ret;
    }
    netDev_ = netDev;

    auto *netDevCtx = static_cast<hccl::NetDevContext *>(netDev_);
    CHK_PTR_NULL(netDevCtx);
    const hccl::HcclIpAddress ipAddr = netDevCtx->GetLocalIp();
    RdmaHandle rdmaHandle = nullptr;
    CHK_RET(hccl::NetworkManager::GetInstance(netDevCtx->GetLogicId()).GetRdmaHandleByIpAddr(ipAddr, rdmaHandle));
    ctxHandle_ = rdmaHandle;
    CHK_PTR_NULL(ctxHandle_);
    HCCL_INFO("AicpuTsRoceEndpoint::%s success, devId[%u], ipAddr[%s], ctxHandle[%p]",
        __func__, devPhyId, ipAddr.GetReadableAddress(), ctxHandle_);

    EXECEPTION_CATCH(regedMemMgr_ = std::make_shared<AicpuTsRoceRegedMemMgr>(netDev_), return HCCL_E_PTR);
    this->regedMemMgr_->rdmaHandle_ = this->ctxHandle_;

    constexpr uint32_t defaultPort = 16666;
    CHK_RET(ServerSocketListen(defaultPort));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceEndpoint::ServerSocketListen(const uint32_t port)
{
    auto &serverSocketMap = GetServerSocketMap();
    if (serverSocketMap.find(port) != serverSocketMap.end()) {
        HCCL_INFO("[AicpuTsRoceEndpoint::%s] reuse serverSocket for port[%u]", __func__, port);
        return HCCL_SUCCESS;
    }

    uint32_t listenPort = (port != 0) ? port : 60001;
    EXECEPTION_CATCH(
        serverSocket_ = std::make_shared<hccl::HcclSocket>(static_cast<HcclNetDevCtx>(netDev_), listenPort),
        return HCCL_E_PTR);
    CHK_PTR_NULL(serverSocket_);

    HcclResult ret = serverSocket_->Init();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] HcclSocket Init failed, ret[%d]", __func__, ret);
        return ret;
    }

    ret = serverSocket_->Listen();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] HcclSocket Listen failed, ret[%d]", __func__, ret);
        return ret;
    }

    serverSocketMap[listenPort] = serverSocket_;
    HCCL_INFO("[AicpuTsRoceEndpoint][%s] listen on port[%u] success", __func__, listenPort);
    return HCCL_SUCCESS;
}

std::unordered_map<uint32_t, std::shared_ptr<hccl::HcclSocket>> &AicpuTsRoceEndpoint::GetServerSocketMap()
{
    static std::unordered_map<uint32_t, std::shared_ptr<hccl::HcclSocket>> serverSocketMap;
    return serverSocketMap;
}

HcclResult AicpuTsRoceEndpoint::AddListenSocketWhiteList(uint32_t port, const std::vector<SocketWlistInfo> &wlistInfos)
{
    if (wlistInfos.empty()) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] empty whitelist", __func__);
        return HCCL_E_PARA;
    }
    auto &sockMap = GetServerSocketMap();
    const uint32_t listenPort = (port != 0U) ? port : 60001U;
    auto it = sockMap.find(listenPort);
    if (it == sockMap.end() || it->second == nullptr) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] no listen socket for port[%u]", __func__, listenPort);
        return HCCL_E_NOT_FOUND;
    }
    std::vector<SocketWlistInfo> mutableCopy = wlistInfos;
    return it->second->AddWhiteList(mutableCopy);
}

HcclResult AicpuTsRoceEndpoint::AcceptDataSocket(uint32_t port, const std::string &tag,
    std::shared_ptr<hccl::HcclSocket> &outConnected, uint32_t acceptTimeoutMs)
{
    auto &sockMap = GetServerSocketMap();
    const uint32_t listenPort = (port != 0U) ? port : 60001U;
    auto it = sockMap.find(listenPort);
    if (it == sockMap.end() || it->second == nullptr) {
        HCCL_ERROR("[AicpuTsRoceEndpoint][%s] no listen socket for port[%u]", __func__, listenPort);
        return HCCL_E_NOT_FOUND;
    }
    return it->second->Accept(tag, outConnected, acceptTimeoutMs);
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