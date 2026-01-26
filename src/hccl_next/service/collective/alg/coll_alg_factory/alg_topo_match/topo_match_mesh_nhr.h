/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 拓扑匹配TopoMatchMeshNHR类头文件
 * Author: ningshuqi
 * Create: 2025-04-27
 */

#ifndef HCCLV2_TOPO_MATCH_MESH_NHR
#define HCCLV2_TOPO_MATCH_MESH_NHR
#include <string>
#include <vector>
#include <map>

#include "types.h"
#include "dev_type.h"
#include "topo_match_base.h"
#include "rank_gph.h"

namespace Hccl {

class TopoMatchMeshNHR : public TopoMatchBase {
public:
    explicit TopoMatchMeshNHR(
        const RankId vRank, const u32 rankSize, const RankGraph *rankGraph, const DevType devType);
    ~TopoMatchMeshNHR() override;

    std::string Describe() const override
    {
        return "Topo Match for combined Algorithm: level 0 Mesh, level 1 NHR.";
    }
    using TopoMatchBase::MatchTopo;
    HcclResult MatchTopo(std::vector<std::vector<std::vector<RankId>>> &vTopo,
        std::vector<std::vector<RankId>> &virtRanks, std::vector<std::map<RankId, u32>> &virtRankMap) override;

private:
    HcclResult GenerateLevel0(
        const std::set<RankId> &rankSet, u32 levelSize, RankId rankId,
                              std::vector<std::vector<std::vector<RankId>>> &vTopo,
                              std::vector<std::vector<RankId>> &virtRanks);
    std::vector<std::vector<RankId>> rankOnSameBoardVector_;  // ranks vector with same boardIds in rack
    std::vector<std::vector<RankId>> rankOnSameSlotVector_;   // ranks vector with same slotIds in rack
    std::vector<u32> numRanksPerBoard_;                       // ranks num with same boardIds in rack
};
}  // namespace Hccl

#endif  // !HCCLV2_TOPO_MATCH_MESH_NHR