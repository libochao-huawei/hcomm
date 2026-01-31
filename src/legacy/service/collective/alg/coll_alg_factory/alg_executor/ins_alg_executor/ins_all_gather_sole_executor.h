/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法库InsAllGatherSoleExecutor类头文件
 * Author: shenyutian
 * Create: 2024-04-30
 */

#ifndef HCCLV2_INS_ALL_GATHER_SOLE_EXECUTOR
#define HCCLV2_INS_ALL_GATHER_SOLE_EXECUTOR
#include "ins_coll_alg_base.h"

namespace Hccl {

template <typename AlgTopoMatch, typename InsAlgTemplate> class InsAllGatherSoleExecutor : public InsCollAlgBase {
public:
    explicit InsAllGatherSoleExecutor();
    ~InsAllGatherSoleExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based All Gather Sole Executor.";
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

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<InsQuePtr> requiredQue_;
    ResLinks               tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_ALL_GATHER_SOLE_EXECUTOR