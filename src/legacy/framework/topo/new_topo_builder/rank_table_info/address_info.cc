/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Address header file
 * Create: 2024-12-16
 */
#include "address_info.h"

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

namespace Hccl {
using namespace std;
const unordered_map<string, AddrType> AddressInfo::strToAddrType
    = (unordered_map<string, AddrType>{{"EID", AddrType::EID}, {"IPV4", AddrType::IPV4}, {"IPV6", AddrType::IPV6}});

void AddressInfo::Deserialize(const nlohmann::json &addressInfoJson)
{
    std::string addrTypeStr;
    std::string msgAddrtype = "error occurs when parser object of propName \"addr_type\"";
    TRY_CATCH_THROW(InvalidParamsException, msgAddrtype, addrTypeStr = GetJsonProperty(addressInfoJson, "addr_type"););
    
    if (!IsStringInAddrType(addrTypeStr)) {
        THROW<InvalidParamsException>(StringFormat("[AddressInfo::%s] failed with Invalid addrType. ",
                                                   __func__));
    }
    addrType = strToAddrType.at(addrTypeStr);
    
    std::string address;
    std::string msgAddr = "error occurs when parser object of propName \"addr\"";
    TRY_CATCH_THROW(InvalidParamsException, msgAddr, address = GetJsonProperty(addressInfoJson, "addr"););
   
    if (address.length() < MIN_VALUE_ADDR_LENGRH || address.length() > MAX_VALUE_ADDR_LENGRH) {
        THROW<InvalidParamsException>(StringFormat("addr [%s] length is out of range [%u] to [%u]", address.c_str(), MIN_VALUE_ADDR_LENGRH, MAX_VALUE_ADDR_LENGRH));
    }

    if (addrTypeStr == "IPV4") {
        IPV4ToAddr(address);
    } else if (addrTypeStr == "IPV6") {
        IPV6ToAddr(address);
    } else if (addrTypeStr == "EID"){
        EidToAddr(address);
    }

    planeId=addressInfoJson.value<std::string>("plane_id", "0");
    if (planeId.length() > MAX_VALUE_PLANEID) {
        THROW<InvalidParamsException>(StringFormat("plane_id [%s] length is out of range [%u] to [%u]", planeId.c_str(),MIN_VALUE_PLANEID, MAX_VALUE_PLANEID));
    }

    nlohmann::json portsJsons;
    std::string msgPortlist = "error occurs when parser object of propName \"ports\"";
    TRY_CATCH_THROW(InvalidParamsException, msgPortlist, GetJsonPropertyList(addressInfoJson, "ports", portsJsons););
    for (auto &portsJson : portsJsons) {
    if (portsJson.get<std::string>().size() < MIN_VALUE_PORT_LENGTH || portsJson.get<std::string>().size() > MAX_VALUE_PORT_LENGTH){
        THROW<InvalidParamsException>(StringFormat("portsString [%u] length is out of range [%u] to [%u]", portsJson.get<std::string>().size(), MIN_VALUE_PORT_LENGTH, MAX_VALUE_PORT_LENGTH));
    }
        ports.emplace(portsJson);
    }
    if (ports.size() < MIN_VALUE_PORT || ports.size() > MAX_VALUE_PORT) {
        THROW<InvalidParamsException>(StringFormat("ports [%u] length is out of range [%u] to [%u]", ports.size(), MIN_VALUE_PORT, MAX_VALUE_PORT));
    }
}

void AddressInfo::EidToAddr(std::string address)
{
    if (!IpAddress::IsEID(address)) {
        THROW<InvalidParamsException>(StringFormat("[AddressInfo::%s] failed with rankAddrs is error. ", __func__));
    }
    Eid eid = IpAddress::StrToEID(address);
    IpAddress ipAddress0(eid);
    addr = ipAddress0;
}

void AddressInfo::IPV4ToAddr(std::string address)
{
    s32 ipFamily = AF_INET;

        if (IpAddress::IsIPv4(address)) {
            ipFamily = AF_INET;
        }else {
            THROW<InvalidParamsException>(StringFormat("[AddressInfo::%s] failed with addrs is error. "
                                                       ,__func__));
        }
       IpAddress ipAddress0(address, ipFamily);
       addr=ipAddress0;
}

void AddressInfo::IPV6ToAddr(std::string address)
{
    s32 ipFamily = AF_INET6;

    if (IpAddress::IsIPv6(address)) {
        ipFamily = AF_INET6;
    }else {
        THROW<InvalidParamsException>(StringFormat("[AddressInfo::%s] failed with addr is error. "
                                                    ,__func__));
    }
    IpAddress ipAddress0(address, ipFamily);
    addr=ipAddress0;
}

std::string AddressInfo::Describe() const
{
    return StringFormat("AddressInfo[addrType=%s, addr=%s, planeId=%s,portsize=%u]",
                        addrType.Describe().c_str(), addr.Describe().c_str(), planeId.c_str(),ports.size());
}

AddressInfo::AddressInfo(BinaryStream &binStream)
{
    IpAddress address(binStream);
    addr=address;
    u32 addrTypeInt{0};
    binStream >> addrTypeInt;
    addrType = static_cast<AddrType::Value>(addrTypeInt);
    size_t portsSize{0};
    binStream >> portsSize;
    for (u32 i = 0; i < portsSize; i++) {
        string port;
        binStream>>port;
        ports.emplace(port);
    }
    binStream>>planeId;
}

void AddressInfo::GetBinStream(BinaryStream &binStream) const
{
    if (ports.size() == 0) {
        std::string msg = StringFormat("ports size is zero.");
        THROW<InvalidParamsException>(msg);
    }
    addr.GetBinStream(binStream);
    binStream << static_cast<u32>(addrType);
    binStream << ports.size();
    for (auto &it : ports) {
        binStream<<it;
    }
    binStream<<planeId;
}   

} // namespace Hccl