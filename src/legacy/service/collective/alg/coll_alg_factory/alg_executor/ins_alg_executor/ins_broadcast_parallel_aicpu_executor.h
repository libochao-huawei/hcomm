/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_INS_BROADCAST_PARALLEL_APICPU_EXECUTOR_H
#define HCCLV2_INS_BROADCAST_PARALLEL_APICPU_EXECUTOR_H

#include "ins_coll_alg_base.h"

namespace Hccl {

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
class InsBroadcastParallelAiCpuExecutor : public InsCollAlgBase {
public:
    explicit InsBroadcastParallelAiCpuExecutor();
    ~InsBroadcastParallelAiCpuExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based BroadCast Parallel AICPU Executor.";
    }

    HcclResult CalcRes(const RankGraph* rankGraph, CollAlgResReq& algResReq) override;

    HcclResult CalcResOffload(const RankGraph* rankGraph, const u64& dataSize, CollOffloadOpResReq& resReq) override;
    // HOST 接口
    HcclResult Orchestrate(
        const RankGraph* rankGraph, const CollAlgOperator& op, const CollAlgParams& params, InsQuePtr insQue) override;
    // AICPU 接口
    HcclResult Orchestrate(
        const AlgTopoInfo& topoInfo, const CollAlgOperator& op, const CollAlgParams& params, ConnectedLinkMgr* linkMgr,
        InsQuePtr insQue) override;

private:
    struct ScratchMultiple {
        u32 interScatter;
        u32 intraScatter;
        u32 interAllReduce;
        u32 intraAllReduce;
        float maxMultiple;
    };
    struct SliceConfig {
        u32 loopTimes;
        u64 sliceCount;           // 正常切分大小
        u64 sliceCountPart0;      // 正常切块第一部分大小
        u64 sliceCountPart1;      // 正常切块第二部分大小
        u64 finalSliceCount;      // 尾块切分大小
        u64 finalSliceCountPart0; // 尾块第一部分切分大小
        u64 finalSliceCountPart1; // 尾块第二部分切分大小
    };
    struct ScratchOffset {
        u64 interScatterStage0;
        u64 intraScatterStage0;
        u64 intraScatterStage1;
        u64 interScatterStage1;
        u64 intraAllReduceStage2;
        u64 interAllReduceStage2;
        u64 interAllReduceStage3;
        u64 intraAllReduceStage3;
    };
    struct DataParameters {
        u64 CountPart0;
        u64 CountPart1;
        u64 dataOffset0;
        u64 dataOffset1;
    };

    struct StageProcAlgPara {
        std::function<HcclResult(TempFuncs&, TemplateDataParams&, ResLinks&, std::vector<InsQuePtr>&)> part0FuncPtr;
        u64 part0ScratchOffset;
        ResLinks part0links;
        std::vector<InsQuePtr> part0Que;
        std::function<HcclResult(TempFuncs&, TemplateDataParams&, ResLinks&, std::vector<InsQuePtr>&)> part1FuncPtr;
        u64 part1ScratchOffset;
        ResLinks part1links;
        std::vector<InsQuePtr> part1Que;
    };

    HcclResult CalcLocalRankSize();
    HcclResult CalcLocalRoot();
    void CalcScratchMultiple(
        std::vector<float>& splitDataSize, ScratchMultiple& scratchMultiple, InsAlgTemplate0& intraScatterTempAlg,
        InsAlgTemplate1& interScatterTempAlg, InsAlgTemplate2& intraAllReduceTempAlg,
        InsAlgTemplate3& interAllReduceTempAlg);
    void CalcSlice(std::vector<float>& splitDataSize, float scratchMaxMultiple, SliceConfig& slice);
    void CalcScratchOffset(SliceConfig& slice, ScratchMultiple& scratchMultiple, ScratchOffset& scratchOffset);
    HcclResult StageProcess(DataParameters& dataParameters, std::vector<StageProcAlgPara>& stageProcAlgParaVec);
    // Host
    HcclResult PrepareResForTemplate(
        const RankGraph* rankGraph, InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllReduceTempAlg, InsAlgTemplate3& interAllReduceTempAlg);
    // Aicpu
    HcclResult PrepareResForTemplate(
        ConnectedLinkMgr* linkMgr, InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllReduceTempAlg, InsAlgTemplate3& interAllReduceTempAlg);

    void GenDataParams(
        const u64 dataOffset, const u64 sliceCount, const u64 scratchOffsetCount, TemplateDataParams& dataParams) const;
    HcclResult GenInsQues(
        InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllReduceTempAlg, InsAlgTemplate3& interAllReduceTempAlg);

    u32 intraLocalRankSize_{0}; // server内算法rankSize
    u32 interLocalRankSize_{0}; // server间算法rankSize

    RankId intraLocalRank_{INVALID_RANKID}; // server内算法rank
    RankId interLocalRank_{INVALID_RANKID}; // server间算法rank

    u32 intraLocalRoot_{0}; // server内算法root
    u32 interLocalRoot_{0}; // server间算法root

    const RankGraph* rankGraph_ = nullptr;

    std::vector<std::vector<std::vector<RankId>>> vTopo_;
    std::vector<std::vector<RankId>> virtRanks_;
    std::vector<std::map<RankId, u32>> virtRankMap_; // map<virtRank, virtRankOrder>

    std::vector<InsQuePtr> intraQue_;
    std::vector<InsQuePtr> interQue_;
    std::vector<InsQuePtr> syncQueues_;
    ResLinks intraLinks_;
    ResLinks interLinks_;

    const RankGraph* rankGraphPtr_ = nullptr;
};

} // namespace Hccl

#endif