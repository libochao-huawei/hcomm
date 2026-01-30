/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库BroadcastParallelExecutor类实现
 * Author: caixin
 * Create: 2025-10-23
 */

#ifndef HCCLV2_INS_BROADCAST_PARALLEL_EXECUTOR_H
#define HCCLV2_INS_BROADCAST_PARALLEL_EXECUTOR_H

#include "ins_coll_alg_base.h"

namespace Hccl {

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1>
class InsBroadcastParallelExecutor : public InsCollAlgBase {
public:
    explicit InsBroadcastParallelExecutor();
    ~InsBroadcastParallelExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based All Gather Parallel Executor.";
    }

    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;
    // HOST 接口
    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                          InsQuePtr insQue) override;
    // AICPU 接口
    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                             ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;

private:
    void GetParallelDataSplit(std::vector<float> &splitDataSize) const;
    HcclResult CalcLocalRankSize();
    HcclResult CalcLocalRoot();
    // Host
    HcclResult PrepareResForTemplate(const RankGraph *rankGraph, InsAlgTemplate0 &tempAlgIntra, 
        InsAlgTemplate1 &tempAlgInter);
    // Aicpu
    HcclResult PrepareResForTemplate(ConnectedLinkMgr *linkMgr, InsAlgTemplate0 &tempAlgIntra, 
        InsAlgTemplate1 &tempAlgInter);
    void GenDataParams(const u64 dataOffset, const u64 sliceCount, const u64 scratchOffsetCount, 
        TemplateDataParams &dataParams) const;
    HcclResult GenInsQues(InsAlgTemplate0 &tempAlgIntra, InsAlgTemplate1 &tempAlgInter);

    u32 intraLocalRankSize_{0};  // server内算法rankSize
    u32 interLocalRankSize_{0};  // server间算法rankSize

    RankId intraLocalRank_{INVALID_RANKID};  // server内算法rank
    RankId interLocalRank_{INVALID_RANKID};  // server间算法rank

    u32 intraLocalRoot_{0};  // server内算法root
    u32 interLocalRoot_{0};  // server间算法root

    const RankGraph *rankGraph_ = nullptr;

    std::vector<std::vector<std::vector<RankId>>> vTopo_;
    std::vector<std::vector<RankId>> virtRanks_;
    std::vector<std::map<RankId, u32>> virtRankMap_; // map<virtRank, virtRankOrder>

    std::vector<InsQuePtr> requiredQue_;
    std::vector<InsQuePtr> intraQue_;
    std::vector<InsQuePtr> interQue_;
    std::vector<InsQuePtr> syncQueues_;
    ResLinks               intraLinks_;
    ResLinks               interLinks_;

    const RankGraph *rankGraphPtr_ = nullptr;
};

} // namespace Hccl

#endif // HCCLV2_INS_BROADCAST_PARALLEL_EXECUTOR_H
