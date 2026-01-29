/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: NewRankTable header file 
 * Create: 2024-12-16
 */

#ifndef CONTROL_PLANE_H
#define CONTROL_PLANE_H

#include <string>
#include "nlohmann/json.hpp"
#include "ip_address.h"
#include "nlohmann/json.hpp"
#include "topo_common_types.h"


namespace Hccl {
constexpr unsigned int MIN_VALUE_LISTENPORT = 1;
constexpr unsigned int MAX_VALUE_LISTENPORT = 65536;
constexpr unsigned int MIN_VALUE_ADDR = 1;
constexpr unsigned int MAX_VALUE_ADDR = 256;
class ControlPlane{
public:
    ControlPlane(){};
    AddrType                   addrType;
    IpAddress                  addr;
    u32                        listenPort{0};
    void                       Deserialize(const nlohmann::json &controlPlaneJson);
    void                       GetBinStream(BinaryStream& binaryStream) const;
    explicit                   ControlPlane(BinaryStream &binStream);
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

#endif // CONTROL_PLANE_H