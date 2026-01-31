/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法库ReduceScatterSoleExecutor类头文件
 * Author: shenyutian
 * Create: 2024-02-04
 */

#ifndef HCCLV2_REDUCE_SCATTER_SOLE_EXECUTOR
#define HCCLV2_REDUCE_SCATTER_SOLE_EXECUTOR

#include "temp_reduce_scatter_ring.h"
#include "temp_reduce_scatter_concurr_mesh.h"
#include "temp_reduce_scatter_mesh.h"
#include "coll_alg_base.h"
#include "topo_match_concurr_mesh.h"
#include "topo_match_mesh.h"

namespace Hccl {

template <typename AlgTopoMatch, typename AlgTemplate> class ReduceScatterSoleExecutor : public CollAlgBase {
public:
    explicit ReduceScatterSoleExecutor();
    ~ReduceScatterSoleExecutor() override;

    std::string Describe() const override
    {
        return "Reduce Scatter Sole Executor.";
    }

    HcclResult GenPrimQues(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                           PrimQuePtr primQue) override;
    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;

    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult GenPrimQuesAIC(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                              ConnectedLinkMgr *linkMgr, PrimQuePtr primQue) override;

private:
    HcclResult GenPrimQues4Offload(AlgTemplateBase &tempAlg);
    HcclResult GenPrimQues4Opbase(const u32 requiredScratchMultiplier, const u32 dataSizePerVolume,
                                  AlgTemplateBase &tempAlg);

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<PrimQuePtr> requiredQue_;
    ResLinks                tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_REDUCE_SCATTER_SOLE_EXECUTOR