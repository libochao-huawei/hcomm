/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <algorithm>
#include "log.h"
#include "hccl_ip_address.h"
#include <regex>
#include <log.h>
#include "hccn_rping.h"
#include "../../../legacy/common/utils/string_util.h"
namespace hccl {

HcclResult HcclIpAddress::SetBianryAddress(s32 family, const union HcclInAddr &address)
{
    char buf[IP_ADDRESS_BUFFER_LEN] = {0};
    if (inet_ntop(family, &address, buf, sizeof(buf)) == nullptr) {
        if (family == AF_INET) {
            HCCL_ERROR("ip addr[0x%08x] is invalid IPv4 address.", address.addr.s_addr);
        } else {
            HCCL_ERROR("ip addr[%08x %08x %08x %08x] is invalid IPv6 address.",
                address.addr6.s6_addr32[0],  // 打印ipv6地址中的 word 0
                address.addr6.s6_addr32[1],  // 打印ipv6地址中的 word 1
                address.addr6.s6_addr32[2],  // 打印ipv6地址中的 word 2
                address.addr6.s6_addr32[3]); // 打印ipv6地址中的 word 3
        }
        return HCCL_E_PARA;
    } else {
        this->family = family;
        this->binaryAddr = address;
        this->readableIP = buf;
        this->readableAddr = this->readableIP;
        return HCCL_SUCCESS;
    }
}

HcclResult HcclIpAddress::SetReadableAddress(const std::string &address)
{
    CHK_PRT_RET(address.empty(), HCCL_ERROR("ip addr is null."), HCCL_E_PARA);

    std::size_t found = address.find("%");
    if ((found == 0) || (found == (address.length() - 1))) {
        HCCL_ERROR("addr[%s] is invalid.", address.c_str());
        return HCCL_E_PARA;
    }
    std::string ipStr = address.substr(0, found);
    int cnt = std::count(ipStr.begin(), ipStr.end(), ':');
    if (cnt >= 2) { // ipv6地址中至少有2个":"
        if (inet_pton(AF_INET6, ipStr.c_str(), &binaryAddr.addr6) <= 0) {
            HCCL_ERROR("ip addr[%s] is invalid IPv6 address.", ipStr.c_str());
            binaryAddr.addr6.s6_addr32[0] = 0; // 清空ipv6地址中的 word 0
            binaryAddr.addr6.s6_addr32[1] = 0; // 清空ipv6地址中的 word 1
            binaryAddr.addr6.s6_addr32[2] = 0; // 清空ipv6地址中的 word 2
            binaryAddr.addr6.s6_addr32[3] = 0; // 清空ipv6地址中的 word 3
            clear();
            return HCCL_E_PARA;
        }
        this->family = AF_INET6;
    } else {
        if (inet_pton(AF_INET, ipStr.c_str(), &binaryAddr.addr) <= 0) {
            HCCL_ERROR("ip addr[%s] is invalid IPv4 address.", ipStr.c_str());
            clear();
            return HCCL_E_PARA;
        }
        this->family = AF_INET;
    }
    if (found != std::string::npos) {
        this->ifname = address.substr(found + 1);
    }
    this->readableIP = ipStr;
    this->readableAddr = address;
    return HCCL_SUCCESS;
}

HcclResult HcclIpAddress::SetIfName(const std::string &name)
{
    CHK_PRT_RET(name.empty(), HCCL_ERROR("if name is null."), HCCL_E_PARA);

    std::size_t found = readableAddr.find("%");
    if (found == std::string::npos) {
        ifname = name;
        readableAddr.append("%");
        readableAddr.append(ifname);
    } else {
        HCCL_ERROR("addr[%s] ifname has existed.", readableAddr.c_str());
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

std::string  HcclIpAddress::Describe() const
{
    std::string desc = Hccl::StringFormat("IpAddress[%s, ", eid.Describe().c_str());
    
    if (family == AF_INET) {
        desc += Hccl::StringFormat("AF=v4, addr=%s]", GetIpStr().c_str());
    } else {
        desc += Hccl::StringFormat("AF=v6, addr=%s, scopeId=0x%x]", GetIpStr().c_str(), scopeID);
    }
    return desc;
}

HcclIpAddress::HcclIpAddress(const Eid &eidInput)
{
    for (uint32_t i = 0; i < URMA_EID_LEN; i++) {
        eid.raw[i] = eidInput.raw[i];
    }

    HCCL_INFO("[IpAddress] %s", eid.Describe().c_str());
    // IPoURMA适配后，使用EID初始化时转为ipv6建链
    this->family = AF_INET6;
    (void)memcpy_s(binaryAddr.addr6.s6_addr, sizeof(eid.raw), eid.raw, sizeof(eid.raw));  
    (void)SetBianryAddress(family, binaryAddr);
}

bool HcclIpAddress::IsEID(const std::string& str)
{
    if (str.length() == URMA_EID_LEN * URMA_EID_NUM_TWO) {
        std::regex hexCharsRegex("[0-9a-fA-F]+");
        return std::regex_match(str, hexCharsRegex);
    }
    return false;
}

Eid HcclIpAddress::StrToEID(const std::string& str)
{
    Eid tmpeEid{};
    const int Base = 16;
    for (size_t i = 0; i < URMA_EID_LEN; ++i) {
        std::string byteString = str.substr(i * 2, 2);
        tmpeEid.raw[i] = static_cast<uint8_t>(std::stoi(byteString, nullptr, Base));
    }
    return tmpeEid;
}
std::string HcclIpAddress::GetIpStr() const
{
    const void *src = nullptr;
    if (family == AF_INET) {
        src = &binaryAddr.addr;
    } else if (family == AF_INET6) {
        src = &binaryAddr.addr6;
    } 
    char dst[INET6_ADDRSTRLEN];
    const char *res = inet_ntop(family, src, dst, INET6_ADDRSTRLEN);
    if (res == nullptr) {
        // 转换失败处理：返回空字符串或抛异常
        return "";  // 示例
    }
    return dst;
}

std::string Eid::Describe() const
{
    return Hccl::StringFormat("eid[%016llx:%016llx]",
                        static_cast<unsigned long long>(be64toh(in6.subnetPrefix)),
                        static_cast<unsigned long long>(be64toh(in6.interfaceId)));
    }

}
