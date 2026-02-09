/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "comm_addr_logger.h"
#include "hccl_comm_pub.h"
#include <arpa/inet.h>
#include <array>
#include <securec.h>

namespace hcomm {
namespace logger {

std::string CommAddrLogger::GetTypeString(CommAddrType type)
{
    switch (type) {
        case COMM_ADDR_TYPE_IP_V4:
            return "IPv4";
        case COMM_ADDR_TYPE_IP_V6:
            return "IPv6";
        case COMM_ADDR_TYPE_ID:
            return "ID";
        case COMM_ADDR_TYPE_EID:
            return "EID";
        default:
            return "Unknown(" + std::to_string(type) + ")";
    }
}

std::string CommAddrLogger::ConvertIPv4(const struct in_addr& addr)
{
    std::array<char, INET_ADDRSTRLEN> buffer;
    const char* result = inet_ntop(AF_INET, &addr, buffer.data(), buffer.size());
    if (result == nullptr) {
        HCCL_ERROR("[%s] inet_ntop failed for IPv4 address", __func__);
        return "conversion failed";
    }
    return std::string(result);
}

std::string CommAddrLogger::ConvertIPv6(const struct in6_addr& addr6)
{
    std::array<char, INET6_ADDRSTRLEN> buffer;
    const char* result = inet_ntop(AF_INET6, &addr6, buffer.data(), buffer.size());
    if (result == nullptr) {
        HCCL_ERROR("[%s] inet_ntop failed for IPv6 address", __func__);
        return "conversion failed";
    }
    return std::string(result);
}

std::string CommAddrLogger::ConvertID(uint32_t id)
{
    return "id:0x" + std::to_string(id);
}

std::string CommAddrLogger::ConvertEID(const uint8_t eid[16])
{
    char buffer[64];
    int32_t ret = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1,
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        eid[0], eid[1], eid[2], eid[3],
        eid[4], eid[5], eid[6], eid[7],
        eid[8], eid[9], eid[10], eid[11],
        eid[12], eid[13], eid[14], eid[15]);
    if (ret < 0) {
        HCCL_ERROR("[%s] snprintf_s failed for EID", __func__);
        return "conversion failed";
    }
    return std::string(buffer);
}

std::string CommAddrLogger::ToString(const CommAddr& commAddr)
{
    std::string typeStr = GetTypeString(commAddr.type);

    switch (commAddr.type) {
        case COMM_ADDR_TYPE_IP_V4:
            return "[" + typeStr + "] " + ConvertIPv4(commAddr.addr);
        case COMM_ADDR_TYPE_IP_V6:
            return "[" + typeStr + "] " + ConvertIPv6(commAddr.addr6);
        case COMM_ADDR_TYPE_ID:
            return "[" + typeStr + "] " + ConvertID(commAddr.id);
        case COMM_ADDR_TYPE_EID:
            return "[" + typeStr + "] eid:" + ConvertEID(commAddr.eid);
        default:
            return "[" + typeStr + "]";
    }
}

void CommAddrLogger::Print(uint32_t idx, const char* endpointName, const CommAddr& commAddr)
{
    std::string addrStr = ToString(commAddr);
    HCCL_INFO("[%s] channelDescs[%u] %s commAddr: type[%d], addr[%s]",
        __func__, idx, endpointName, commAddr.type, addrStr.c_str());
}

} // namespace logger
} // namespace hcomm
