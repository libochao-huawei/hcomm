/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库InsReduceScatterSoleExecutor类头文件
 * Author: caichanghua
 * Create: 2025-02-12
 */

#ifndef HCCLV2_INS_REDUCE_SCATTER_SOLE_EXECUTOR_H
#define HCCLV2_INS_REDUCE_SCATTER_SOLE_EXECUTOR_H
#include "ins_coll_alg_base.h"

namespace Hccl {

template <typename AlgTopoMatch, typename InsAlgTemplate>
class InsReduceScatterSoleExecutor : public InsCollAlgBase {
public:
    explicit InsReduceScatterSoleExecutor();
    ~InsReduceScatterSoleExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based Reduce Scatter Sole Executor.";
    }

    // HOST 接口
    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                          InsQuePtr insQue) override;
    // AICPU 接口
    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                             ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;

    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

private:
    HcclResult OrchestrateLoop(InsAlgTemplate &tempAlg, const ParamPool& paramPool);

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<InsQuePtr> requiredQue_;
    ResLinks               tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_INS_REDUCE_SCATTER_SOLE_EXECUTOR_H