/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "cpu_urma_endpoint.h"
#include <algorithm>
#include "endpoint_mgr.h"
#include "log.h"
#include "reged_mems/urma_mem.h"
#include "adapter_rts_common.h"
#include "server_socket_manager.h"
#include "hccp_peer_manager.h"

namespace hcomm {
CpuUrmaEndpoint::CpuUrmaEndpoint(const EndpointDesc& endpointDesc) : Endpoint(endpointDesc) {}

HcclResult CpuUrmaEndpoint::Init()
{
    HCCL_INFO("[%s] localEndpoint protocol[%d]", __func__, endpointDesc_.protocol);

    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr));
    u32 devPhyId;
    s32 deviceLogicId;
    HcclResult ret = hrtGetDevice(&deviceLogicId);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("call hrtGetDevice failed, deviceLogicId[%d]", deviceLogicId);
        return ret;
    }
    RaSocketSetWhiteListStatus(1); // PEER模式需要手动开启白名单模式
    Hccl::HccpPeerManager::GetInstance().Init(deviceLogicId);
    ret = hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(deviceLogicId), devPhyId);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("call hrtGetDevicePhyIdByIndex failed, deviceLogicId[%d], devPhyId[%u]", deviceLogicId, devPhyId);
        return ret;
    }
    auto& rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
    ctxHandle_ = static_cast<void*>(
        rdmaHandleMgr.GetByAddr(devPhyId, Hccl::LinkProtoType::UB, ipAddr, Hccl::PortDeploymentType::HOST_NET));
    CHK_PTR_NULL(ctxHandle_);
    HCCL_INFO(
        "CpuUrmaEndpoint::%s success, devId[%u], ipAddr[%s], ctxHandle[%p]", __func__, devPhyId,
        ipAddr.Describe().c_str(), ctxHandle_);

    EXECEPTION_CATCH(regedMemMgr_ = std::make_unique<UbRegedMemMgr>(), return HCCL_E_PARA);
    this->regedMemMgr_->rdmaHandle_ = this->ctxHandle_;
    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::ServerSocketListen(const uint32_t port)
{
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr));

    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));
    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(devId, devPhyId));

    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UB);
    Hccl::PortData localPort = Hccl::PortData(devPhyId, type, 0, ipAddr);

    HCCL_INFO("[CpuUrmaEndpoint::%s] devicePhyId[%u] ipAddress[%s]", __func__, devPhyId, ipAddr.Describe().c_str());

    CHK_RET(
        ServerSocketManager::GetInstance().ServerSocketStartListen(
            localPort, Hccl::NicType::HOST_NIC_TYPE, devPhyId, port));

    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::ServerSocketStopListen(const uint32_t port)
{
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr));

    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));
    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(devId, devPhyId));
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UB);
    Hccl::PortData localPort = Hccl::PortData(devPhyId, type, 0, ipAddr);
    CHK_RET(ServerSocketManager::GetInstance().ServerSocketStopListen(localPort, Hccl::NicType::HOST_NIC_TYPE, port));

    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::RegisterMemory(HcommMem mem, const char* memTag, void** memHandle)
{
    CHK_RET(this->regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::UnregisterMemory(void* memHandle)
{
    CHK_RET(this->regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::MemoryExport(void* memHandle, void** memDesc, uint32_t* memDescLen)
{
    CHK_RET(this->regedMemMgr_->MemoryExport(this->endpointDesc_, memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::MemoryImport(const void* memDesc, uint32_t descLen, HcommMem* outMem)
{
    CHK_RET(this->regedMemMgr_->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::MemoryUnimport(const void* memDesc, uint32_t descLen)
{
    CHK_RET(this->regedMemMgr_->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

HcclResult CpuUrmaEndpoint::GetAllMemHandles(void** memHandles, uint32_t* memHandleNum)
{
    CHK_RET(this->regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

} // namespace hcomm
