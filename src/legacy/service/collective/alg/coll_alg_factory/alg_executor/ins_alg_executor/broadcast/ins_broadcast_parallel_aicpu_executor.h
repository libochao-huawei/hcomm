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
    explicit InsBroadcastParallelAiCpuExecutor() = default;
    ~InsBroadcastParallelAiCpuExecutor() override = default;

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
        u32 interAllGather;
        u32 intraAllGather;
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
        u64 intraAllGatherStage2;
        u64 interAllGatherStage2;
        u64 interAllGatherStage3;
        u64 intraAllGatherStage3;
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
    HcclResult WrapPrepResLinks(const RankGraph* type, const LinkReq& linkReq, ResLinks& resLinks)
    {
        return PrepResLinks(myRank_, type, linkPriority_, linkReq, resLinks);
    }
    HcclResult WrapPrepResLinks(ConnectedLinkMgr* type, const LinkReq& linkReq, ResLinks& resLinks)
    {
        return PrepResLinks(myRank_, linkReq, type, resLinks);
    }
    HcclResult PreCalcRes(
        const RankGraph* rankGraph, AlgTempResReq& resReqIntraScatter, AlgTempResReq& resReqInterScatter,
        AlgTempResReq& resReqIntraAllGather, AlgTempResReq& resReqInterAllGather);
    template <typename T>
    HcclResult CalcSingleAlgRes(
        InsAlgTemplate0& intraScatter, InsAlgTemplate1& interScatter, InsAlgTemplate2& intraAllGather,
        InsAlgTemplate3& interAllGather, T* type, AlgTempResReq& resReqIntraScatter, AlgTempResReq& resReqInterScatter,
        AlgTempResReq& resReqIntraAllGather, AlgTempResReq& resReqInterAllGather);

    template <typename T>
    HcclResult PrepareRes(
        T* type, AlgTempResReq& resReqIntraScatter, AlgTempResReq& resReqInterScatter,
        AlgTempResReq& resReqIntraAllGather, AlgTempResReq& resReqInterAllGather);

    HcclResult CalcLocalRankSize()
    {
        uint64_t virtRanks_2 = 2;
        CHK_PRT_RET(
            virtRanks_.size() < virtRanks_2, HCCL_ERROR("[CalcLocalRankSize] virtRanks level num is smaller than 2."),
            HcclResult::HCCL_E_INTERNAL);

        intraLocalRankSize_ = virtRanks_.at(0).size();
        interLocalRankSize_ = virtRanks_.at(1).size();

        HCCL_INFO(
            "[CalcLocalRankSize] localRankSize: myRank[%d] intraLocalRankSize[%u] interLocalRankSize[%u]", myRank_,
            intraLocalRankSize_, interLocalRankSize_);
        return HcclResult::HCCL_SUCCESS;
    };
    void GetParallelDataSplit(std::vector<float>& splitDataSize)
    {
        // to do 先做等分，后续根据性能做调整
        double splitData = 0.5;
        splitDataSize.push_back(splitData);
        splitDataSize.push_back(splitData);
        return;
    }
    HcclResult CalcLocalRoot()
    {
        CHK_PRT_RET(
            root_ >= rankSize_, HCCL_ERROR("[CalcLocalRoot] root[%u] is out of rankSize[%u]", root_, rankSize_),
            HcclResult::HCCL_E_INTERNAL);

        u32 intraLocalRootIdx = root_ % intraLocalRankSize_;
        intraLocalRoot_ = static_cast<u32>(vTopo_.at(0).at(0).at(intraLocalRootIdx));
        u32 interLocalRootIdx = root_ / intraLocalRankSize_;
        interLocalRoot_ = static_cast<u32>(vTopo_.at(1).at(0).at(interLocalRootIdx));

        HCCL_INFO(
            "[CalcLocalRoot] localRoot: myRank[%d] intraLocalRoot[%u] interLocalRoot[%u]", myRank_, intraLocalRoot_,
            interLocalRoot_);
        return HcclResult::HCCL_SUCCESS;
    }

    void CalcScratchMultiple(
        std::vector<float>& splitDataSize, ScratchMultiple& scratchMultiple, InsAlgTemplate0& intraScatterTempAlg,
        InsAlgTemplate1& interScatterTempAlg, InsAlgTemplate2& intraAllGatherTempAlg,
        InsAlgTemplate3& interAllGatherTempAlg)
    {
        scratchMultiple.intraScatter = intraScatterTempAlg.CalcScratchMultiple(BufferType::INPUT, BufferType::INPUT);
        scratchMultiple.interScatter = interScatterTempAlg.CalcScratchMultiple(BufferType::INPUT, BufferType::INPUT);
        scratchMultiple.intraAllGather =
            intraAllGatherTempAlg.CalcScratchMultiple(BufferType::INPUT, BufferType::INPUT);
        scratchMultiple.interAllGather =
            interAllGatherTempAlg.CalcScratchMultiple(BufferType::INPUT, BufferType::INPUT);
        // 计算第一步需要的倍数和最后一步所需要的数据缓存倍数，取multiple最大需求
        float multiple0 = splitDataSize.at(0) * float(scratchMultiple.interScatter) +
                          splitDataSize.at(1) * float(scratchMultiple.intraScatter);
        float multiple1 = splitDataSize.at(0) * float(scratchMultiple.interAllGather / interLocalRankSize_) +
                          splitDataSize.at(1) * float(scratchMultiple.intraAllGather / intraLocalRankSize_);
        scratchMultiple.maxMultiple = std::max(multiple0, multiple1);
        return;
    }
    void CalcSlice(std::vector<float>& splitDataSize, float scratchMaxMultiple, SliceConfig& slice);
    void CalcScratchOffset(SliceConfig& slice, ScratchMultiple& scratchMultiple, ScratchOffset& scratchOffset);
    void LogAlgInfo(
        InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllGatherTempAlg, InsAlgTemplate3& interAllGatherTempAlg)
    {
        HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Alg0 is [%s]", intraScatterTempAlg.Describe().c_str());
        HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Alg1 is [%s]", interScatterTempAlg.Describe().c_str());
        HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Alg2 is [%s]", intraAllGatherTempAlg.Describe().c_str());
        HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Alg3 is [%s]", interAllGatherTempAlg.Describe().c_str());
        return;
    }
    HcclResult StageProcess(DataParameters& dataParameters, std::vector<StageProcAlgPara>& stageProcAlgParaVec);
    void AlgTemplateInitPara(
        const CollAlgOperator& op, InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllGatherTempAlg, InsAlgTemplate3& interAllGatherTempAlg)
    {
        intraScatterTempAlg.SetDmaMode(dmaMode_);
        intraScatterTempAlg.SetCollOp(op);
        intraScatterTempAlg.SetDataType(dataType_);
        intraScatterTempAlg.SetRoot(intraLocalRoot_);

        interScatterTempAlg.SetDmaMode(dmaMode_);
        interScatterTempAlg.SetCollOp(op);
        interScatterTempAlg.SetDataType(dataType_);
        interScatterTempAlg.SetRoot(interLocalRoot_);

        intraAllGatherTempAlg.SetDmaMode(dmaMode_);
        intraAllGatherTempAlg.SetCollOp(op);
        intraAllGatherTempAlg.SetDataType(dataType_);
        intraAllGatherTempAlg.SetRoot(intraLocalRoot_);

        interAllGatherTempAlg.SetDmaMode(dmaMode_);
        interAllGatherTempAlg.SetCollOp(op);
        interAllGatherTempAlg.SetDataType(dataType_);
        interAllGatherTempAlg.SetRoot(intraLocalRoot_);
        return;
    }
    // Host
    HcclResult PrepareResForTemplate(
        const RankGraph* rankGraph, InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllGatherTempAlg, InsAlgTemplate3& interAllGatherTempAlg);
    // Aicpu
    HcclResult PrepareResForTemplate(
        ConnectedLinkMgr* linkMgr, InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllGatherTempAlg, InsAlgTemplate3& interAllGatherTempAlg);

    void GenDataParamsStage(
        const u64 sliceSize, const u64 inputSliceStride, const u64 dataOffset, const u64 sliceCount,
        const u64 scratchOffsetCount, TemplateDataParams& dataParams) const
    {
        dataParams.buffInfo.inBuffType = BufferType::INPUT;
        dataParams.buffInfo.outBuffType = BufferType::INPUT;
        dataParams.buffInfo.scratBuffType = BufferType::SCRATCH;
        dataParams.buffInfo.inBuffBaseOff = dataOffset;
        dataParams.buffInfo.outBuffBaseOff = dataOffset;
        dataParams.buffInfo.scratchBuffBaseOff = scratchOffsetCount * dataTypeSize_;
        dataParams.sliceSize = sliceSize;
        dataParams.inputSliceStride = inputSliceStride;
        dataParams.outputSliceStride = sliceSize;
        dataParams.repeatNum = 1;
        dataParams.inputRepeatStride = 0;
        dataParams.outputRepeatStride = 0;
        return;
    }
    void InitStageProcAlgParaVec(
        std::vector<StageProcAlgPara>& stageProcAlgParaVec, ScratchOffset& scratchOffset,
        InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllGatherTempAlg, InsAlgTemplate3& interAllGatherTempAlg)
    {
        // 以此输入第1个阶段的part0 GenExtIns, scratchOffset, links,que以及 part1部分对应信息
        stageProcAlgParaVec = {
            {[&](auto&... args) { return interScatterTempAlg.GenExtIns(args...); }, scratchOffset.interScatterStage0,
             interLinks_, interQue_, // stage0 part0
             [&](auto&... args) { return intraScatterTempAlg.GenExtIns(args...); }, scratchOffset.intraScatterStage0,
             intraLinks_, intraQue_}, // stage0 part1
            {[&](auto&... args) { return intraScatterTempAlg.GenExtIns(args...); }, scratchOffset.intraScatterStage1,
             intraLinks_, intraQue_, // stage1 part0
             [&](auto&... args) { return interScatterTempAlg.GenExtIns(args...); }, scratchOffset.interScatterStage1,
             interLinks_, interQue_}, // stage1 part1
            {[&](auto&... args) { return intraAllGatherTempAlg.GenExtIns(args...); },
             scratchOffset.intraAllGatherStage2, intraLinks_, intraQue_, // stage2 part0
             [&](auto&... args) { return interAllGatherTempAlg.GenExtIns(args...); },
             scratchOffset.interAllGatherStage2, interLinks_, interQue_}, // stage2 part1
            {[&](auto&... args) { return interAllGatherTempAlg.GenExtIns(args...); },
             scratchOffset.interAllGatherStage3, interLinks_, interQue_, // stage3 part0
             [&](auto&... args) { return intraAllGatherTempAlg.GenExtIns(args...); },
             scratchOffset.intraAllGatherStage3, intraLinks_, intraQue_}, // stage3 part1
        };
    }
    HcclResult GenInsQues(
        InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllGatherTempAlg, InsAlgTemplate3& interAllGatherTempAlg);

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

    std::vector<InsQuePtr> requiredQue_;
    std::vector<InsQuePtr> intraQue_;
    std::vector<InsQuePtr> interQue_;
    std::vector<InsQuePtr> syncQueues_;
    ResLinks intraLinks_;
    ResLinks interLinks_;

    const RankGraph* rankGraphPtr_ = nullptr;
};

} // namespace Hccl

#endif