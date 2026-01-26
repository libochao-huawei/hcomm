/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 拓扑匹配TopoMatchConcurrMesh类头文件
 * Author: shenyutian
 * Create: 2024-03-12
 */

#ifndef HCCLV2_TOPO_MATCH_CONCURR_MESH
#define HCCLV2_TOPO_MATCH_CONCURR_MESH

#include "topo_match_base.h"

namespace Hccl {

class TopoMatchConcurrMesh : public TopoMatchBase {
public:
    explicit TopoMatchConcurrMesh(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                                  const DevType devType);
    ~TopoMatchConcurrMesh() override;

    std::string Describe() const override
    {
        return "Topo Match for Multi-dimensional Concurrent Mesh Algorithm (CURRENTLY only 2-D Concurr Mesh is "
               "supported).";
    }
    using TopoMatchBase::MatchTopo;
    HcclResult MatchTopo(std::vector<std::vector<RankId>> &vTopo, std::vector<RankId> &virtRanks,
                         std::map<RankId, u32> &virtRankMap) override;

private:
    std::vector<RankId> rankIds_;         // virtualTopoRank
    std::vector<RankId> rankOnSameBoard_; // ranks with same boardIds in rack
    std::vector<RankId> rankOnSameSlot_;  // ranks with same slotIds in rack
    std::vector<std::vector<RankId>> rankOnSameBoardVector_;  // ranks vector with same boardIds in rack
    std::vector<std::vector<RankId>> rankOnSameSlotVector_;  // ranks vector with same slotIds in rack
    std::vector<u32> numRanksPerBoard_;  // ranks num with same boardIds in rack
};
} // namespace Hccl

#endif // !HCCLV2_TOPO_MATCH_CONCURR_MESH