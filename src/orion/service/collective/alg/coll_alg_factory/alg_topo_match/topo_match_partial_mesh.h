/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 拓扑匹配TopoMatch Partial Mesh类实现
 * Author: zhanlijuan
 * Create: 2025-04-16
 */

#ifndef HCCLV2_TOPO_MATCH_MESH
#define HCCLV2_TOPO_MATCH_MESH
#include <string>
#include <vector>
#include <map>

#include <hccl/hccl_types.h>
#include "hccl/base.h"
#include "types/types.h"
#include "dev_type.h"
#include "topo_match_base.h"
#include "rank_gph.h"

namespace Hccl {

class TopoMatchPartialMesh : public TopoMatchBase {
public:
    explicit TopoMatchPartialMesh(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                           const DevType devType);
    ~TopoMatchPartialMesh() override;

    std::string Describe() const override
    {
        return "Topo Match for Partial Mesh Algorithm (CURRENTLY only 910_95 is supported).";
    }
    using TopoMatchBase::MatchTopo;
    HcclResult MatchTopo(std::vector<std::vector<RankId>> &vTopo, std::vector<RankId> &virtRanks,
                         std::map<RankId, u32> &virtRankMap) override;

    HcclResult SetTargetRanks(std::set<u32>& targetRanks) override;

private:
    HcclResult MeshTopoForAllLevel();
    std::vector<RankId> rankIds_; // virtualTopoRank
    std::vector<std::vector<RankId>> rankOnSameBoardVector_;  // ranks vector with same boardIds in rack
    std::vector<std::vector<RankId>> rankOnSameSlotVector_;  // ranks vector with same slotIds in rack
    std::vector<u32> numRanksPerBoard_;  // ranks num with same boardIds in rack
};
} // namespace Hccl

#endif // !HCCLV2_TOPO_MATCH_MESH