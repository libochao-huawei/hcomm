/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for the details. You may not use this file except in compliance with the License.
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

HcclResult HcommEndpointMap::AddEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> endpoint)
{
    if (endpoint == nullptr) {
        HCCL_DEBUG("[HcommEndpointMap] AddEndpoint skipped, null endpoint");
        return HCCL_SUCCESS;
    }
    int32_t deviceId = 0;
    (void)hrtGetDevice(&deviceId);
    BaseCommRes* res = BaseCommResMgr::Instance().GetOrCreate(static_cast<uint32_t>(deviceId));
    EndpointsMgr* mgr = res->GetEndpointsMgr();
    CHK_PTR_NULL(mgr);
    return mgr->InstallOwnedEndpoint(handle, std::move(endpoint));
}

HcclResult HcommEndpointMap::RemoveEndpoint(EndpointHandle handle)
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
        return mgr->RemoveOwnedEndpoint(handle);
    });
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HcommEndpointMap] VisitBaseCommRes failed, ret[%d]", ret);
        return ret;
    }
    return HCCL_SUCCESS;
}

HcclResult HcommEndpointMap::UpdateEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> newEndpoint)
{
    if (newEndpoint == nullptr) {
        HCCL_DEBUG("[HcommEndpointMap] UpdateEndpoint skipped, null newEndpoint");
        return HCCL_E_PTR;
    }
    HcclResult ret = RemoveEndpoint(handle);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    return AddEndpoint(handle, std::move(newEndpoint));
}

HcclResult HcommEndpointMap::GetEndpoint(EndpointHandle handle, Endpoint*& outEndpoint)
{
    outEndpoint = nullptr;
    HcclResult ret = BaseCommResMgr::Instance().VisitBaseCommRes([&](BaseCommRes* res) -> HcclResult {
        if (outEndpoint != nullptr) {
            return HCCL_SUCCESS;
        }
        EndpointsMgr* mgr = res->TryGetEndpointsMgr();
        if (mgr == nullptr) {
            return HCCL_SUCCESS;
        }
        return mgr->FindEndpointByHandle(handle, outEndpoint);
    });
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HcommEndpointMap] VisitBaseCommRes failed, ret[%d]", ret);
        return ret;
    }
    return HCCL_SUCCESS;
}

} // namespace hcomm
