/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "endpoint.h"
#include <functional>
#include "aicpu_ts_roce_endpoint.h"
#include "cpu_roce_endpoint.h"
#include "urma_endpoint.h"
#include "ub_mem_endpoint.h"
#include "uboe_endpoint.h"
#include "ubg_endpoint.h"
#include "cpu_urma_endpoint.h"
#include "aicputs_hccs_endpoint.h"
#include "hccp_nda.h"
#include "adapter_rts_common.h"

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
        case COMM_PROTOCOL_UBG:
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
    using EndpointCreator = std::function<std::unique_ptr<Endpoint>(const EndpointDesc &)>;
    struct Entry {
        CommProtocol protocol;
        EndpointLocType locType;
        EndpointCreator creator;
    };
    static const Entry table[] = {
        {COMM_PROTOCOL_ROCE,    ENDPOINT_LOC_TYPE_HOST,   [](const EndpointDesc &d) { return std::make_unique<CpuRoceEndpoint>(d); }},
        {COMM_PROTOCOL_UBC_TP,  ENDPOINT_LOC_TYPE_HOST,   [](const EndpointDesc &d) { return std::make_unique<CpuUrmaEndpoint>(d); }},
        {COMM_PROTOCOL_UBC_CTP, ENDPOINT_LOC_TYPE_HOST,   [](const EndpointDesc &d) { return std::make_unique<CpuUrmaEndpoint>(d); }},
        {COMM_PROTOCOL_UBC_TP,  ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<UrmaEndpoint>(d); }},
        {COMM_PROTOCOL_UBC_CTP, ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<UrmaEndpoint>(d); }},
        {COMM_PROTOCOL_UB_MEM,  ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<UbMemEndpoint>(d); }},
        {COMM_PROTOCOL_PCIE,    ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<UbMemEndpoint>(d); }},
        {COMM_PROTOCOL_UBOE,    ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<UboeEndpoint>(d); }},
        {COMM_PROTOCOL_UBG,     ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<UbgEndpoint>(d); }},
        {COMM_PROTOCOL_ROCE,    ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<AicpuTsRoceEndpoint>(d); }},
        {COMM_PROTOCOL_HCCS,    ENDPOINT_LOC_TYPE_DEVICE, [](const EndpointDesc &d) { return std::make_unique<AicpuTsHccsEndpoint>(d); }},
    };

    for (const auto &entry : table) {
        if (entry.protocol == endpointDesc.protocol && entry.locType == endpointDesc.loc.locType) {
            EXCEPTION_CATCH(endpointPtr = entry.creator(endpointDesc), return HCCL_E_PTR);
            return HCCL_SUCCESS;
        }
    }

    HCCL_ERROR("[%s] failed, endpointDesc.protocol [%d] and endpointDesc.loc.locType [%d] do not match.",
        __func__, endpointDesc.protocol, endpointDesc.loc.locType);
    return HCCL_E_PARA;
}

HcclResult Endpoint::CheckFeature(const EndpointDesc &endpointDesc, HcommEndpointFeatureType featureType, bool &value)
{
    if (featureType == HCOMM_ENDPOINT_FEATURE_NDA) {
        if (endpointDesc.protocol != COMM_PROTOCOL_ROCE || endpointDesc.loc.locType != ENDPOINT_LOC_TYPE_HOST) {
            HCCL_WARNING("[%s] not support NDA, protocol[%d], locType[%d]",
                __func__, endpointDesc.protocol, endpointDesc.loc.locType);
            value = false;
            return HCCL_SUCCESS;
        }

        Hccl::IpAddress ipAddr{};
        CHK_RET(CommAddrToIpAddress(endpointDesc.commAddr, ipAddr));
        s32 devId = 0;
        CHK_RET(hrtGetDevice(&devId));
        u32 devPhyId = 0;
        CHK_RET(hrtGetDevicePhyIdByIndex(devId, devPhyId));

        auto &rdmaHandleMgr = Hccl::RdmaHandleManager::GetInstance();
        void *rdmaHandle = static_cast<void *>(
            rdmaHandleMgr.GetByAddr(devPhyId, Hccl::LinkProtoType::RDMA, ipAddr, Hccl::PortDeploymentType::HOST_NET));
        CHK_PTR_NULL(rdmaHandle);

        s32 directFlag = 0;
        s32 ret = RaNdaGetDirectFlag(rdmaHandle, &directFlag);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] failed to get directFlag, ret[%d]", __func__, ret), HCCL_E_INTERNAL);
        value = (directFlag != DIRECT_FLAG_NOTSUPP);
        HCCL_INFO("[%s] %s NDA, rdmaHandle[%p], directFlag[%d]",
            __func__, value ? "support" : "not support", rdmaHandle, directFlag);
    } else {
        HCCL_WARNING("[%s] unsupported featureType[%d]", __func__, featureType);
        value = false;
    }

    return HCCL_SUCCESS;
}
}