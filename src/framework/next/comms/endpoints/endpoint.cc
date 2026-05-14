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
#include "endpoint.h"
#include "aicpu_ts_roce_endpoint.h"
#include "cpu_roce_endpoint.h"
#include "urma_endpoint.h"
#include "ub_mem_endpoint.h"
#include "uboe_endpoint.h"
#include "cpu_urma_endpoint.h"
#include "aicputs_hccs_endpoint.h"
#include "../../../../platform/hccp/inc/private/network/ra_rs_comm.h"

namespace hcomm{
static bool IsSupported(const EndpointDesc &endpointDesc)
{
    bool protocolSupported = false;
    bool locTypeSupported = false;
    switch (endpointDesc.protocol) {
        case COMM_PROTOCOL_ROCE:
        case COMM_PROTOCOL_UBC_TP:
        case COMM_PROTOCOL_UBC_CTP:
        case COMM_PROTOCOL_UB_MEM:
        case COMM_PROTOCOL_PCIE:
        case COMM_PROTOCOL_UBOE:
        case COMM_PROTOCOL_HCCS:
            protocolSupported = true;
            break;
        default:
            return false;
    }
    switch (endpointDesc.loc.locType) {
        case ENDPOINT_LOC_TYPE_DEVICE:
        case ENDPOINT_LOC_TYPE_HOST:
            locTypeSupported = true;
            break;
        default:
            return false;
    }

    return protocolSupported && locTypeSupported;
}

Endpoint::Endpoint(const EndpointDesc &endpointDesc)
{
    endpointDesc_ = endpointDesc;
}

HcclResult Endpoint::CreateEndpoint(const EndpointDesc &endpointDesc, std::unique_ptr<Endpoint> &endpointPtr)
{
    if (!IsSupported(endpointDesc)) {
        HCCL_ERROR("[%s]endpointDesc is not supported. endpointDesc.protocol [%d] endpointDesc.loc.locType [%d].", __func__, endpointDesc.protocol, endpointDesc.loc.locType);
        return HCCL_E_PARA;
    }

    HCCL_INFO("[%s]endpointDesc.protocol [%d] endpointDesc.loc.locType [%d].", __func__, endpointDesc.protocol, endpointDesc.loc.locType);

    return CreateEndpointBase(endpointDesc, endpointPtr);
}

HcclResult Endpoint::CreateEndpointBase(const EndpointDesc &endpointDesc, std::unique_ptr<Endpoint> &endpointPtr)
{
if (endpointDesc.protocol == COMM_PROTOCOL_ROCE && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_HOST) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<CpuRoceEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if ((endpointDesc.protocol == COMM_PROTOCOL_UBC_TP || endpointDesc.protocol == COMM_PROTOCOL_UBC_CTP) 	 
                && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_HOST) { 
        EXECEPTION_CATCH(endpointPtr = std::make_unique<CpuUrmaEndpoint>(endpointDesc), return HCCL_E_PTR);	 
    } else if ((endpointDesc.protocol == COMM_PROTOCOL_UBC_TP || endpointDesc.protocol == COMM_PROTOCOL_UBC_CTP)	 
                 && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<UrmaEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_UB_MEM && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<UbMemEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_PCIE && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<UbMemEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_UBOE && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<UboeEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_ROCE && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<AicpuTsRoceEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_HCCS && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<AicpuTsHccsEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else {
        endpointPtr = nullptr;
        HCCL_ERROR("[%s] failed, endpointDesc.protocol [%d] and endpointDesc.loc.locType [%d] do not match.", 
            __func__, endpointDesc.protocol, endpointDesc.loc.locType);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult Endpoint::GetAsyncEventsContext(uint32_t devPhyId, struct AsyncEvent events[], uint32_t &num)
{
    uint32_t interfaceVersion{0};

    int ret;
    // 对RaCtxGetAsyncEvents接口的版本检验
    ret = RaGetInterfaceVersion(devPhyId, RA_RS_CTX_GET_ASYNC_EVENTS, &interfaceVersion);
    if (ret != 0) {
        HCCL_ERROR("[%s] devPhyId[%u] RaGetInterfaceVersion failed, ret[%d]", __func__, devPhyId, ret);
        return HCCL_E_INTERNAL; 
    }

    if (interfaceVersion <= 1) {
        HCCL_ERROR("[%s] devPhyId[%u] version[%u] not support", __func__, devPhyId, interfaceVersion);
        return HCCL_E_NOT_SUPPORT;
    }

    // num = 0;
    // return HCCL_SUCCESS;
    CHK_PTR_NULL(ctxHandle_);
    HCCL_INFO("[EndpointMonitor][%s] devPhyId[%u] events[%p] num[%d]", __func__, devPhyId, events, num);
    ret = RaCtxGetAsyncEvents(ctxHandle_, events, &num);
    if (ret != 0) {
        HCCL_ERROR("[%s] devPhyId[%u] RaCtxGetAsyncEvents failed, ctxHandle[%p] ret[%d]", __func__, devPhyId,
            (void *)ctxHandle_, ret);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}
}