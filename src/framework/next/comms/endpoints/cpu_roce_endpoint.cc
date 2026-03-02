/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "endpoint_mgr.h"
#include "hccl_mem_defs.h"
#include "cpu_roce_endpoint.h"
#include "hccl/hccl_res.h"
#include "log.h"
#include "reged_mems/roce_mem.h"
#include "host_socket_handle_manager.h"
#include "adapter_rts_common.h"
 
namespace hcomm {
CpuRoceEndpoint::CpuRoceEndpoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
}

HcclResult CpuRoceEndpoint::Init()
{
    HCCL_INFO("[%s] localEndpoint protocol[%d]", __func__, endpointDesc_.protocol);

    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_HOST) {
        HCCL_INFO("[CpuRoceEndpoint][%s] CpuRoceEndpoint not support device", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr));
    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));
    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(devId, devPhyId));
    auto &rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
    ctxHandle_ = static_cast<void *>(
        rdmaHandleMgr.GetByAddr(devPhyId, Hccl::LinkProtoType::RDMA, ipAddr, Hccl::PortDeploymentType::HOST_NET));
    CHK_PTR_NULL(ctxHandle_);
    HCCL_INFO("CpuRoceEndpoint::%s success, devId[%u], ipAddr[%s], ctxHandle[%p]",
        __func__,
        devPhyId,
        ipAddr.Describe().c_str(),
        ctxHandle_);

    CHK_RET(ServerSocketListen());
    EXECEPTION_CATCH(regedMemMgr_ = std::make_unique<RoceRegedMemMgr>(), return HCCL_E_PARA);
    this->regedMemMgr_->rdmaHandle_ = this->ctxHandle_;
    return HCCL_SUCCESS;
}

HcclResult CpuRoceEndpoint::ServerSocketListen()
{
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr));

    auto &serverSocketMap = CpuRoceEndpoint::GetServerSocketMap();
    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));
    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(devId, devPhyId));

    if (serverSocketMap.find(ipAddr) != serverSocketMap.end()) {
        HCCL_INFO("[CpuRoceEndpoint::%s] reuse serverSocket", __func__);
        return HCCL_SUCCESS;
    }

    Hccl::SocketHandle socketHandle{};
    EXECEPTION_CATCH(
        socketHandle = Hccl::HostSocketHandleManager::GetInstance().Create(devPhyId, ipAddr), return HCCL_E_PARA);

    HCCL_INFO("[CpuRoceEndpoint::%s] socketHandle[%p] devicePhyId[%u] ipAddress[%s]",
        __func__, socketHandle, devPhyId, ipAddr.Describe().c_str());

    std::shared_ptr<Hccl::Socket> serverSocket{};
    EXECEPTION_CATCH(
        serverSocket = std::make_shared<Hccl::Socket>(socketHandle, ipAddr, 60001, ipAddr, "server",
                         Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE),
        return HCCL_E_PARA);

    HCCL_INFO("[CpuRoceEndpoint::%s] listen_socket_info[%s]", __func__, serverSocket->Describe().c_str());

    EXECEPTION_CATCH(serverSocket->Listen(), return HCCL_E_NETWORK);
    serverSocketMap[ipAddr] = serverSocket;
    return HCCL_SUCCESS;
}

std::unordered_map<Hccl::IpAddress, std::shared_ptr<Hccl::Socket>> &CpuRoceEndpoint::GetServerSocketMap()
{
    static std::unordered_map<Hccl::IpAddress, std::shared_ptr<Hccl::Socket>, std::hash<Hccl::IpAddress>>
        serverSocketMap;
    return serverSocketMap;
}

HcclResult CpuRoceEndpoint::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    CHK_RET(this->regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult CpuRoceEndpoint::UnregisterMemory(void* memHandle)
{
    CHK_RET(this->regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult CpuRoceEndpoint::MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    CHK_RET(this->regedMemMgr_->MemoryExport(this->endpointDesc_, memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult CpuRoceEndpoint::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    CHK_RET(this->regedMemMgr_->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult CpuRoceEndpoint::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    CHK_RET(this->regedMemMgr_->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

HcclResult CpuRoceEndpoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    CHK_RET(this->regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}
}