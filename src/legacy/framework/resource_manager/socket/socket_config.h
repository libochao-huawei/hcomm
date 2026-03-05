/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_SOCKET_CONFIG_H
#define HCCL_SOCKET_CONFIG_H

#include "types.h"
#include "virtual_topo.h"
#include "hash_utils.h"

namespace Hccl {
MAKE_ENUM(SocketRole, SERVER, CLIENT)
class SocketConfig {
public:
    RankId            remoteRank;
    LinkData          link;
    const std::string tag;

    SocketConfig(RankId remoteRank, const LinkData &link, const std::string &tag)
        : remoteRank(remoteRank), link(link), tag(tag),
          role(link.GetLocalRankId() < link.GetRemoteRankId() ? SocketRole::SERVER : SocketRole::CLIENT),
          hccpTag(role == SocketRole::SERVER
                      ? tag + "_" + to_string(link.GetLocalRankId()) + "_" + to_string(link.GetRemoteRankId()) + "_" +
                            link.GetLocalAddr().GetIpStr() + "_" + link.GetRemoteAddr().GetIpStr()
                      : tag + "_" + to_string(link.GetRemoteRankId()) + "_" + to_string(link.GetLocalRankId()) + "_" +
                            link.GetRemoteAddr().GetIpStr() + "_" + link.GetLocalAddr().GetIpStr())
    {}

    SocketConfig(const LinkData &link, const std::string &tag)
        : remoteRank(link.GetRemoteRankId()), link(link), tag(tag),
          role(link.GetLocalAddr() < link.GetRemoteAddr() ? SocketRole::SERVER : SocketRole::CLIENT),
          hccpTag(role == SocketRole::SERVER
                      ? tag + "_" + to_string(link.GetLocalRankId()) + "_" + to_string(link.GetRemoteRankId()) + "_" +
                            link.GetLocalAddr().GetIpStr() + "_" + link.GetRemoteAddr().GetIpStr()
                      : tag + "_" + to_string(link.GetRemoteRankId()) + "_" + to_string(link.GetLocalRankId()) + "_" +
                            link.GetRemoteAddr().GetIpStr() + "_" + link.GetLocalAddr().GetIpStr())

    {}

    SocketRole GetRole() const
    {
        return role;
    }

    const string &GetHccpTag() const
    {
        return hccpTag;
    }

private:
    SocketRole role{};
    string     hccpTag;
};
} // namespace Hccl

namespace std {
// 特化SocketConfig的hash和equal模板，使其可用做map的key
template <> class hash<Hccl::SocketConfig> {
public:
    size_t operator()(const Hccl::SocketConfig &socketConfig) const
    {
        auto remoteRankHash = hash<Hccl::RankId>{}(socketConfig.remoteRank);
        auto localPortHash  = hash<Hccl::PortData>{}(socketConfig.link.GetLocalPort());
        auto remotePortHash = hash<Hccl::PortData>{}(socketConfig.link.GetRemotePort());
        auto tagHash        = hash<string>{}(socketConfig.tag);

        return Hccl::HashCombine({remoteRankHash, localPortHash, remotePortHash, tagHash});
    }
};

template <> class equal_to<Hccl::SocketConfig> {
public:
    bool operator()(const Hccl::SocketConfig &config, const Hccl::SocketConfig &otherConfig) const
    {
        return config.remoteRank == otherConfig.remoteRank
               && config.link.GetLocalPort().GetAddr() == otherConfig.link.GetLocalPort().GetAddr()
               && config.link.GetRemotePort().GetAddr() == otherConfig.link.GetRemotePort().GetAddr()
               && config.tag == config.tag;
    }
};
} // namespace std

#endif // HCCL_SOCKET_CONFIG_H
