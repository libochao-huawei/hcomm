/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: TopoInfo header file
 * Create: 2024-12-16
 */

#ifndef NEW_TOPO_INFO_H
#define NEW_TOPO_INFO_H

#include <map>
#include <vector>
#include <unordered_set>

#include "peer_info.h"
#include "edge_info.h"

namespace Hccl {

constexpr u32 MAX_PEER_COUNT = 65;
class TopoInfo {
public:
    TopoInfo() {};
    std::string                          version;
    u32                                  peerCount{0};
    u32                                  edgeCount{0};
    std::vector<PeerInfo>                peers;
    std::map<u32, std::vector<EdgeInfo>> edges;
    void                                 Deserialize(const nlohmann::json &topoInfoJson);
    std::string                          Describe() const;
    void                                 Dump() const;
    TopoInfo(BinaryStream& binaryStream);
    void GetBinStream(BinaryStream& binaryStream) const;

private:
    void DeserializePeers(const nlohmann::json &topoInfoJson);
    void DeserializeEdges(const nlohmann::json &topoInfoJson);
    void VerifyEdges(EdgeInfo &edge);
    unordered_set<u32> idSet;
};
} // namespace Hccl
#endif // NEW_TOPO_INFO_H