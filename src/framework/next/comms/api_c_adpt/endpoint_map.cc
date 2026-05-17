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
#include "base_comm_res.h"
#include "sub_resource_mgrs.h"
#include "adapter_hal_pub.h"

namespace hcomm {

void HcommEndpointMap::AddEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> endpoint)
{
    if (endpoint == nullptr) {
        return;
    }
    int32_t deviceId = 0;
    (void)hrtGetDevice(&deviceId);
    BaseCommRes* res = BaseCommResMgr::Instance().GetOrCreate(static_cast<uint32_t>(deviceId));
    EndpointsMgr* mgr = res->GetEndpointsMgr();
    mgr->InstallOwnedEndpoint(handle, std::move(endpoint));
}

bool HcommEndpointMap::RemoveEndpoint(EndpointHandle handle)
{
    bool removed = false;
    HcclResult ret = BaseCommResMgr::Instance().VisitBaseCommRes([&](BaseCommRes* res) -> HcclResult {
        if (removed) {
            return HCCL_SUCCESS;
        }
        EndpointsMgr* mgr = res->TryGetEndpointsMgr();
        if (mgr == nullptr) {
            return HCCL_SUCCESS;
        }
        if (mgr->RemoveOwnedEndpoint(handle) == HCCL_SUCCESS) {
            removed = true;
        }
        return HCCL_SUCCESS;
    });
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HcommEndpointMap][%s] VisitBaseCommRes failed, ret=%d", __func__, ret);
        return false;
    }
    return removed;
}

bool HcommEndpointMap::UpdateEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> newEndpoint)
{
    if (newEndpoint == nullptr) {
        return false;
    }
    if (!RemoveEndpoint(handle)) {
        return false;
    }
    AddEndpoint(handle, std::move(newEndpoint));
    return true;
}

Endpoint* HcommEndpointMap::GetEndpoint(EndpointHandle handle)
{
    Endpoint* found = nullptr;
    HcclResult ret = BaseCommResMgr::Instance().VisitBaseCommRes([&](BaseCommRes* res) -> HcclResult {
        if (found != nullptr) {
            return HCCL_SUCCESS;
        }
        EndpointsMgr* mgr = res->TryGetEndpointsMgr();
        if (mgr == nullptr) {
            return HCCL_SUCCESS;
        }
        found = mgr->FindEndpointByHandle(handle);
        return HCCL_SUCCESS;
    });
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HcommEndpointMap][%s] VisitBaseCommRes failed, ret=%d", __func__, ret);
        return nullptr;
    }
    return found;
}

} // namespace hcomm
