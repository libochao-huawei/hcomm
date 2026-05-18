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
    if (commAddr.type != COMM_ADDR_TYPE_IP_V4 && commAddr.type != COMM_ADDR_TYPE_IP_V6 &&
        commAddr.type != COMM_ADDR_TYPE_EID && commAddr.type != COMM_ADDR_TYPE_MULTI_PORT) {
        HCCL_ERROR("[%s] failed, comm address type[%d] is not supported.", __func__, commAddr.type);
        return HCCL_E_NOT_SUPPORT;
    }

    Hccl::BinaryAddr binAddr;
    int32_t family = AF_INET6;
    if (commAddr.type == COMM_ADDR_TYPE_IP_V4) {
        binAddr.addr = commAddr.addr;
        family = AF_INET;
        ipAddr = Hccl::IpAddress(binAddr, family);
        return HCCL_SUCCESS;
    }

<<<<<<< HEAD
    if (commAddr.type == COMM_ADDR_TYPE_EID) {
=======
    if (commAddr.type == COMM_ADDR_TYPE_EID || commAddr.type == COMM_ADDR_TYPE_MULTI_PORT) {
        const uint8_t *eid = nullptr;
        if (commAddr.type == COMM_ADDR_TYPE_EID) {
            eid = commAddr.eid;
        } else {
            const uint8_t portNum = commAddr.portsAddr.portNum;
            CHK_PRT_RET(portNum == 0 || portNum > HCOMM_NIC_PORT_MAX_NUM,
                HCCL_ERROR("[%s] invalid portNum[%u], max[%u].",
                    __func__, portNum, HCOMM_NIC_PORT_MAX_NUM),
                HCCL_E_PARA);

            // 兼容旧接口：CommAddrToIpAddress只能返回一个IpAddress。
            // MULTI_PORT场景下默认返回PORT0地址；完整多PORT展开由CpuUboeEndpoint::GetIpAddressByPortId处理。
            eid = commAddr.portsAddr.eidList[0];
        }
>>>>>>> 4c169f3f... Uboe 控制面 Endpoint 适配
        Hccl::Eid inputEid;
        s32 sret = memcpy_s(inputEid.raw, sizeof(inputEid.raw), eid, COMM_ADDR_EID_LEN);
        CHK_PRT_RET(sret != EOK, HCCL_ERROR("memcpy failed. errorno[%d]:", sret), HCCL_E_MEMORY);
        ipAddr = Hccl::IpAddress(inputEid);
        return HCCL_SUCCESS;
    }

    if (commAddr.type == COMM_ADDR_TYPE_MULTI_PORT) {
        if (commAddr.portsAddr.family != AF_INET && commAddr.portsAddr.family != AF_INET6) {
            HCCL_ERROR("[%s] failed, ports address family[%d] is not supported.", __func__, commAddr.portsAddr.family);
            return HCCL_E_NOT_SUPPORT;
        }
        
        s32 sret = memcpy_s(&binAddr, sizeof(Hccl::BinaryAddr), &commAddr.portsAddr.linkAddr, sizeof(Hccl::BinaryAddr));
        CHK_PRT_RET(sret != EOK, HCCL_ERROR("memcpy failed. errorno[%d]:", sret), HCCL_E_MEMORY);
        ipAddr = Hccl::IpAddress(binAddr, commAddr.portsAddr.family);
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
        case COMM_PROTOCOL_PCIE:
            linkProtocol = Hccl::LinkProtocol::PCIE;
            break;
        case COMM_PROTOCOL_UBOE:
            linkProtocol = Hccl::LinkProtocol::UBOE;
            break;
        default:
            HCCL_ERROR("[%s] Invalid CommProtocol[%u]", __func__, commProtocol);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult CommAddrTypeToHcclAddressType(CommAddrType commAddrType, HcclAddressType &hcclAddressType)
{
    switch (commAddrType) {
        case COMM_ADDR_TYPE_IP_V4:
            hcclAddressType = HCCL_ADDR_TYPE_IP_V4;
            break;
        case COMM_ADDR_TYPE_IP_V6:
            hcclAddressType = HCCL_ADDR_TYPE_IP_V6;
            break;
        default:
            HCCL_ERROR("[%s] Invaild CommAddrType[%u]", __func__, commAddrType);
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

static HcclResult EndpointLocTypeToPortDeploymentType(const EndpointLocType locType,
    Hccl::PortDeploymentType &deployType)
{
    switch (locType) {
        case EndpointLocType::ENDPOINT_LOC_TYPE_HOST:
            deployType = Hccl::PortDeploymentType::HOST_NET;
            break;
        case EndpointLocType::ENDPOINT_LOC_TYPE_DEVICE:
            deployType = Hccl::PortDeploymentType::DEV_NET;
            break;
        default:
            HCCL_ERROR("[%s] unknown type of EndpointLocType[%d]", __func__, locType);
            return HcclResult::HCCL_E_PARA;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult EndpointDescPairToLinkData(const EndpointDesc &locEp, const EndpointDesc &rmtEp,
    Hccl::LinkData &linkData, u32 reuseIdx)
{
    Hccl::PortDeploymentType portDeploymentType = Hccl::PortDeploymentType::INVALID;
    CHK_RET(EndpointLocTypeToPortDeploymentType(locEp.loc.locType, portDeploymentType));

    Hccl::LinkProtocol linkProtocol = Hccl::LinkProtocol::INVALID;
    CHK_RET(CommProtocolToLinkProtocol(locEp.protocol, linkProtocol));

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(locEp.commAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(rmtEp.commAddr, rmtAddr));

    uint32_t locDevPhyId = locEp.loc.device.devPhyId;
    uint32_t rmtDevPhyId = rmtEp.loc.device.devPhyId;

    // 开源开放架构下comms层级不感知通信域层级的rank信息
    // 当前复用orion数据结构故使用devId替换
    linkData = Hccl::LinkData(
        portDeploymentType,
        linkProtocol, 
        locDevPhyId, rmtDevPhyId,
        locAddr, rmtAddr, reuseIdx
    );

    return HCCL_SUCCESS;
}

HcclResult EndpointDescPairToLinkDataWithRankIds(const uint32_t myRank, const uint32_t rmtRank,
    const EndpointDesc &locEp, const EndpointDesc &rmtEp, Hccl::LinkData &linkData, uint32_t devicePhyId, uint32_t remoteDevicePhyId,
    u32 reuseIdx)
{
    Hccl::PortDeploymentType portDeploymentType = Hccl::PortDeploymentType::INVALID;
    CHK_RET(EndpointLocTypeToPortDeploymentType(locEp.loc.locType, portDeploymentType));

    Hccl::LinkProtocol linkProtocol = Hccl::LinkProtocol::INVALID;
    CHK_RET(CommProtocolToLinkProtocol(locEp.protocol, linkProtocol));

    Hccl::IpAddress locAddr{};
    Hccl::IpAddress rmtAddr{};
    CHK_RET(CommAddrToIpAddress(locEp.commAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(rmtEp.commAddr, rmtAddr));

    // 临时方案，为支持开源开放与orion通信域混跑，复用orion数据结构，添加rank信息
    linkData = Hccl::LinkData(
        portDeploymentType,
        linkProtocol, 
        myRank, rmtRank,
        locAddr, rmtAddr, devicePhyId, remoteDevicePhyId, reuseIdx
    );
    linkData.UpdateIpAddrWithPCIE();

    return HCCL_SUCCESS;
}

} // namespace hcomm