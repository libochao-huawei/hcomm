/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "endpoint_map.h"
#include "base_comm_res_mgr.h"
#include "sub_resource_mgrs.h"
#include "hccl_common.h"

namespace hcomm {

// 从EndpointDesc中获取设备物理ID
static inline uint32_t GetDevPhyIdFromEndpoint(const EndpointDesc& desc)
{
    if (desc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        uint32_t devPhyId = desc.loc.device.devPhyId;
        // 如果设备物理ID超出范围，返回备份设备ID
        if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
            HCCL_WARNING("[hcomm][%s] devPhyId[%u] >= MAX_MODULE_DEVICE_NUM[%u], using backup slot",
                __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
            return MAX_MODULE_DEVICE_NUM;
        }
        return devPhyId;
    }
    return MAX_MODULE_DEVICE_NUM;
}

void HcommEndpointMap::AddEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> endpoint)
{
    if (endpoint == nullptr) {
        HCCL_ERROR("[HcommEndpointMap][%s] endpoint is null", __func__);
        return;
    }

    // 从EndpointDesc中获取设备物理ID
    EndpointDesc desc = endpoint->GetEndpointDesc();
    uint32_t devPhyId = GetDevPhyIdFromEndpoint(desc);

    BaseCommResMgr& mgr = BaseCommResMgr::Instance();
    BaseCommRes* baseCommRes = mgr.GetOrCreate(devPhyId);
    if (baseCommRes == nullptr) {
        HCCL_ERROR("[HcommEndpointMap][%s] GetOrCreate failed for devPhyId=%u", __func__, devPhyId);
        return;
    }

    EndpointsMgr* endpointsMgr = baseCommRes->GetEndpointsMgr();
    if (endpointsMgr == nullptr) {
        HCCL_ERROR("[HcommEndpointMap][%s] GetEndpointsMgr failed for devPhyId=%u", __func__, devPhyId);
        return;
    }

    endpointsMgr->InstallOwnedEndpoint(handle, std::move(endpoint));
}

bool HcommEndpointMap::RemoveEndpoint(EndpointHandle handle)
{
    BaseCommResMgr& mgr = BaseCommResMgr::Instance();
    Endpoint* endpoint = nullptr;

    mgr.VisitBaseCommRes([&](BaseCommRes* baseCommRes) {
        EndpointsMgr* endpointsMgr = baseCommRes->TryGetEndpointsMgr();
        if (endpointsMgr == nullptr) {
            return;
        }
        Endpoint* ep = endpointsMgr->FindEndpointByHandle(handle);
        if (ep != nullptr) {
            endpoint = ep;
        }
    });

    if (endpoint == nullptr) {
        HCCL_WARNING("[HcommEndpointMap][%s] endpoint not found, handle=%p", __func__, handle);
        return false;
    }

    EndpointDesc desc = endpoint->GetEndpointDesc();
    uint32_t devPhyId = GetDevPhyIdFromEndpoint(desc);

    BaseCommRes* baseCommRes = mgr.GetBaseCommRes(devPhyId);
    if (baseCommRes == nullptr) {
        HCCL_ERROR("[HcommEndpointMap][%s] GetBaseCommRes failed for devPhyId=%u", __func__, devPhyId);
        return false;
    }

    EndpointsMgr* endpointsMgr = baseCommRes->GetEndpointsMgr();
    if (endpointsMgr == nullptr) {
        HCCL_ERROR("[HcommEndpointMap][%s] GetEndpointsMgr failed for devPhyId=%u", __func__, devPhyId);
        return false;
    }

    auto ret = endpointsMgr->RemoveOwnedEndpoint(handle);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HcommEndpointMap][%s] RemoveOwnedEndpoint failed, handle=%p", __func__, handle);
        return false;
    }

    return true;
}

bool HcommEndpointMap::UpdateEndpoint(
    EndpointHandle handle,
    std::unique_ptr<Endpoint> newEndpoint)
{
    (void)handle;
    (void)newEndpoint;
    return false;
}

Endpoint* HcommEndpointMap::GetEndpoint(EndpointHandle handle)
{
    BaseCommResMgr& mgr = BaseCommResMgr::Instance();
    Endpoint* endpoint = nullptr;

    mgr.VisitBaseCommRes([&](BaseCommRes* baseCommRes) {
        if (endpoint != nullptr) {
            return;
        }
        EndpointsMgr* endpointsMgr = baseCommRes->TryGetEndpointsMgr();
        if (endpointsMgr == nullptr) {
            return;
        }
        endpoint = endpointsMgr->FindEndpointByHandle(handle);
    });

    return endpoint;
}

} // namespace hcomm