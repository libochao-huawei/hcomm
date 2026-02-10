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

inline bool operator==(const EndpointDesc& a, const EndpointDesc& b) noexcept {
    return ::memcmp(&a, &b, sizeof(EndpointDesc)) == 0;
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
struct hash<EndpointDesc> {
    size_t operator()(const EndpointDesc& e) const noexcept {
        // FNV-1a（64位）对字节序列做hash
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&e);
        size_t h = sizeof(size_t) == 8
            ? static_cast<size_t>(14695981039346656037ull)
            : static_cast<size_t>(2166136261u);

        const size_t prime = sizeof(size_t) == 8
            ? static_cast<size_t>(1099511628211ull)
            : static_cast<size_t>(16777619u);

        for (size_t i = 0; i < sizeof(EndpointDesc); ++i) {
            h ^= static_cast<size_t>(p[i]);
            h *= prime;
        }
        return h;
    }
};
};  // namespace std

#endif // TOPO_COMMON_TYPES_H