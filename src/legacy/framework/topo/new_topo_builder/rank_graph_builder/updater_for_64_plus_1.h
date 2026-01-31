/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: UpdaterFor64Plus1
 * Create: 2025-06-13
 */

#ifndef UPDATER_FOR_64_PLUS_1_H
#define UPDATER_FOR_64_PLUS_1_H

#include <string>
#include <memory>
#include <unordered_map>
#include "rank_table_info.h"
#include "rank_gph.h"
#include "phy_topo.h"

namespace Hccl {

constexpr LocalId DEVICE_NUM_PER_AXIS = 8;
constexpr LocalId DEVICE_HALF_NUM_PER_AXIS = 4;
constexpr u32     BACKUP_TO_PLANE_ADDR_NUM = 4;
constexpr u32     BACKUP_PLANE_NUM = 4;

class UpdaterFor64Plus1 {
public:
    UpdaterFor64Plus1() = default;
    ~UpdaterFor64Plus1() = default;
    UpdaterFor64Plus1(const UpdaterFor64Plus1 &) = delete;
    UpdaterFor64Plus1& operator=(const UpdaterFor64Plus1&) = delete;

    void SaveReplaceInfo(const NewRankInfo& rank);
    void UpdateRankGraph(RankGraph* rankGraph, const RankTableInfo* rankTable) const;

private:
    void UpdateNetInstance(NetInstance* fabricGroup, LocalId localId, LocalId replacedLocalId, const RankTableInfo* rankTable) const;
    std::string GetPortFromSet(std::set<string> &ports, u32 linkIdx) const;
    std::shared_ptr<PhyTopo::Link> GetPeer2PlaneEdges(u32 backupPlaneId, shared_ptr<NetInstance::Peer> peer, 
        std::shared_ptr<Graph<PhyTopo::Node, PhyTopo::Link>> phyTopoGraph) const;
    void AddPeer2BackupLinks(std::shared_ptr<NetInstance::Peer> peer, std::shared_ptr<NetInstance::Peer> backupPeer,
                             LocalId replacedLocalId, NetInstance* netInstance, const RankTableInfo* rankTable) const;
    bool IsSameX(LocalId srcLocalId, LocalId dstLocalId) const;
    bool IsSameY(LocalId srcLocalId, LocalId dstLocalId) const;
    std::pair<u32, u32> GetLinkIndex(LocalId localId, LocalId replacedLocalId) const; // <BackupPlaneId, addr idx>

    std::unordered_map<std::string, std::pair<LocalId, LocalId>> replaceInfo{}; // <R0Id, <localId, replacedLocalId>>
};

} // namespace Hccl

#endif //UPDATER_FOR_64_PLUS_1_H
