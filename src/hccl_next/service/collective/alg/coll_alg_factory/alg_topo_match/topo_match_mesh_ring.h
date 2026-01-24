/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 拓扑匹配TopoMatchMeshRing类头文件
 * Author: shenyutian
 * Create: 2024-02-18
 */

#ifndef HCCLV2_TOPO_MATCH_MESH_RING
#define HCCLV2_TOPO_MATCH_MESH_RING
#include <string>
#include <vector>
#include <map>

#include "types.h"
#include "dev_type.h"
#include "topo_match_base.h"
#include "rank_gph.h"
#include "net_instance.h"

namespace Hccl {

class TopoMatchMeshRing : public TopoMatchBase {
public:
    explicit TopoMatchMeshRing(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                               const DevType devType);
    ~TopoMatchMeshRing() override;

    std::string Describe() const override
    {
        return "Topo Match for combined Algorithm: level 0 Mesh, level 1 Ring.";
    }
    using TopoMatchBase::MatchTopo;
    HcclResult MatchTopo(std::vector<std::vector<std::vector<RankId>>> &vTopo,
                         std::vector<std::vector<RankId>>              &virtRanks,
                         std::vector<std::map<RankId, u32>>            &virtRankMap) override;
private:
    HcclResult MeshRingTopoForAllLevel(std::set<RankId> rankSetR0,
                                       std::vector<std::vector<std::vector<RankId>>> &vTopo,
                                       std::vector<std::vector<RankId>> &virtRanks);
    std::vector<std::vector<RankId>> rankOnSameBoardVector_;  // ranks vector with same boardIds in rack
    std::vector<std::vector<RankId>> rankOnSameSlotVector_;  // ranks vector with same slotIds in rack
    std::vector<u32> numRanksPerBoard_;  // ranks num with same boardIds in rack
};
} // namespace Hccl

#endif // !HCCLV2_TOPO_MATCH_RING_RING