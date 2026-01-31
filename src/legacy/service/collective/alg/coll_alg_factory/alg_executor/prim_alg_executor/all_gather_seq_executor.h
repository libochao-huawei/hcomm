/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法库AllGatherSeqExecutor类头文件
 * Author: shenyutian
 * Create: 2024-02-17
 */

#ifndef HCCLV2_ALL_GATHER_SEQ_EXECUTOR
#define HCCLV2_ALL_GATHER_SEQ_EXECUTOR

#include "temp_all_gather_ring.h"
#include "temp_all_gather_mesh.h"
#include "coll_alg_base.h"
#include "topo_match_mesh_ring.h"

namespace Hccl {
template <typename AlgTopoMatch, typename AlgTemp0, typename AlgTemp1> class AllGatherSeqExecutor : public CollAlgBase {
public:
    explicit AllGatherSeqExecutor();
    ~AllGatherSeqExecutor() override;

    std::string Describe() const override
    {
        return "All Gather Sequential Executor.";
    }

    HcclResult GenPrimQues(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                           PrimQuePtr primQue) override;
    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult GenPrimQuesAIC(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                              ConnectedLinkMgr *linkMgr, PrimQuePtr primQue) override;

private:
    HcclResult GenPrimQues4Offload(AlgTemplateBase &tempAlg0, AlgTemplateBase &tempAlg1);
    HcclResult GenPrimQues4Opbase(const u32 dataSizePerVolume, AlgTemplateBase &tempAlg0, AlgTemplateBase &tempAlg1);

    HcclResult PrepRes(const RankGraph *rankGraph, const LinkReq &linkReq, ResLinks &resLinks);

    std::vector<std::vector<RankId>>              virtRanks_;
    std::vector<std::map<RankId, u32>>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<std::vector<RankId>>> vTopo_;

    std::vector<u32>                     tempRankSizes_;
    std::vector<std::vector<PrimQuePtr>> tempRequiredQues_;
    std::vector<ResLinks>                tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_ALL_GATHER_SEQ_EXECUTOR