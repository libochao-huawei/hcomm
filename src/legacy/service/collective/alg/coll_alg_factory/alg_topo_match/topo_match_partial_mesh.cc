/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 拓扑匹配TopoMatch Partial Mesh类实现
 * Author: zhanlijuan
 * Create: 2025-04-16
 */

#include "topo_match_partial_mesh.h"

namespace Hccl {
TopoMatchPartialMesh::TopoMatchPartialMesh(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                             const DevType devType)
    : TopoMatchBase(vRank, rankSize, rankGraph, devType)
{
}

TopoMatchPartialMesh::~TopoMatchPartialMesh()
{
}

HcclResult TopoMatchPartialMesh::MatchTopo(std::vector<std::vector<RankId>> &vTopo, std::vector<RankId> &virtRanks,
                                    std::map<RankId, u32> &virtRankMap)
{
    if (devType_ != DevType::DEV_TYPE_910_95) {
        HCCL_ERROR("[CollAlgFactory] [TopoMatchPartialMesh] Rank [%d], deviceType [%s] not supported yet.", myRank_,
                    DevTypeToString(devType_).c_str());
                    return HcclResult::HCCL_E_PARA;
    }
    // 获取并校验通信层数
    std::set<u32> levelSet = rankGraph_->GetLevels(myRank_);
    CHK_PRT_RET((levelSet.size() == COMM_LEVEL_SIZE_0),
                HCCL_ERROR("[CollAlgFactory] [TopoMatchPartialMesh] Rank [%d], Invalid virtual topo.", myRank_),
                HcclResult::HCCL_E_PARA);

    // 只有Level0 场景
    if (levelSet.size() == COMM_LEVEL_SIZE_1) {
        std::set<RankId> partialRankSetR0; // ranks vector of partial mesh topo
        partialRankSetR0.insert(myRank_);
        for (auto it : batchSendRecvtargetRanks_) {
            if (static_cast<RankId>(it) == myRank_) {
                continue;
            }
            u32 pathNum = GetPathNum(myRank_, static_cast<RankId>(it));
            if (pathNum != 0) {
                HCCL_DEBUG("[CollAlgFactory][TopoMatchPartialMesh] Rank [%d], remoteUsrRank [%d] has links.", myRank_,
                    static_cast<RankId>(it));
                partialRankSetR0.insert(static_cast<RankId>(it));
            } else {
                HCCL_ERROR("[CollAlgFactory][TopoMatchPartialMesh] Rank [%d], remoteUsrRank [%d] has no link.", myRank_,
                    static_cast<RankId>(it));
                return HcclResult::HCCL_E_PARA;
            }
        }
        for (RankId rankId : partialRankSetR0) {
            rankIds_.push_back(rankId);
        }
    // Level0 和 Level1打平场景
    } else if (levelSet.size() == COMM_LEVEL_SIZE_2) {
        CHK_RET(MeshTopoForAllLevel());
    } else {
        HCCL_ERROR("[CollAlgFactory] [TopoMatchPartialMesh] Rank [%d], deviceType [%s] not supported yet.",
            myRank_, DevTypeToString(devType_).c_str());
        return HcclResult::HCCL_E_PARA;
    }
    virtRanks = rankIds_;
    vTopo.push_back(rankIds_);

    CHK_PRT_RET(GenVirtRankMapping(virtRanks, virtRankMap) != HcclResult::HCCL_SUCCESS,
        HCCL_ERROR("[CollAlgFactory] [TopoMatchPartialMesh] Rank [%d], Fail to generate virtRankMapping.", myRank_),
        HcclResult::HCCL_E_INTERNAL);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TopoMatchPartialMesh::SetTargetRanks(std::set<u32>& targetRanks)
{
    batchSendRecvtargetRanks_ = targetRanks;
    HCCL_DEBUG("[CollAlgFactory][TopoMatchPartialMesh] SetTargetRanks batchSendRecvtargetRanks_ size[%u]",
        batchSendRecvtargetRanks_.size());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TopoMatchPartialMesh::MeshTopoForAllLevel()
{
    std::set<RankId> partialRankSet; // ranks vector of partial mesh topo
    partialRankSet.insert(myRank_);
    for (auto it : batchSendRecvtargetRanks_) {
        if (static_cast<RankId>(it) == myRank_) {
            continue;
        }
        u32 pathNum = GetPathNum(myRank_, static_cast<RankId>(it));
        if (pathNum != 0) {
            partialRankSet.insert(static_cast<RankId>(it));
        } else {
            HCCL_ERROR("[CollAlgFactory][TopoMatchPartialMesh] Rank [%d], remoteUsrRank [%d] has no link.", myRank_,
                static_cast<RankId>(it));
            return HcclResult::HCCL_E_PARA;
        }
    }
    for (RankId rankId : partialRankSet) {
        rankIds_.push_back(rankId);
    }
    return HcclResult::HCCL_SUCCESS;
}
} // namespace Hccl