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
#include "comm_addr_convert.h"
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

std::string CommAddrLogger::ToString(const CommAddr& commAddr)
{
    // 模仿 IpAddress::Describe 的格式
    // 输出：IpAddress[eid[xxxxxxxxxxxxxxxx:xxxxxxxxxxxxxxxx], AF=v4/v6, addr=xxx.xxx.xxx.xxx, scopeId=0x0]
    std::string desc = "IpAddress[";

    switch (commAddr.type) {
        case COMM_ADDR_TYPE_IP_V4:
            desc += ConvertEID(commAddr.eid) + ", AF=v4, addr=" + ConvertIPv4(commAddr.addr) + "]";
            break;
        case COMM_ADDR_TYPE_IP_V6:
            desc += ConvertEID(commAddr.eid) + ", AF=v6, addr=" + ConvertIPv6(commAddr.addr6) + ", scopeId=0x0]";
            break;
        case COMM_ADDR_TYPE_ID:
            desc += "id:" + ConvertID(commAddr.id) + "]";
            break;
        case COMM_ADDR_TYPE_EID:
            desc += ConvertEID(commAddr.eid) + "]";
            break;
        default:
            desc += "Unknown]";
            break;
    }

    return desc;
}

void CommAddrLogger::Print(uint32_t idx, const char* endpointName, const CommAddr& commAddr)
{
    std::string addrStr = ToString(commAddr);
    HCCL_INFO("[%s] channelDescs[%u] %s commAddr: type[%d], addr[%s]",
        __func__, idx, endpointName, commAddr.type, addrStr.c_str());
}

} // namespace logger
} // namespace hcomm
