/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库AllGatherParallelExecutor类实现
 * Author: libiaozhi
 * Create: 2025-07-08
 */

#ifndef HCCLV2_INS_ALL_GATHER_PARALLEL_EXECUTOR_H
#define HCCLV2_INS_ALL_GATHER_PARALLEL_EXECUTOR_H
#include "ins_coll_alg_base.h"

namespace Hccl {


template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1>
class InsAllGatherParallelExecutor : public InsCollAlgBase {
public:
    explicit InsAllGatherParallelExecutor();
    ~InsAllGatherParallelExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based All Gather Parallel Executor.";
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
    HcclResult CalcLocalRankSize();
    HcclResult GenInsQuesHost(InsAlgTemplate0 &tempAlgIntra, InsAlgTemplate1 &tempAlgInter);
    void GenTemplateAlgParamsIntra0(const u64 dataOffset, const u64 dataCountPerLoopAixs0, const u64 scratchOffset, TemplateDataParams &tempAlgParamsIntra0) const;
    void GenTemplateAlgParamsIntra1(const u64 dataOffset, const u64 dataCountPerLoopAixs1, const u64 scratchOffset, TemplateDataParams &tempAlgParamsIntra1) const;
    void GenTemplateAlgParamsInter0(const u64 dataOffset, const u64 dataCountPerLoopAixs0, const u64 scratchOffset, TemplateDataParams &tempAlgParamsInter0) const;
    void GenTemplateAlgParamsInter1(const u64 dataOffset, const u64 dataCountPerLoopAixs1, const u64 scratchOffset, TemplateDataParams &tempAlgParamsInter1) const;
    void GetParallelDataSplit(std::vector<float> &splitDataSize) const;
    HcclResult PrepareResForTemplate(const RankGraph *rankGraph, InsAlgTemplate0 &tempAlgIntra, InsAlgTemplate1 &tempAlgInter);
    HcclResult PrepareResForTemplate(ConnectedLinkMgr *linkMgr, InsAlgTemplate0 &tempAlgIntra, InsAlgTemplate1 &tempAlgInter);
    uint64_t rankSizeLevel0_{0};
    uint64_t rankSizeLevel1_{0};

    uint64_t rankIdxLevel0_{0};
    uint64_t rankIdxLevel1_{0};

    const RankGraph *rankGraph_ = nullptr;

    std::vector<std::vector<RankId>>              virtRanks_;
    std::vector<std::map<RankId, u32>>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<std::vector<RankId>>> vTopo_;

    std::vector<InsQuePtr> requiredQue_;
    std::vector<InsQuePtr> intraQue_;
    std::vector<InsQuePtr> interQue_;
    std::vector<InsQuePtr> syncQueues_;
    ResLinks               intraLinks_;
    ResLinks               interLinks_;

    const RankGraph *rankGraphPtr_ = nullptr;
};

} // namespace Hccl

#endif // HCCLV2_INS_ALL_GATHER_PARALLEL_EXECUTOR_H
