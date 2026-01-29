/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库InsV2AlltoAllSoleExecutor类实现
 * Author: wuhongge
 * Create: 2025-08-15
 */

#ifndef HCCLV2_INS_V2_ALL_TO_ALL_SOLE_EXECUTOR_H
#define HCCLV2_INS_V2_ALL_TO_ALL_SOLE_EXECUTOR_H
#include "ins_coll_alg_base.h"

namespace Hccl {
template <typename AlgTopoMatch, typename InsAlgTemplate>
class InsV2AlltoAllSoleExecutor : public InsCollAlgBase {
public:
    explicit InsV2AlltoAllSoleExecutor();
    ~InsV2AlltoAllSoleExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based AlltoAll Sole Executor.";
    }

    // HOST 接口
    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
        InsQuePtr insQue) override;
    // AICPU 接口
    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
        ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;

    HcclResult CalcResOffload(
        const RankGraph *rankGraph, const u64 &dataSize, CollOffloadOpResReq &resReq) override;

    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult CalBlockDim(u32& blockDim, u64 dataSize, u32 blockDimLimit) override;

private:
    HcclResult InitParams(const CollAlgOperator &op, const CollAlgParams &params) override;
    HcclResult InitCommInfo(const RankGraph *rankGraph);
    HcclResult InitCommInfo(const AlgTopoInfo &topoInfo);
    HcclResult CreateTemplates(std::shared_ptr<InsAlgTemplate> &algTemplatePtr);
    HcclResult GetTemplateResRequest(
        const RankGraph *rankGraph, std::shared_ptr<InsAlgTemplate> &algTemplate, AlgTempResReq &tempResReq) const;
    HcclResult GetTemplateResRequest(
        ConnectedLinkMgr *linkMgr, std::shared_ptr<InsAlgTemplate> &algTemplate, AlgTempResReq &tempResReq) const;
    HcclResult OrchestrateLoop(std::shared_ptr<InsAlgTemplate> algTemplate);

    std::vector<RankId> virtRanks_;
    std::map<RankId, u32> virtRankMap_;
    std::vector<std::vector<RankId>> vTopo_;
    ResLinks tempResLinks_;
    std::vector<InsQuePtr> tempInsQue_;
};
}  // namespace Hccl

#endif  // HCCLV2_INS_V2_ALL_TO_ALL_SOLE_EXECUTOR_H