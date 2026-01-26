/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Topo Common header file
 * Create: 2024-12-16
 */

#ifndef TOPO_COMMON_TYPES_H
#define TOPO_COMMON_TYPES_H

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
MAKE_ENUM(LinkProtocol, UB_CTP, UB_TP, ROCE, HCCS, TCP, UB_MEM)
MAKE_ENUM(LinkPortType, PEER, BACKUP_PLANE, NET_PLANE)
MAKE_ENUM(LinkType, PEER2PEER, PEER2NET, PEER2BACKUP)
MAKE_ENUM(AddrPosition, HOST, DEVICE)
MAKE_ENUM(AddrType, EID, IPV4, IPV6)
MAKE_ENUM(NetType, CLOS, MESH_1D, MESH_2D, A3_SERVER, A2_AX_SERVER, TOPO_FILE_DESC)
MAKE_ENUM(TopoType, CLOS, MESH_1D, MESH_2D, A3_SERVER, A2_AX_SERVER)
constexpr LocalId BACKUP_LOCAL_ID = 64;
} // namespace Hccl

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
                h = h ^ static_cast<size_t>(commAddr.id);
                break;
            }
            default: {
                break;
            }
        }
        return h;
    }
};

inline bool operator==(const CommAddr& lhs, const CommAddr& rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }

    switch (lhs.type) {
        case COMM_ADDR_TYPE_IP_V4:
            return lhs.addr.s_addr == rhs.addr.s_addr;

        case COMM_ADDR_TYPE_IP_V6:
            return memcmp(lhs.addr6.s6_addr, rhs.addr6.s6_addr, sizeof(lhs.addr6.s6_addr)) == 0;

        case COMM_ADDR_TYPE_ID:
            return lhs.id == rhs.id;

        case COMM_ADDR_TYPE_EID:
            return memcmp(lhs.eid, rhs.eid, COMM_ADDR_EID_LEN) == 0;

        case COMM_ADDR_TYPE_RESERVED:
        default:
            return true; 
    }
}


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