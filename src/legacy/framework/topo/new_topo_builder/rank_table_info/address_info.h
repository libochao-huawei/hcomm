/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Address header file
 * Create: 2024-12-16
 */
#ifndef ADDRESS_INFO_H
#define ADDRESS_INFO_H

#include <set>
#include <string>
#include "nlohmann/json.hpp"
#include "ip_address.h"
#include "topo_common_types.h"

namespace Hccl {

constexpr unsigned int MIN_VALUE_PLANEID = 0;
constexpr unsigned int MAX_VALUE_PLANEID = 1024;
constexpr unsigned int MIN_VALUE_PORT = 1;
constexpr unsigned int MAX_VALUE_PORT = 16;
constexpr unsigned int MIN_VALUE_PORT_LENGTH = 1;
constexpr unsigned int MAX_VALUE_PORT_LENGTH = 32;
constexpr unsigned int MIN_VALUE_ADDR_LENGRH = 1;
constexpr unsigned int MAX_VALUE_ADDR_LENGRH = 256;

class AddressInfo{
public:
    AddressInfo() {};
    ~AddressInfo() {};
    
    IpAddress                  addr;
    AddrType                   addrType;
    std::set<std::string>      ports;
    std::string                planeId{"0"};
    void                       Deserialize(const nlohmann::json &addressInfoJson);
    explicit                   AddressInfo(BinaryStream &binStream);
    void                       GetBinStream(BinaryStream &binStream) const;
    std::string                Describe() const;

    private:
    static const std::unordered_map<std::string , AddrType> strToAddrType;
    void EidToAddr(std::string str);
    void IPV4ToAddr(std::string str);
    void IPV6ToAddr(std::string str);
    static bool IsStringInAddrType(std::string str)
    {
        return strToAddrType.count(str) > 0;
    }
};

} // namespace Hccl

#endif // ADDRESS_INFO_H