/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库InsScatterSoleExecutor类头文件
 * Author: chenchao
 * Create: 2025-01-12
 */

#ifndef HCCLV2_INS_SCATTER_SOLE_EXECUTOR
#define HCCLV2_INS_SCATTER_SOLE_EXECUTOR

#include "ins_coll_alg_base.h"
#include "ins_temp_all_gather_mesh.h"
#include "ins_temp_scatter_nhr.h"
#include "topo_match_mesh.h"
#include "topo_match_nhr.h"
#include "topo_match_concurr_mesh.h"

namespace Hccl {

template <typename AlgTopoMatch, typename InsAlgTemplate> class InsScatterSoleExecutor : public InsCollAlgBase {
public:
    explicit InsScatterSoleExecutor();
    ~InsScatterSoleExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based Scatter Sole Executor.";
    }

    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                          InsQuePtr insQue) override;
    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                             ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;

private:
    HcclResult GenInsQues4Offload(InsAlgTemplate &tempAlg);
    HcclResult GenInsQues4Opbase(InsAlgTemplate &tempAlg);
    HcclResult GenInsQues4Ccu(InsAlgTemplate &tempAlg);

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<InsQuePtr> requiredQue_;
    ResLinks               tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_INS_SCATTER_SOLE_EXECUTOR