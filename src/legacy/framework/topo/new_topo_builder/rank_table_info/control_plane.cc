/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Control Plane
 * Create: 2024-12-18
 */
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include "json_parser.h"
#include "invalid_params_exception.h"
#include "types.h"
#include "const_val.h"
#include "dev_type.h"
#include "exception_util.h"
#include "control_plane.h"
#include "topo_common_types.h"
namespace Hccl {
using namespace std;

const unordered_map<string, AddrType> ControlPlane::strToAddrType
    = (unordered_map<string, AddrType>{{"EID", AddrType::EID}, {"IPV4", AddrType::IPV4}, {"IPV6", AddrType::IPV6}});

void ControlPlane::Deserialize(const nlohmann::json &controlPlaneJson)
{
    string      addrTypeStr;
    std::string msgAddrtype  = "error occurs when parser object of propName \"addr_type\"";
    
    TRY_CATCH_THROW(InvalidParamsException, msgAddrtype, addrTypeStr = GetJsonProperty(controlPlaneJson, "addr_type"););
    
    if (!IsStringInAddrType(addrTypeStr)) {
        THROW<InvalidParamsException>(StringFormat("[ControlPlane::%s] failed with Invalid addrType. ",
                                                   __func__));
    }
    addrType = strToAddrType.at(addrTypeStr);
    
    std::string address;
    std::string msgAddr = "error occurs when parser object of propName \"addr\"";
    TRY_CATCH_THROW(InvalidParamsException, msgAddr, address = GetJsonProperty(controlPlaneJson, "addr"););
   
    if (address.length() < MIN_VALUE_ADDR || address.length() > MAX_VALUE_ADDR) {
        THROW<InvalidParamsException>(StringFormat("addr [%s] length is out of range [%u] to [%u]", address.c_str(), MIN_VALUE_ADDR, MAX_VALUE_ADDR));
    }

    if (addrTypeStr == "IPV4") {
        IPV4ToAddr(address);
    } else if (addrTypeStr == "IPV6") {
        IPV6ToAddr(address);
    } else if (addrTypeStr == "EID"){
        EidToAddr(address);
    }

    std::string msgListenport = "error occurs when parser object of propName \"listen_port\"";
    TRY_CATCH_THROW(InvalidParamsException, msgListenport, listenPort= GetJsonPropertyUInt(controlPlaneJson, "listen_port"););
    
    if (listenPort < MIN_VALUE_LISTENPORT || listenPort> MAX_VALUE_LISTENPORT) {
        THROW<InvalidParamsException>(StringFormat("listen_port [%u] length is out of range [%u] to [%u]", listenPort, MIN_VALUE_LISTENPORT, MAX_VALUE_LISTENPORT));
    }
}

void ControlPlane::EidToAddr(std::string address)
{
    if (!IpAddress::IsEID(address)) {
        THROW<InvalidParamsException>(StringFormat("[ControlPlane::%s] failed with rankAddrs is error. ", __func__));
    }
    Eid eid = IpAddress::StrToEID(address);
    IpAddress ipAddress0(eid);
    addr = ipAddress0;
}

void ControlPlane::IPV4ToAddr(std::string address)
{
    s32 ipFamily = AF_INET;

        if (IpAddress::IsIPv4(address)) {
            ipFamily = AF_INET;
        }else {
            THROW<InvalidParamsException>(StringFormat("[ControlPlane::%s] failed with addrs is error. "
                                                       ,__func__));
        }
       IpAddress ipAddress0(address, ipFamily);
       addr=ipAddress0;
}

void ControlPlane::IPV6ToAddr(std::string address)
{
    s32 ipFamily = AF_INET6;

    if (IpAddress::IsIPv6(address)) {
        ipFamily = AF_INET6;
    }else {
        THROW<InvalidParamsException>(StringFormat("[ControlPlane::%s] failed with addr is error. "
                                                    ,__func__));
    }
    IpAddress ipAddress0(address, ipFamily);
    addr=ipAddress0;
}

std::string ControlPlane::Describe() const
{
    return StringFormat("ControlPlane[addrType=%s, addr=%s, listenPort=%u]",
                        addrType.Describe().c_str(), addr.Describe().c_str(), listenPort);
}

void ControlPlane::GetBinStream(BinaryStream &binStream) const
{
    binStream<<static_cast<u32>(addrType)<<listenPort;
    addr.GetBinStream(binStream);
}

ControlPlane::ControlPlane(BinaryStream &binStream)
{
    u32 addrTypeInt{0};
    binStream >> addrTypeInt;
    addrType = static_cast<AddrType::Value>(addrTypeInt);
    binStream>>listenPort;
    IpAddress address(binStream);
    addr=address;
}

} // namespace Hccl