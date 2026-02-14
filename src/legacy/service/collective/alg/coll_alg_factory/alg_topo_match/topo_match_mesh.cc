/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "topo_match_mesh.h"

namespace Hccl {
TopoMatchMesh::TopoMatchMesh(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                             const DevType devType)
    : TopoMatchBase(vRank, rankSize, rankGraph, devType)
{
}

TopoMatchMesh::~TopoMatchMesh()
{
}

HcclResult TopoMatchMesh::MatchTopo(std::vector<std::vector<RankId>> &vTopo, std::vector<RankId> &virtRanks,
                                    std::map<RankId, u32> &virtRankMap)
{
    CHK_PRT_RET(devType_ != DevType::DEV_TYPE_950,
        HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], deviceType [%s] not supported yet.", myRank_,
                    DevTypeToString(devType_).c_str()),
        HcclResult::HCCL_E_PARA);
    // 获取并校验通信层数
    std::set<u32> levelSet = rankGraph_->GetLevels(myRank_);
    CHK_PRT_RET((levelSet.size() == COMM_LEVEL_SIZE_0),
                HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], Invalid virtual topo.", myRank_),
                HcclResult::HCCL_E_PARA);

    // 只有Level0 场景
    if (levelSet.size() == COMM_LEVEL_SIZE_1) {
        rankOnSameBoardVector_.resize(RANK_SIZE_EIGHT, {});
        rankOnSameSlotVector_.resize(RANK_SIZE_EIGHT, {});
        CHK_RET(CalcRankOnSamePlaneOfR0(rankOnSameBoardVector_, rankOnSameSlotVector_, numRanksPerBoard_));
        const NetInstance* netInstance = rankGraph_->GetNetInstanceByRankId(0, myRank_);
        std::set<RankId> rankSetR0 = netInstance->GetRankIds();
        if (numRanksPerBoard_.size() == 1 || numRanksPerBoard_[0] == 1) {
            CHK_PRT_RET((numRanksPerBoard_[0] != rankSize_) && (numRanksPerBoard_.size() != rankSize_),
                HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], invalid virtual topo with rankSize [%u]: [%u] "
                        "peers on identical board, [%u] peers on identical slot.",
                        myRank_, rankSize_, numRanksPerBoard_[0], numRanksPerBoard_.size()),
                HcclResult::HCCL_E_PARA);
        } else {
            if (!IsAllRanksFullMeshConnected(rankSetR0)) {
                HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], Invalid virtual topo for "
                            "mesh in level0.", myRank_);
                return HcclResult::HCCL_E_PARA;
            }
        }
        for (RankId rankId : rankSetR0) {
            rankIds_.push_back(rankId);
        }
    // Level0 和 Level1打平场景
    } else if (levelSet.size() == COMM_LEVEL_SIZE_2) {
        CHK_RET(MeshTopoForAllLevel());
    } else {
        HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], levelSet size [%zu] not supported yet.",
            myRank_, levelSet.size());
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    virtRanks = rankIds_;
    vTopo.push_back(rankIds_);

    CHK_PRT_RET(GenVirtRankMapping(virtRanks, virtRankMap) != HcclResult::HCCL_SUCCESS,
                HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], Fail to generate virtRankMapping.", myRank_),
                HcclResult::HCCL_E_INTERNAL);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TopoMatchMesh::MeshTopoForAllLevel()
{
    const NetInstance* netInstance = rankGraph_->GetNetInstanceByRankId(0, myRank_);
    if (netInstance == nullptr) {
        HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], netInstance is nullptr.", myRank_);
        return HcclResult::HCCL_E_PTR;
    }
    std::set<RankId> rankSet = netInstance->GetRankIds();
    for (u32 rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        RankId rankId = static_cast<RankId>(rankIdx);
        auto rankInRankSet = std::find(rankSet.begin(), rankSet.end(), rankId);
        if (rankInRankSet != rankSet.end()) {
            u32 srcLocalId = rankGraph_->GetReplacedLocalId(myRank_);
            u32 dstLocalId = rankGraph_->GetReplacedLocalId(rankId);
            if ((srcLocalId / RANK_SIZE_EIGHT == dstLocalId / RANK_SIZE_EIGHT) ||
                (srcLocalId % RANK_SIZE_EIGHT == dstLocalId % RANK_SIZE_EIGHT)) {
                rankIds_.push_back(rankId);
                continue;
            }
        }
        if (GetPathNum(myRank_, rankId) == 0) {
            HCCL_ERROR("[CollAlgFactory] [TopoMatchMesh] Rank [%d], Invalid virtual topo for full mesh.", myRank_);
            return HcclResult::HCCL_E_PARA;
        }
        rankIds_.push_back(rankId);
    }
    return HcclResult::HCCL_SUCCESS;
}
} // namespace Hccl
