/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库InsBroadcastSoleExecutor类头文件
 * Author: libiaozhi
 * Create: 2025-03-21
 */

#ifndef HCCLV2_INS_BROADCAST_SOLE_EXECUTOR
#define HCCLV2_INS_BROADCAST_SOLE_EXECUTOR

#include "ins_coll_alg_base.h"

namespace Hccl {

template <typename AlgTopoMatch, typename InsAlgTemplate> class InsBroadcastSoleExecutor : public InsCollAlgBase {
public:
    explicit InsBroadcastSoleExecutor();
    ~InsBroadcastSoleExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based broadcast Sole Executor.";
    }

    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                          InsQuePtr insQue) override;
    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                             ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;

private:
    HcclResult OrchestrateOffload(InsAlgTemplate &tempAlg);
    HcclResult OrchestrateOpbase(InsAlgTemplate &tempAlg);

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<InsQuePtr> requiredQue_;
    ResLinks               tempResLinks_;
};

} // namespace Hccl

#endif // #ifndef HCCLV2_INS_BROADCAST_SOLE_EXECUTOR
