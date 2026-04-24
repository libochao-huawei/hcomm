/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "uboe_endpoint.h"
#include <algorithm>
#include "endpoint_mgr.h"
#include "log.h"
#include "hccl/hccl_res.h"
#include "reged_mems/urma_mem.h"
#include "adapter_rts_common.h"
#include "server_socket_manager.h"
 
namespace hcomm {

UboeEndpoint::UboeEndpoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
}

HcclResult UboeEndpoint::Init()
{
    HCCL_INFO("[%s] localEndpoint protocol[%d]", __func__, endpointDesc_.protocol);

    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr));

    s32 deviceLogicId;
    u32 devPhyId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    CHK_RET(hrtGetDevicePhyIdByIndex(deviceLogicId, devPhyId));
    endpointDesc_.loc.device.devPhyId = devPhyId;

    Hccl::HccpHdcManager::GetInstance().Init(deviceLogicId);
    auto &rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
    Hccl::IpAddress eidAddress{};
    rdmaHandleMgr.UboeIpv4ToEid(ipAddr, eidAddress, devPhyId);
    EXECEPTION_CATCH(ctxHandle_ = static_cast<void *>(rdmaHandleMgr.GetByIp(endpointDesc_.loc.device.devPhyId, eidAddress)), 
        return HCCL_E_PARA);
    CHK_PTR_NULL(ctxHandle_);
    HCCL_INFO("%s success, devId[%u], eidAddress[%s], ctxHandle[%p]",
        __func__, devPhyId, eidAddress.Describe().c_str(), ctxHandle_);

    EXECEPTION_CATCH(regedMemMgr_ = std::make_unique<UbRegedMemMgr>(), return HCCL_E_INTERNAL);
    regedMemMgr_->rdmaHandle_ = ctxHandle_;

    return HcclResult::HCCL_SUCCESS;
}

HcclResult UboeEndpoint::ServerSocketListen(const uint32_t port)
{
    HCCL_INFO("%s is not supported", __func__);
    return HCCL_SUCCESS;
}

HcclResult UboeEndpoint::ServerSocketStopListen(const uint32_t port)
{
    HCCL_INFO("%s is not supported", __func__);
    return HCCL_SUCCESS;
}

HcclResult UboeEndpoint::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    CHK_RET(regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult UboeEndpoint::UnregisterMemory(void* memHandle)
{
    CHK_RET(regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult UboeEndpoint::MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    CHK_RET(regedMemMgr_->MemoryExport(endpointDesc_, memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult UboeEndpoint::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    CHK_RET(regedMemMgr_->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult UboeEndpoint::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    CHK_RET(regedMemMgr_->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

HcclResult UboeEndpoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    CHK_RET(regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

}