/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "topo_match_nhr.h"
#include "net_instance.h"

namespace Hccl {
TopoMatchNHR::TopoMatchNHR(const RankId vRank, const u32 rankSize, const RankGraph *rankGraph,
                             const DevType devType)
    : TopoMatchBase(vRank, rankSize, rankGraph, devType)
{
}

TopoMatchNHR::~TopoMatchNHR()
{
}

HcclResult TopoMatchNHR::MatchTopo(std::vector<std::vector<RankId>> &vTopo, std::vector<RankId> &virtRanks,
                                    std::map<RankId, u32> &virtRankMap)
{
    CHK_PRT_RET(devType_ != DevType::DEV_TYPE_950,
        HCCL_ERROR("[CollAlgFactory] [TopoMatchNHR] Rank [%d], deviceType [%s] not supported yet.", myRank_,
                    DevTypeToString(devType_).c_str()),
        HcclResult::HCCL_E_PARA);

    // 获取并校验通信层数
    std::set<u32> levelSet = rankGraph_->GetLevels(myRank_);
    CHK_PRT_RET((levelSet.size() == COMM_LEVEL_SIZE_0),
                HCCL_ERROR("[CollAlgFactory] [TopoMatchNHR] Rank [%d], Invalid virtual topo.", myRank_),
                HcclResult::HCCL_E_PARA);

    // 只有Level0 场景
    if (levelSet.size() == COMM_LEVEL_SIZE_1) {
        rankOnSameBoardVector_.resize(RANK_SIZE_EIGHT, {});
        rankOnSameSlotVector_.resize(RANK_SIZE_EIGHT, {});
        CHK_RET(CalcRankOnSamePlaneOfR0(rankOnSameBoardVector_, rankOnSameSlotVector_, numRanksPerBoard_));
        CHK_PRT_RET((numRanksPerBoard_.size() == 0),
            HCCL_ERROR("[CollAlgFactory] [TopoMatchNHR] Rank [%d], invalid virtual topo", myRank_),
            HcclResult::HCCL_E_PARA);
        if (numRanksPerBoard_.size() == 1 || numRanksPerBoard_[0] == 1) {
            CHK_PRT_RET((numRanksPerBoard_[0] != rankSize_) && (numRanksPerBoard_.size() != rankSize_),
                HCCL_ERROR("[CollAlgFactory] [TopoMatchNHR] Rank [%d], invalid virtual topo with rankSize [%u]: [%u] "
                        "peers on identical board, [%u] peers on identical slot.",
                        myRank_, rankSize_, numRanksPerBoard_[0], numRanksPerBoard_.size()),
                HcclResult::HCCL_E_PARA);
            const NetInstance* netInstance = rankGraph_->GetNetInstanceByRankId(0, myRank_);
            std::set<RankId> rankSetR0 = netInstance->GetRankIds();
            for (RankId rankId : rankSetR0) {
                rankIds_.push_back(rankId);
            }
        } else {
            CHK_RET(NHRTopoForAllRanks());
        }

    // Level0 和 Level1打平场景
    } else if (levelSet.size() == COMM_LEVEL_SIZE_2) {
        CHK_RET(NHRTopoForAllRanks());
    } else {
        HCCL_ERROR("[CollAlgFactory] [TopoMatchNHR] Rank [%d], deviceType [%s] not supported yet.",
            myRank_, DevTypeToString(devType_).c_str());
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    virtRanks = rankIds_;
    vTopo.push_back(rankIds_);

    CHK_PRT_RET(GenVirtRankMapping(virtRanks, virtRankMap) != HcclResult::HCCL_SUCCESS,
                HCCL_ERROR("[CollAlgFactory] [TopoMatchNHR] Rank [%d], Fail to generate virtRankMapping.", myRank_),
                HcclResult::HCCL_E_INTERNAL);

    return HcclResult::HCCL_SUCCESS;
}

HcclResult TopoMatchNHR::NHRTopoForAllRanks()
{
    RankId sendToRank;
    RankId recvFromRank;
    u32 nSteps = GetNHRStepNum(rankSize_);
    std::set<RankId> rankSet;
    for (u32 currentStep = 0; currentStep < nSteps; currentStep++) {
        u32 deltaRank = nSteps - 1 - currentStep;
        sendToRank = (myRank_ + (1 << deltaRank)) % rankSize_;
        recvFromRank = (myRank_ + rankSize_ - (1 << deltaRank)) % rankSize_;
        rankSet.insert(sendToRank);
        rankSet.insert(recvFromRank);
    }
    for (RankId rankId : rankSet) {
        if (GetPathNum(myRank_, rankId) == 0) {
            HCCL_ERROR("[CollAlgFactory] [TopoMatchNHR] Rank [%d], Invalid virtual topo for NHR.", myRank_);
            return HcclResult::HCCL_E_PARA;
        }
    }
    for (u32 rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        rankIds_.push_back(RankId(rankIdx));
    }
    return HcclResult::HCCL_SUCCESS;
}

// NHR的算法步数 = Ceil(log2(N))
u32 TopoMatchNHR::GetNHRStepNum(u32 rankSize) const
{
    u32 nSteps = 0;
    for (u32 tmp = rankSize - 1; tmp != 0; tmp >>= 1, nSteps++) {
    }
    HCCL_DEBUG("[NHRBase][GetStepNumInterServer] rankSize[%u] nSteps[%u]", rankSize, nSteps);

    return nSteps;
}
} // namespace Hccl
