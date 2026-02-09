/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ub_mem_endpoint.h"
#include "log.h"
#include "hccl_res.h"
#include "adapter_rts_common.h"
#include "server_socket_mgr.h"
#include "ub_mem.h"
 
namespace hcomm {
UbMemEndpoint::UbMemEndpoint(const EndpointDesc &endpointDesc) : Endpoint(endpointDesc){}

HcclResult UbMemEndpoint::Init()
{
    s32 deviceLogicId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    CHK_RET(ServerSocketMgr::ListenStart(deviceLogicId, endpointDesc_.commAddr, Hccl::NicType::DEVICE_NIC_TYPE));

    EXECEPTION_CATCH(regedMemMgr_ = std::make_unique<UbMemRegedMemMgr>(), return HCCL_E_INTERNAL);

    return HcclResult::HCCL_SUCCESS;
}

HcclResult UbMemEndpoint::ServerSocketListen()
{
    HCCL_INFO("UbMemEndpoint ServerSocketListen is not supported");
    return HCCL_SUCCESS;
}

HcclResult UbMemEndpoint::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    CHK_RET(this->regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult UbMemEndpoint::UnregisterMemory(void* memHandle)
{
    CHK_RET(this->regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult UbMemEndpoint::MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    HCCL_INFO("UbMemEndpoint MemoryExport is not supported");
    return HCCL_SUCCESS;
}

HcclResult UbMemEndpoint::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    HCCL_INFO("UbMemEndpoint MemoryImport is not supported");
    return HCCL_SUCCESS;
}

HcclResult UbMemEndpoint::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    HCCL_INFO("UbMemEndpoint MemoryUnimport is not supported");
    return HCCL_SUCCESS;
}

HcclResult UbMemEndpoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    HCCL_INFO("UbMemEndpoint GetAllMemHandles is not supported");
    return HCCL_SUCCESS;
}
}