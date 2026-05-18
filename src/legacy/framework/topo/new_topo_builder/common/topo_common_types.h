/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TOPO_COMMON_TYPES_H
#define TOPO_COMMON_TYPES_H

#include <cstdint>
#include <cstring>
#include <netinet/in.h>

#include "hccl_res.h"
#include "enum_factory.h"
#include "hccl/base.h"
#include "hccl_rank_graph.h"
namespace Hccl {
using NetPlaneId = u32;
using LocalId = u32;
using NodeId = u64;
using DeviceId = u32;
using PlaneId = std::string;
using FabricId = u32;

MAKE_ENUM(LinkDirection, BOTH, SEND_ONLY, RECV_ONLY)
MAKE_ENUM(LinkProtocol, UB_CTP, UB_TP, ROCE, HCCS, TCP, UB_MEM, PCIE, UBOE)
MAKE_ENUM(LinkPortType, PEER, BACKUP_PLANE, NET_PLANE)
MAKE_ENUM(LinkType, PEER2PEER, PEER2NET, PEER2BACKUP)
MAKE_ENUM(AddrPosition, HOST, DEVICE)
MAKE_ENUM(AddrType, EID, IPV4, IPV6)
MAKE_ENUM(NetType, CLOS, MESH_1D, MESH_2D, A3_SERVER, A2_AX_SERVER, TOPO_FILE_DESC)
MAKE_ENUM(TopoType, CLOS, MESH_1D, MESH_2D, A3_SERVER, A2_AX_SERVER, TOPO_TYPE_RESERVED)
constexpr LocalId BACKUP_LOCAL_ID = 64;
} // namespace Hccl

inline bool operator==(const MultiPortAddr &lhs, const MultiPortAddr &rhs)
{
    if (lhs.portNum != rhs.portNum) {
        return false;
    }
    if (lhs.family != rhs.family) {
        return false;
    }
    if (lhs.family == AF_INET) {
        if (memcmp(&lhs.linkAddr.addr, &rhs.linkAddr.addr, sizeof(lhs.linkAddr.addr)) != 0) {
            return false;
        }
    } else {
        if (memcmp(&lhs.linkAddr.addr6, &rhs.linkAddr.addr6, sizeof(lhs.linkAddr.addr6)) != 0) {
            return false;
        }
    }
    return memcmp(lhs.eidList, rhs.eidList, static_cast<size_t>(lhs.portNum) * COMM_ADDR_EID_LEN) == 0;
}

inline bool operator==(const CommAddr& lhs, const CommAddr& rhs) {
    // 类型不同，直接不等
    if (lhs.type != rhs.type) {
        return false;
    }

    // 类型相同，根据类型比较具体数据
    switch (lhs.type) {
        case COMM_ADDR_TYPE_IP_V4:
            return lhs.addr.s_addr == rhs.addr.s_addr;

        case COMM_ADDR_TYPE_IP_V6:
            // 比较 IPv6 地址的 16 字节
            return memcmp(lhs.addr6.s6_addr, rhs.addr6.s6_addr, sizeof(lhs.addr6.s6_addr)) == 0;

        case COMM_ADDR_TYPE_ID:
            return lhs.id == rhs.id;

        case COMM_ADDR_TYPE_EID:
            // 假设 COMM_ADDR_EID_LEN 是一个常量，比如 16
            return memcmp(lhs.eid, rhs.eid, COMM_ADDR_EID_LEN) == 0;

        case COMM_ADDR_TYPE_MULTI_PORT: {
            if (lhs.portsAddr.portNum != rhs.portsAddr.portNum) {
                return false;
            }
            if (lhs.portsAddr.portNum == 0 || lhs.portsAddr.portNum > HCOMM_NIC_PORT_MAX_NUM) {
                return false;
            }
            return memcmp(lhs.portsAddr.eidList, rhs.portsAddr.eidList,
                static_cast<size_t>(lhs.portsAddr.portNum) * COMM_ADDR_EID_LEN) == 0 &&
                memcmp(&lhs.portsAddr.addr6, &rhs.portsAddr.addr6, sizeof(lhs.portsAddr.addr6)) == 0;
        }

        case COMM_ADDR_TYPE_RESERVED:
        default:
            return true; 
    }
}

namespace std {
template <>
struct hash<Hccl::LinkProtocol> {
    size_t operator()(const Hccl::LinkProtocol& k) const noexcept
    {
        return static_cast<std::size_t>(k);
    }
};
template <>
struct hash<Hccl::NetType> {
    size_t operator()(const Hccl::NetType& k) const noexcept
    {
        return static_cast<std::size_t>(k);
    }
};
template <>
struct hash<Hccl::TopoType> {
    size_t operator()(const Hccl::TopoType& k) const noexcept
    {
        return static_cast<std::size_t>(k);
    }
};

template <>
struct hash<CommProtocol> {
    size_t operator()(const CommProtocol& protocol) const
    {
        return static_cast<size_t>(protocol);
    }
};

template <>
struct hash<MultiPortAddr> {
    size_t operator()(const MultiPortAddr& addr) const noexcept
    {
        constexpr uint8_t BITS_8 = 8;
        uint8_t LEFT_SHIFT_BITS = BITS_8;
        size_t h = 0;
        h = h ^ static_cast<size_t>(addr.portNum);
        h = h ^ (static_cast<size_t>(addr.family) << LEFT_SHIFT_BITS);
        if (addr.family == AF_INET) {
            LEFT_SHIFT_BITS += BITS_8;
            h = h ^ (static_cast<size_t>(addr.linkAddr.addr.s_addr) << LEFT_SHIFT_BITS);
        } else {
            const auto *addrBytes = reinterpret_cast<const uint8_t *>(&addr.linkAddr.addr6);
            for (u32 i = 0; i < sizeof(addr.linkAddr.addr6); ++i) {
                h = h ^ (static_cast<size_t>(addrBytes[i]) << (((i + 1) % BITS_8) + LEFT_SHIFT_BITS));
            }
            LEFT_SHIFT_BITS += BITS_8;
        }
        for (uint8_t idx = 0; idx < addr.portNum && idx < HCOMM_NIC_PORT_MAX_NUM; ++idx) {
            for (u32 i = 0; i < COMM_ADDR_EID_LEN; ++i) {
                h = h ^ (static_cast<size_t>(addr.eidList[idx][i]) << (((i + 1) % BITS_8) + LEFT_SHIFT_BITS));
            }
            LEFT_SHIFT_BITS += BITS_8;
        }
        return h;
    }
};

template <>
struct hash<CommAddr> {
    size_t operator()(const CommAddr& commAddr) const noexcept
    {
        size_t h = 0;
        h = h ^ static_cast<size_t>(commAddr.type);
        switch (commAddr.type) {
            case COMM_ADDR_TYPE_EID: {
                for (u32 i = 0; i < COMM_ADDR_EID_LEN && i < sizeof(commAddr.eid); ++i) {
                    h = h ^ (static_cast<size_t>(commAddr.eid[i]) << ((i % sizeof(size_t)) * 8));
                }
                break;
            }
            case COMM_ADDR_TYPE_IP_V4: {
                // IPv4地址哈希
                h = h ^ static_cast<size_t>(commAddr.addr.s_addr);
                break;
            }
             case COMM_ADDR_TYPE_IP_V6: {
                for (u32 i = 0; i < sizeof(commAddr.addr6); ++i) {
                    h = h ^ static_cast<size_t>(commAddr.addr6.s6_addr[i]) << (i % 8);
                }
                break;
            }
            case COMM_ADDR_TYPE_ID: {
                // ID类型哈希
                h = h ^ static_cast<size_t>(commAddr.id);
                break;
            }
            case COMM_ADDR_TYPE_MULTI_PORT: {
<<<<<<< HEAD
                h = h ^ (std::hash<MultiPortAddr>()(commAddr.portsAddr) << 8);
=======
                h = h ^ static_cast<size_t>(commAddr.portsAddr.portNum);
                for (uint8_t portId = 0; portId < commAddr.portsAddr.portNum && portId < HCOMM_NIC_PORT_MAX_NUM; ++portId) {
                    for (u32 i = 0; i < COMM_ADDR_EID_LEN; ++i) {
                        h = h ^ (static_cast<size_t>(commAddr.portsAddr.eidList[portId][i])
                            << ((i % sizeof(size_t)) * 8));
                    }
                }
                const auto *addrBytes = reinterpret_cast<const uint8_t *>(&commAddr.portsAddr.addr6);
                for (u32 i = 0; i < sizeof(commAddr.portsAddr.addr6); ++i) {
                    h = h ^ (static_cast<size_t>(addrBytes[i]) << ((i % sizeof(size_t)) * 8));
                }
>>>>>>> 4c169f3f... Uboe 控制面 Endpoint 适配
                break;
            }
            default: {
                break;
            }
        }
        return h;
    }
};

template <>
struct hash<std::pair<CommAddr, CommProtocol>> {
    size_t operator()(const std::pair<CommAddr, CommProtocol>& key) const
    {
        size_t h1 = std::hash<CommAddr>()(key.first);
        size_t h2 = std::hash<CommProtocol>()(key.second);
        return h1 ^ (h2 << 1);
    }
};
};  // namespace std

#endif // TOPO_COMMON_TYPES_H