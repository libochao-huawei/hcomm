/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "orion_adpt_utils.h"

// Orion
#include "transport_status.h"
#include "topo_common_types.h"
#include "virtual_topo.h"

namespace hcomm {

HcclResult CommAddrToIpAddress(const CommAddr &commAddr, Hccl::IpAddress &ipAddr)
{
    if (commAddr.type != COMM_ADDR_TYPE_IP_V4 && commAddr.type != COMM_ADDR_TYPE_IP_V6 && commAddr.type != COMM_ADDR_TYPE_EID) {
        HCCL_ERROR("[%s] failed, comm address type[%d] is not supported.", __func__, commAddr.type);
        return HCCL_E_NOT_SUPPORT;
    }

    Hccl::BinaryAddr binAddr;
    int32_t family = AF_INET6;
    if (commAddr.type == COMM_ADDR_TYPE_IP_V4) {
        binAddr.addr = commAddr.addr;
        int32_t family = AF_INET;
        ipAddr = Hccl::IpAddress(binAddr, family);
        return HCCL_SUCCESS;
    }

    if (commAddr.type == COMM_ADDR_TYPE_EID){
        Hccl::Eid inputEid;
        std::memcpy(inputEid.raw, commAddr.eid, Hccl::URMA_EID_LEN);
        ipAddr = Hccl::IpAddress(inputEid);
        return HCCL_SUCCESS;
    }

    binAddr.addr6 = commAddr.addr6;
    ipAddr = Hccl::IpAddress(binAddr, family);
    return HCCL_SUCCESS;
}

HcclResult IpAddressToCommAddr(const Hccl::IpAddress &ipAddr, CommAddr &commAddr)
{
    int32_t family = ipAddr.GetFamily();
    const auto &binAddr = ipAddr.GetBinaryAddress();

    if (family == AF_INET) {
        commAddr.addr = binAddr.addr;
        commAddr.type = COMM_ADDR_TYPE_IP_V4;
        return HcclResult::HCCL_SUCCESS;
    }

    commAddr.addr6 = binAddr.addr6;
    commAddr.type = COMM_ADDR_TYPE_IP_V6;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CommProtocolToLinkProtocol(CommProtocol commProtocol, Hccl::LinkProtocol &linkProtocol)
{
    switch (commProtocol) {
        case COMM_PROTOCOL_UBC_CTP:
            linkProtocol = Hccl::LinkProtocol::UB_CTP;
            break;
        case COMM_PROTOCOL_UBC_TP:
            linkProtocol = Hccl::LinkProtocol::UB_TP;
            break;
        case COMM_PROTOCOL_ROCE:
            linkProtocol = Hccl::LinkProtocol::ROCE;
            break;
        case COMM_PROTOCOL_HCCS:
            linkProtocol = Hccl::LinkProtocol::HCCS;
            break;
        case COMM_PROTOCOL_UB_MEM:
            linkProtocol = Hccl::LinkProtocol::UB_MEM;
            break;
        default:
            HCCL_ERROR("[%s] Invaild CommProtocol[%u]", __func__, commProtocol);
            return HCCL_E_NOT_FOUND;
    }
    return HCCL_SUCCESS;
}

Hccl::LinkData BuildDefaultLinkData()
{
    Hccl::PortDeploymentType portDeploymentType = Hccl::PortDeploymentType::HOST_NET;
    Hccl::LinkProtocol linkProtocol = Hccl::LinkProtocol::ROCE;
    Hccl::IpAddress locAddr;
    Hccl::IpAddress rmtAddr;
    uint32_t locDevPhyId = 0;
    uint32_t rmtDevPhyId = 0;
    return Hccl::LinkData(
        portDeploymentType,
        linkProtocol, 
        locDevPhyId, rmtDevPhyId,
        locAddr, rmtAddr
    );
}

HcclResult EndpointDescPairToLinkData(const EndpointDesc &locEp, const EndpointDesc &rmtEp, Hccl::LinkData &linkData)
{
    // 0) PortDeploymentType: 由 EndpointLocType 推导
    Hccl::PortDeploymentType portDeploymentType = Hccl::PortDeploymentType::DEV_NET;
    switch (locEp.loc.locType) {
        case EndpointLocType::ENDPOINT_LOC_TYPE_HOST:
            portDeploymentType = Hccl::PortDeploymentType::HOST_NET;
            break;
        case EndpointLocType::ENDPOINT_LOC_TYPE_DEVICE:
            portDeploymentType = Hccl::PortDeploymentType::DEV_NET;
            break;
        default:
            // 保留/未知：保持默认 DEV_NET 或者在此处报错
            HCCL_ERROR("[%s] unknown type of EndpointLocType[%d]", __func__, locEp.loc.locType);
            break;
    }

    // 1) LinkProtocol: 由 CommProtocol 推导
    Hccl::LinkProtocol linkProtocol = Hccl::LinkProtocol::ROCE; // 默认值可按需调整
    CommProtocolToLinkProtocol(locEp.protocol, linkProtocol);

    // TODO: client / server 的确定用 IpAddress
    Hccl::IpAddress locAddr;
    Hccl::IpAddress rmtAddr;
    CommAddrToIpAddress(locEp.commAddr, locAddr);
    CommAddrToIpAddress(rmtEp.commAddr, rmtAddr);

    uint32_t locDevPhyId = locEp.loc.device.devPhyId;
    uint32_t rmtDevPhyId = rmtEp.loc.device.devPhyId;

    linkData = Hccl::LinkData(
        portDeploymentType,
        linkProtocol, 
        locDevPhyId, rmtDevPhyId,
        locAddr, rmtAddr
    );

    return HCCL_SUCCESS;
}

} // namespace hcomm