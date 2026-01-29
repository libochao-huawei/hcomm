/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 拓扑匹配TopoMatchNHR类头文件
 * Author: ningshuqi
 * Create: 2025-02-12
 */

#ifndef HCCLV2_TOPO_MATCH_NHR
#define HCCLV2_TOPO_MATCH_NHR
#include <string>
#include <vector>
#include <map>

#include "types.h"
#include "dev_type.h"
#include "topo_match_base.h"
#include "rank_gph.h"

namespace Hccl {

class TopoMatchNHR : public TopoMatchBase {
public:
    explicit TopoMatchNHR(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                           const DevType devType);
    ~TopoMatchNHR() override;

    std::string Describe() const override
    {
        return "Topo Match for NHR Algorithm (CURRENTLY only 910_95 is supported).";
    }
    using TopoMatchBase::MatchTopo;
    HcclResult MatchTopo(std::vector<std::vector<RankId>> &vTopo, std::vector<RankId> &virtRanks,
                         std::map<RankId, u32> &virtRankMap) override;

private:
    HcclResult NHRTopoForAllRanks();
    u32 GetNHRStepNum(u32 rankSize) const;
    std::vector<RankId> rankIds_; // virtualTopoRank
    std::vector<std::vector<RankId>> rankOnSameBoardVector_;  // ranks vector with same boardIds in rack
    std::vector<std::vector<RankId>> rankOnSameSlotVector_;  // ranks vector with same slotIds in rack
    std::vector<u32> numRanksPerBoard_;  // ranks num with same boardIds in rack
};
} // namespace Hccl

#endif // !HCCLV2_TOPO_MATCH_MESH