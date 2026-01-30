/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库InsV2BroadcastSoleExecutor类头文件
 * Author: zhangzuyu
 * Create: 2025-08-01
 */

#ifndef HCCLV2_INS_BROADCAST_SOLE_EXECUTOR
#define HCCLV2_INS_BROADCAST_SOLE_EXECUTOR

#include "ins_coll_alg_base.h"

namespace Hccl {

template <typename AlgTopoMatch, typename InsAlgTemplate> class InsV2BroadcastSoleExecutor : public InsCollAlgBase {
public:
    explicit InsV2BroadcastSoleExecutor();
    ~InsV2BroadcastSoleExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based broadcast 2D Executor.";
    }

    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                          InsQuePtr insQue) override;
    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                             ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;

private:
    HcclResult OrchestrateLoop(std::shared_ptr<InsAlgTemplate> &tempAlg);
    HcclResult CreateTemplates(std::shared_ptr<InsAlgTemplate> &algTemplatePtr);
    HcclResult InitCommInfo(const RankGraph *rankGraph);
    HcclResult InitCommInfo(const AlgTopoInfo &topoInfo);
    HcclResult GetTemplateResRequest(
        const RankGraph *rankGraph, std::shared_ptr<InsAlgTemplate> &algTemplate, AlgTempResReq &tempResReq) const;
    HcclResult GetTemplateResRequest(
        ConnectedLinkMgr *linkMgr, std::shared_ptr<InsAlgTemplate> &algTemplate, AlgTempResReq &tempResReq) const;

    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;

    std::vector<InsQuePtr> requiredQue_;
    ResLinks               tempResLinks_;
};

} // namespace Hccl

#endif // HCCLV2_INS_BROADCAST_SOLE_EXECUTOR
