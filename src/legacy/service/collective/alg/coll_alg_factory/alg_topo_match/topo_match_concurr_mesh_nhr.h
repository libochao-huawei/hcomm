/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 拓扑匹配TopoMatchConcurrMeshNHR类头文件
 */

#ifndef HCCLV2_TOPO_MATCH_CONCURR_MESH_NHR
#define HCCLV2_TOPO_MATCH_CONCURR_MESH_NHR

#include "topo_match_base.h"

namespace Hccl {

class TopoMatchConcurrMeshNHR : public TopoMatchBase {
public:
    explicit TopoMatchConcurrMeshNHR(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                                  const DevType devType);
    ~TopoMatchConcurrMeshNHR() override;

    std::string Describe() const override
    {
        return "Topo Match for Multi-dimensional Concurrent Mesh Algorithm (CURRENTLY only 2-D Concurr Mesh is "
               "supported).";
    }
    using TopoMatchBase::MatchTopo;
    HcclResult MatchTopo(std::vector<std::vector<std::vector<RankId>>> &vTopo,
        std::vector<std::vector<RankId>> &virtRanks, std::vector<std::map<RankId, u32>> &virtRankMap) override;

private:
    std::vector<RankId> rankOnSameBoard_; // ranks with same boardIds in rack
    std::vector<RankId> rankOnSameSlot_;  // ranks with same slotIds in rack
    std::vector<std::vector<RankId>> rankOnSameBoardVector_;  // ranks vector with same boardIds in rack
    std::vector<std::vector<RankId>> rankOnSameSlotVector_;  // ranks vector with same slotIds in rack
    std::vector<u32> numRanksPerBoard_;  // ranks num with same boardIds in rack

    HcclResult GenerateLevel0(const std::set<RankId> &rankSetLevel0, u32 gcdInstSize, RankId rankId, 
                              std::vector<std::vector<std::vector<RankId>>> &vTopo,
                              std::vector<std::vector<RankId>> &virtRanks);

    // 当 level0 子通信域为矩形时, 计算 myRank 所在 level0 子通信域的具体信息
    HcclResult FindLevel0Block(const Matrix<RankId>& podTopo, u32 subDim0Size, u32 subDim1Size,
                               u32 myRank, std::vector<RankId>& subRankList, Matrix<RankId>& subRankTopo) const;

    // FindLevel0Bloc 的辅助函数
    HcclResult FindMyRankLocation(const Matrix<RankId>& podTopo, u32 myRank, u32 &row, u32 &col) const;
    HcclResult ExtractLevel0Block(const Matrix<RankId>& podTopo, u32 startRow, u32 startCol,
                                  u32 targetRow, u32 targetCol,
                                  u32 subDim0Size, u32 subDim1Size,
                                  std::vector<RankId>& subRankList,
                                  Matrix<RankId>& subRankTopo) const;

    // 对称判断依据: instSizeList 中每个元素相等
    bool CheckSymmetric(std::vector<u32>& values) const;
};
} // namespace Hccl

#endif // !HCCLV2_TOPO_MATCH_CONCURR_MESH_NHR