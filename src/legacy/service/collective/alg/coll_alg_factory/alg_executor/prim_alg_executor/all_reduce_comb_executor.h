/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法库AllReduceCombExecutor类头文件
 * Author: shenyutian
 * Create: 2024-02-04
 */

#ifndef HCCLV2_ALL_REDUCE_COMB_EXECUTOR
#define HCCLV2_ALL_REDUCE_COMB_EXECUTOR

#include "temp_reduce_scatter_ring.h"
#include "temp_reduce_scatter_concurr_mesh.h"
#include "temp_reduce_scatter_mesh.h"
#include "temp_all_gather_ring.h"
#include "temp_all_gather_concurr_mesh.h"
#include "temp_all_gather_mesh.h"
#include "coll_alg_base.h"
#include "topo_match_concurr_mesh.h"
#include "topo_match_mesh.h"

namespace Hccl {

template <typename AlgTopoMatch, typename AlgTempRS, typename AlgTempAG>
class AllReduceCombExecutor : public CollAlgBase {
public:
    explicit AllReduceCombExecutor();
    ~AllReduceCombExecutor() override;

    std::string Describe() const override
    {
        return "All Reduce Comb Executor: sole TempReduceScatter + sole TempAllGather.";
    }

    HcclResult GenPrimQues(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                           PrimQuePtr primQue) override;
    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult GenPrimQuesAIC(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                              ConnectedLinkMgr *linkMgr, PrimQuePtr primQue) override;

private:
    HcclResult GenPrimQues4Offload(AlgTemplateBase &tempRSAlg, AlgTemplateBase &tempAGAlg);
    HcclResult GenPrimQues4Opbase(const u32 requiredScratchMultiplier, const u32 dataSizePerVolume,
                                  AlgTemplateBase &tempRSAlg, AlgTemplateBase &tempAGAlg);

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<PrimQuePtr> requiredQue_;
    ResLinks                tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_ALL_REDUCE_COMB_EXECUTOR