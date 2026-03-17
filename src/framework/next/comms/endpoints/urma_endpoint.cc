/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "urma_endpoint.h"
#include <algorithm>
#include "endpoint_mgr.h"
#include "log.h"
#include "hccl/hccl_res.h"
#include "reged_mems/urma_mem.h"
#include "adapter_rts_common.h"
 
namespace hcomm {
UrmaEndpoint::UrmaEndpoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
}

HcclResult UrmaEndpoint::Init()
{
    HCCL_INFO("[%s] localEndpoint protocol[%d]", __func__, endpointDesc_.protocol);

    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_DEVICE){
        HCCL_ERROR("[UrmaEndpoint][%s] endpointDesc.loc.locType[%d] only support ENDPOINT_LOC_TYPE_DEVICE", __func__, endpointDesc_.loc.locType);
        return HCCL_E_PARA;
    }
    Hccl::IpAddress ipAddr{};
    HcclResult ret = CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr);
    if(ret!= HCCL_SUCCESS) {
        HCCL_ERROR("call CommAddrToIpAddress failed");
        return ret;
    }


    u32 devPhyId;
    s32 deviceLogicId;
    ret = hrtGetDevice(&deviceLogicId);
    if(ret != HCCL_SUCCESS){
        HCCL_ERROR("call hrtGetDevice failed, deviceLogicId[%d]", deviceLogicId);
        return ret;
    }
    Hccl::HccpHdcManager::GetInstance().Init(deviceLogicId);
    ret = hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(deviceLogicId), devPhyId);
    if(ret!= HCCL_SUCCESS){
        HCCL_ERROR("call hrtGetDevicePhyIdByIndex failed, deviceLogicId[%d], devPhyId[%u]",deviceLogicId, devPhyId);
        return ret;
    }

    if (endpointDesc_.loc.device.devPhyId != devPhyId){
        HCCL_WARNING("[UrmaEndpoint][%s] endpointDesc.loc.device.devPhyId[%u] incorrect", __func__, endpointDesc_.loc.device.devPhyId);
        endpointDesc_.loc.device.devPhyId = devPhyId;   // 当前endpointDesc.loc.device.devPhyId不准，暂时由查询的devPhyId赋值
    }

    auto &rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
    EXECEPTION_CATCH(ctxHandle_ = static_cast<void *>(rdmaHandleMgr.GetByIp(endpointDesc_.loc.device.devPhyId, ipAddr)), return HCCL_E_PARA);
    CHK_PTR_NULL(ctxHandle_);
    HCCL_INFO("%s success, devId[%u], ipAddr[%s], ctxHandle[%p]",
        __func__, devPhyId, ipAddr.Describe().c_str(), ctxHandle_);

    CHK_RET(ServerSocketListen());
    EXECEPTION_CATCH(this->regedMemMgr_ = std::make_unique<UbRegedMemMgr>(), return HCCL_E_INTERNAL);
    this->regedMemMgr_->rdmaHandle_ = this->ctxHandle_;

    // ccu模式专用的资源分配器
    ccuChannelCtxPool_.reset(new (std::nothrow) CcuChannelCtxPool(deviceLogicId));
    CHK_PTR_NULL(ccuChannelCtxPool_);

    return HcclResult::HCCL_SUCCESS;
}

HcclResult UrmaEndpoint::ServerSocketListen()
{
    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_DEVICE){
        HCCL_INFO("[UrmaEndpoint][%s] endpointDesc.loc.locType[%d] skip create ServerSocket", __func__, endpointDesc_.loc.locType);
        return HCCL_SUCCESS;
    }

    Hccl::IpAddress ipaddr{};
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipaddr));

    HCCL_INFO("endpointDesc_.loc.device.devPhyId %u",endpointDesc_.loc.device.devPhyId);

    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UB);
    Hccl::PortData localPort = Hccl::PortData(static_cast<Hccl::RankId>(endpointDesc_.loc.device.devPhyId), type, 0, ipaddr);

    Hccl::SocketHandle socketHandle = Hccl::SocketHandleManager::GetInstance().Create(endpointDesc_.loc.device.devPhyId, localPort);
    auto &serverSocketMap = UrmaEndpoint::GetServerSocketMap();

    if (serverSocketMap.find(localPort) != serverSocketMap.end()){
        HCCL_INFO("[UrmaEndpoint][%s] reuse serverSocket", __func__);
        return HCCL_SUCCESS;
    }
    

    HCCL_INFO("[UrmaEndpoint][%s] socketHandle[%p] devicePhyId[%u] localPort[%s]", 
        __func__, 
        socketHandle, 
        endpointDesc_.loc.device.devPhyId, 
        localPort.Describe().c_str()
    );

    std::unique_ptr<Hccl::Socket> serverSocket;
    EXECEPTION_CATCH(serverSocket = std::make_unique<Hccl::Socket>(socketHandle, ipaddr, 60001, ipaddr, "server", Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE), return HCCL_E_PARA); //端口号可能冲突，需要SE做决定
    HCCL_INFO("[UrmaEndpoint][%s] listen_socket_info[%s]", __func__, serverSocket->Describe().c_str());
    EXECEPTION_CATCH(serverSocket->Listen(), return HCCL_E_INTERNAL);
    serverSocketMap[localPort] = std::move(serverSocket);

    return HCCL_SUCCESS;
}

std::unordered_map<Hccl::PortData, std::unique_ptr<Hccl::Socket>> &UrmaEndpoint::GetServerSocketMap()
{
    static std::unordered_map<Hccl::PortData, std::unique_ptr<Hccl::Socket>> serverSocketMap;
    return serverSocketMap;
}

HcclResult UrmaEndpoint::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    CHK_RET(this->regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult UrmaEndpoint::UnregisterMemory(void* memHandle)
{
    CHK_RET(this->regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult UrmaEndpoint::MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    CHK_RET(this->regedMemMgr_->MemoryExport(this->endpointDesc_, memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult UrmaEndpoint::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    CHK_RET(this->regedMemMgr_->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult UrmaEndpoint::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    CHK_RET(this->regedMemMgr_->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

HcclResult UrmaEndpoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    CHK_RET(this->regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

CcuChannelCtxPool *UrmaEndpoint::GetCcuChannelCtxPool()
{
    return ccuChannelCtxPool_.get();
}

}