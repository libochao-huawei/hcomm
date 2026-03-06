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
 #include "cpu_roce_endpoint.h"
 #include "urma_endpoint.h"
 #include "ub_mem_endpoint.h"

 namespace hcomm{
static bool IsProtocolSupported(CommProtocol protocol)
{
    switch (protocol) {
        case COMM_PROTOCOL_ROCE:
        case COMM_PROTOCOL_UBC_TP:
        case COMM_PROTOCOL_UBC_CTP:
        case COMM_PROTOCOL_UB_MEM:
            return true;
        default:
            return false;
    }
}

Endpoint::Endpoint(const EndpointDesc &endpointDesc)
{
    endpointDesc_ = endpointDesc;
}

HcclResult Endpoint::CreateEndpoint(const EndpointDesc &endpointDesc, std::unique_ptr<Endpoint> &endpointPtr)
{
    if (endpointDesc.loc.locType != ENDPOINT_LOC_TYPE_DEVICE && endpointDesc.loc.locType != ENDPOINT_LOC_TYPE_HOST) {
        HCCL_ERROR("[%s] endpointDesc.loc.locType [%d] is not supported.", __func__, endpointDesc.loc.locType);
        return HCCL_E_PARA;
    }

    if (!IsProtocolSupported(endpointDesc.protocol)){
        HCCL_ERROR("[%s]endpointDesc.protocol [%d] is not supported.", __func__, endpointDesc.protocol);
    }

    if (endpointDesc.protocol == COMM_PROTOCOL_ROCE && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_HOST) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<CpuRoceEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_UBC_TP && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<UrmaEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_UBC_CTP && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<UrmaEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else if (endpointDesc.protocol == COMM_PROTOCOL_UB_MEM && endpointDesc.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        EXECEPTION_CATCH(endpointPtr = std::make_unique<UbMemEndpoint>(endpointDesc), return HCCL_E_PTR);
    } else {
        endpointPtr = nullptr;
        HCCL_ERROR("[%s] failed, endpointDesc.protocol [%d] and endpointDesc.loc.locType [%d] do not match.", 
            __func__, endpointDesc.protocol, endpointDesc.loc.locType);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

 }