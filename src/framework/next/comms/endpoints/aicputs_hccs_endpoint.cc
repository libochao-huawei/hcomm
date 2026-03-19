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

using namespace hccl;

namespace hcomm {
AicpuTsHccsEndPoint::AicpuTsHccsEndPoint(const EndpointDesc &endpointDesc)
    : Endpoint(endpointDesc)
{
    myId_ = ++allId_;
    HCCL_INFO("[AicpuTsHccsEndPoint][AicpuTsHccsEndPoint] myID[%lu]", myId_);
}

AicpuTsHccsEndPoint::~AicpuTsHccsEndPoint()
{
    if (!devIpAddr_.IsInvalid()) {
        hccl::GlobalNetDevMgr::GetInstance(endpointDesc_.loc.device.devPhyId).UnRefNetDevCtx(
            NicType::VNIC_TYPE, devIpAddr_, server_port_);
    }
}

HcclResult AicpuTsHccsEndPoint::Init()
{
    HCCL_INFO("[%s]localEndpoint protocol[%d], type[%u], id[%u] locType[%u], devPhyId[%u], serverIdx[%u], superDevId[%u], superPodIdx[%u]",
        __func__, endpointDesc_.protocol, endpointDesc_.commAddr.type, endpointDesc_.commAddr.id,
        endpointDesc_.loc.locType, endpointDesc_.loc.device.devPhyId, endpointDesc_.loc.device.serverIdx,
        endpointDesc_.loc.device.superDevId, endpointDesc_.loc.device.superPodIdx);

    if (endpointDesc_.loc.locType != ENDPOINT_LOC_TYPE_DEVICE) {
        HCCL_INFO("[AicpuTsHccsEndPoint][%s] AicpuTsHccsEndPoint not support host", __func__);
        return HCCL_E_NOT_SUPPORT;
    }

    u32 devPhyId = endpointDesc_.loc.device.devPhyId;
    uint32_t superDevId = endpointDesc_.loc.device.superDevId;
    CHK_RET(GlobalNetDevMgr::GetInstance(endpointDesc_.loc.device.devPhyId).GetDeviceVnicIP(devPhyId, superDevId, devIpAddr_));
    HCCL_INFO("[AicpuTsHccsEndPoint]devPhyId[%u] superDevId[%u] devIpAddr_[%s] ",
        devPhyId, superDevId, devIpAddr_.GetReadableAddress());

    // will not do ServerInit if server_port_ is 0
    CHK_RET(hccl::GlobalNetDevMgr::GetInstance(endpointDesc_.loc.device.devPhyId).RefNetDevCtx(
        NicType::VNIC_TYPE, devIpAddr_, server_port_, netDevCtx_));

    EXECEPTION_CATCH(regedMemMgr_ = std::make_unique<HccsRegedMemMgr>(netDevCtx_), return HCCL_E_PARA);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::ServerSocketListen(const uint32_t port)
{
    CHK_RET(hccl::GlobalNetDevMgr::GetInstance(endpointDesc_.loc.device.devPhyId).ServerSocketListen(netDevCtx_, port));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::ServerSocketStopListen(const uint32_t port)
{
    CHK_RET(hccl::GlobalNetDevMgr::GetInstance(endpointDesc_.loc.device.devPhyId).ServerSocketStopListen(netDevCtx_, port));
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

HcclResult AicpuTsHccsEndPoint::MemoryGrant(const HcommMemGrantInfo *remoteGrantInfo)
{
    CHK_RET(this->regedMemMgr_->MemoryGrant(remoteGrantInfo));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::GetAllMemHandles(void **memHandles, uint32_t *memHandleNum)
{
    CHK_RET(this->regedMemMgr_->GetAllMemHandles(memHandles, memHandleNum));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::GetRemoteIpcRmaBuffer(std::vector<HcclMem> &remoteIpcRmaBufferVec)
{
    HccsRegedMemMgr *hccsRegedMemMgr = dynamic_cast<HccsRegedMemMgr *>(this->regedMemMgr_.get());
    CHK_RET(hccsRegedMemMgr->GetRemoteIpcRmaBuffer(remoteIpcRmaBufferVec));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::GetRemoteIpcRmaBufferEx(std::vector<HcclMemEx> &remoteIpcRmaBufferVecEx)
{
    HccsRegedMemMgr *hccsRegedMemMgr = dynamic_cast<HccsRegedMemMgr *>(this->regedMemMgr_.get());
    CHK_RET(hccsRegedMemMgr->GetRemoteIpcRmaBufferEx(remoteIpcRmaBufferVecEx));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsHccsEndPoint::GetLocalIpcRmaBufferEx(std::vector<HcclMemEx> &localIpcRmaBufferVecEx)
{
    HccsRegedMemMgr *hccsRegedMemMgr = dynamic_cast<HccsRegedMemMgr *>(this->regedMemMgr_.get());
    CHK_RET(hccsRegedMemMgr->GetLocalIpcRmaBufferEx(localIpcRmaBufferVecEx));
    return HCCL_SUCCESS;
}
}