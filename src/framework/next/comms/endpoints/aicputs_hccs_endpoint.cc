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
#include "aicputs_hccs_endpoint.h"
#include "hccl/hccl_res.h"
#include "log.h"
#include "adapter_rts_common.h"
#include "net_dev/global_net_dev_manager.h"
#include "hccs_mem.h"

namespace hcomm {

HcclResult CommAddrToIpAddress(const CommAddr &commAddr, hccl::HcclIpAddress &ipAddr)
{
    if (commAddr.type != COMM_ADDR_TYPE_IP_V4 && commAddr.type != COMM_ADDR_TYPE_IP_V6 && commAddr.type != COMM_ADDR_TYPE_EID) {
        HCCL_ERROR("[%s] failed, comm address type[%d] is not supported.", __func__, commAddr.type);
        return HCCL_E_NOT_SUPPORT;
    }

    hccl::HcclInAddr binAddr;
    int32_t family = AF_INET6;
    if (commAddr.type == COMM_ADDR_TYPE_IP_V4) {
        binAddr.addr = commAddr.addr;
        int32_t family = AF_INET;
        ipAddr = hccl::HcclIpAddress(family, binAddr);
        return HCCL_SUCCESS;
    }

    if (commAddr.type == COMM_ADDR_TYPE_EID){
        hccl::Eid inputEid;
        std::memcpy(inputEid.raw, commAddr.eid, hccl::URMA_EID_LEN);
        ipAddr = hccl::HcclIpAddress(inputEid);
        return HCCL_SUCCESS;
    }

    binAddr.addr6 = commAddr.addr6;
    ipAddr = hccl::HcclIpAddress(family, binAddr);
    return HCCL_SUCCESS;
}

AicpuTsHccsEndPoint::AicpuTsHccsEndPoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
}

AicpuTsHccsEndPoint::~AicpuTsHccsEndPoint()
{
    if (!ipAddr_.IsInvalid()) {
        hccl::GlobalNetDevMgr::GetInstance().UnRefNetDevCtx(NicType::DEVICE_NIC_TYPE, ipAddr_, server_port_);
    }
    
}

HcclResult AicpuTsHccsEndPoint::Init()
{
    HCCL_INFO("[%s] localEndpoint protocol[%d]", __func__, endpointDesc_.protocol);

    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_DEVICE) {
        HCCL_INFO("[AicpuTsHccsEndPoint][%s] AicpuTsHccsEndPoint not support host", __func__);
        return HCCL_E_NOT_SUPPORT;
    }

    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));
    u32 devPhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(devId, devPhyId));

    // need convert endpointDesc_.commAddr to ipAddr
    CHK_RET(CommAddrToIpAddress(endpointDesc_.commAddr, ipAddr_));
 
    // will not do ServerInit if server_port_ is 0
    CHK_RET(hccl::GlobalNetDevMgr::GetInstance().RefNetDevCtx(
        NicType::DEVICE_NIC_TYPE, ipAddr_, server_port_, netDevCtx_));

    EXECEPTION_CATCH(regedMemMgr_ = std::make_unique<HccsRegedMemMgr>(netDevCtx_), return HCCL_E_PARA);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::ServerSocketListen(const uint32_t port)
{
    // will do ServerInit
    CHK_RET(hccl::GlobalNetDevMgr::GetInstance().ServerInit(netDevCtx_, port));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::ServerSocketStopListen(const uint32_t port)
{
    CHK_RET(hccl::GlobalNetDevMgr::GetInstance().ServerDeInit(netDevCtx_, port));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::RegisterMemory(HcommMem mem, const char *memTag, void **memHandle)
{
    CHK_RET(this->regedMemMgr_->RegisterMemory(mem, memTag, memHandle));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::UnregisterMemory(void* memHandle)
{
    CHK_RET(this->regedMemMgr_->UnregisterMemory(memHandle));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen)
{
    CHK_RET(this->regedMemMgr_->MemoryExport(this->endpointDesc_, memHandle, memDesc, memDescLen));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem)
{
    CHK_RET(this->regedMemMgr_->MemoryImport(memDesc, descLen, outMem));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::MemoryUnimport(const void *memDesc, uint32_t descLen)
{
    CHK_RET(this->regedMemMgr_->MemoryUnimport(memDesc, descLen));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    CHK_RET(this->regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::MemoryGrant(void* memHandle, const HcommMemGrantInfo *remoteGrantInfo)
{
    CHK_RET(this->regedMemMgr_->MemoryGrant(memHandle, remoteGrantInfo));
    return HCCL_SUCCESS;
}
}