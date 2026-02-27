/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ins_all_reduce_parallel_executor_opt.h"

#include <cmath>

#include "log.h"

#include "ins_coll_alg_registry.h"

#include "topo_match_mesh_nhr.h"
#include "topo_match_concurr_mesh_nhr.h"
#include "alg_data_trans_wrapper.h"

#include "ins_temp_reduce_scatter_nhr.h"
#include "ins_temp_reduce_scatter_mesh_1D.h"
#include "ins_temp_reduce_scatter_mesh_2D.h"

#include "ins_temp_all_gather_nhr.h"
#include "ins_temp_all_gather_mesh.h"
#include "ins_temp_all_gather_mesh_2D.h"

namespace Hccl {
constexpr u64 MAX_OFFLOAD_SCRATCH_SIZE = 200 * 1024 * 1024;  // 200M

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::InsAllReduceParallelExecutorV2()
    : InsCollAlgBase()
{
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::~InsAllReduceParallelExecutorV2()
{
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq)
{
    HCCL_INFO("[InsAllReduceParallelExecutorV2] CalcResOffload begins.");
    (void)dataSize;
    u64 scratchMemSize = MAX_OFFLOAD_SCRATCH_SIZE;
    resReq.requiredScratchMemSize = scratchMemSize; // 200MB
    // Topo Match
    AlgTopoMatch topoMatch(myRank_, rankSize_, rankGraph, devType_);
    CHK_RET(topoMatch.MatchTopo(vTopo_, virtRanks_, virtRankMap_));
    CHK_RET(CalcLocalRankSize());

    // 选择算法的template: ReduceScatter
    InsAlgTemplate0 intraTempAlgRS(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 interTempAlgRS(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);

    // ReduceScatter: calculate required insQues and prepare queue
    AlgTempResReq resReqIntraRS;
    AlgTempResReq resReqInterRS;
    
    CHK_RET(intraTempAlgRS.CalcRes(resReqIntraRS));
    CHK_RET(interTempAlgRS.CalcRes(resReqInterRS));

    // 选择算法的template: AllGather
    InsAlgTemplate2 intraTempAlgAG(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 interTempAlgAG(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);

    // AllGather: calculate required insQues and prepare queue
    AlgTempResReq resReqIntraAG;
    AlgTempResReq resReqInterAG;
    
    CHK_RET(intraTempAlgAG.CalcRes(resReqIntraAG));
    CHK_RET(interTempAlgAG.CalcRes(resReqInterAG));

    // 算法从流数量 = Σ(temp的que数量 + temp的从流数量 * temp调用次数) - 算法主流数量
    resReq.requiredSubQueNum = std::max((resReqIntraAG.queNum + resReqInterAG.queNum), (resReqIntraRS.queNum + resReqInterRS.queNum)) - 1;
    HCCL_INFO("[InsAllReduceParallelExecutorV2::CalcResOffload]requiredSubQueNum = %llu", resReq.requiredSubQueNum);

    return HcclResult::HCCL_SUCCESS;
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq)
{
    HCCL_INFO("[InsFourTemplateAllReduceExecutor] CalcRes begins.");
    
    // 拓扑匹配
    AlgTopoMatch topoMatch(myRank_, rankSize_, rankGraph, devType_);
    CHK_RET(topoMatch.MatchTopo(vTopo_, virtRanks_, virtRankMap_));
    algResReq.topoInfo.UpdateMultiLevelTopo(virtRanks_, virtRankMap_, vTopo_);
    CHK_RET(CalcLocalRankSize());

    // 创建四个模板实例
    InsAlgTemplate0 tempAlgRSIntra(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 tempAlgRSInter(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);
    InsAlgTemplate2 tempAlgAGIntra(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 tempAlgAGInter(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);

    // 计算各模板资源需求
    AlgTempResReq resReqRSIntra, resReqRSInter, resReqAGIntra, resReqAGInter;
    
    CHK_RET(tempAlgRSIntra.CalcRes(resReqRSIntra));
    CHK_RET(tempAlgRSInter.CalcRes(resReqRSInter));
    CHK_RET(tempAlgAGIntra.CalcRes(resReqAGIntra));
    CHK_RET(tempAlgAGInter.CalcRes(resReqAGInter));

    // 计算链接信息
    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqRSIntra.links, algResReq.levelRankPairs));
    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqRSInter.links, algResReq.levelRankPairs));
    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqAGIntra.links, algResReq.levelRankPairs));
    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqAGInter.links, algResReq.levelRankPairs));
    
    algResReq.primQueueNum = resReqRSIntra.queNum + resReqRSInter.queNum + 
                            resReqAGIntra.queNum + resReqAGInter.queNum;
    
    HCCL_INFO("[InsFourTemplateAllReduceExecutor::CalcRes] primQueueNum = %u", algResReq.primQueueNum);
    
    std::vector<std::tuple<QId, QId, u32>> notifyRequests;
    for (QId q = 1; q < algResReq.primQueueNum; q++) {
        notifyRequests.emplace_back(std::make_tuple(0, q, 0));
        notifyRequests.emplace_back(std::make_tuple(q, 0, 0));
    }
    
    algResReq.queueNotifys = notifyRequests;
    
    // 计算链接资源
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqRSIntra.links, algResReq.links));
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqRSInter.links, algResReq.links));
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqAGIntra.links, algResReq.links));
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqAGInter.links, algResReq.links));

    return HcclResult::HCCL_SUCCESS;
}

// HOST 侧算法入口，将对应的 instruction 添加到指令队列中
// 传入的insQue为一条主流
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenTemplateAlgParams0(
    const u64 dataOffset, const u64 dataCount, const u64 scratchOffset, TemplateDataParams &tempAlgParams) const
{
    SetCommonTemplateAlgParams(BufferType::INPUT, BufferType::OUTPUT, dataOffset, dataCount, scratchOffset, tempAlgParams);
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenTemplateAlgParams1(
    const u64 dataOffset, const u64 dataCount, const u64 scratchOffset, TemplateDataParams &tempAlgParams) const
{
    SetCommonTemplateAlgParams(BufferType::OUTPUT, BufferType::INPUT, dataOffset, dataCount, scratchOffset, tempAlgParams);
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenTemplateAlgParams2(
    const u64 dataOffset, const u64 dataCount, const u64 scratchOffset, TemplateDataParams &tempAlgParams) const
{
    SetCommonTemplateAlgParams(BufferType::OUTPUT, BufferType::OUTPUT, dataOffset, dataCount, scratchOffset, tempAlgParams);
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GetParallelDataSplitRate(
    std::vector<float> &splitDataSize) const
{
    // to do 先做等分，后续根据性能做调整
    double splitData = 0.5;
    splitDataSize.push_back(splitData);
    splitDataSize.push_back(splitData);
    return;
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::CalcLocalRankSize()
{
    uint64_t virtRanks_2 = 2;
    CHK_PRT_RET(virtRanks_.size() < virtRanks_2,
        HCCL_ERROR("[CalcLocalRankSize] virtRanks level num is smaller than 2."),
        HcclResult::HCCL_E_INTERNAL);

    rankSizeLevel0_ = virtRanks_.at(0).size();
    rankSizeLevel1_ = virtRanks_.at(1).size();

    HCCL_INFO("[CalcLocalRankSize] localRankSize: myRank[%d] rankSizeLevel0_[%u] rankSizeLevel1_[%u]",
        myRank_,
        rankSizeLevel0_,
        rankSizeLevel1_);
    return HcclResult::HCCL_SUCCESS;
};

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::PrepareResForTemplate(
    const RankGraph *rankGraph, InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG)
{
    AlgTempResReq resReqIntraRS;
    AlgTempResReq resReqInterRS;
    AlgTempResReq resReqIntraAG;
    AlgTempResReq resReqInterAG;

    CHK_RET(tempAlgIntraRS.CalcRes(resReqIntraRS));
    CHK_RET(tempAlgInterRS.CalcRes(resReqInterRS));
    CHK_RET(tempAlgIntraAG.CalcRes(resReqIntraAG));
    CHK_RET(tempAlgInterAG.CalcRes(resReqInterAG));

    // 申请算法模板所需资源
    if (!(resReqIntraRS.queNum > 0 && resReqInterRS.queNum > 0 && resReqIntraAG.queNum > 0 && resReqInterAG.queNum > 0)) {
        HCCL_ERROR("resReqIntra.queNum and resReqInter.queNum must larger than 0.");
        return HcclResult::HCCL_E_INTERNAL;
    }
    u32 totalQueueNum = std::max(resReqIntraRS.queNum + resReqInterRS.queNum, resReqIntraAG.queNum + resReqInterAG.queNum);
    CHK_RET(InitQueue(totalQueueNum, requiredQue_));

    for (u32 i = 0; i < requiredQue_.size(); i++) {
        if (i < resReqIntraRS.queNum) {
            intraQue_.push_back(requiredQue_[i]);
        } else {
            interQue_.push_back(requiredQue_[i]);
        }
    }
    syncQueues_.emplace_back(intraQue_[0]);
    syncQueues_.emplace_back(interQue_[0]);

    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqIntraRS.links, intraRSLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqInterRS.links, interRSLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqIntraAG.links, intraAGLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqInterAG.links, interAGLinks_));
    HCCL_INFO("[InsAllReduceParallelExecutorV2] intraRSLinks_ size[%zu], interRSLinks_ size[%zu], intraAGLinks_ size[%zu], interAGLinks_ size[%zu]",
        intraRSLinks_.size(),
        interRSLinks_.size(),
        intraAGLinks_.size(),
        interAGLinks_.size());
    return HCCL_SUCCESS;
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::PrepareResForTemplate(ConnectedLinkMgr *linkMgr,
                                                                                                               InsAlgTemplate0 &tempAlgIntraRS,
                                                                                                               InsAlgTemplate1 &tempAlgInterRS,
                                                                                                               InsAlgTemplate2 &tempAlgIntraAG,
                                                                                                               InsAlgTemplate3 &tempAlgInterAG)
{
    AlgTempResReq resReqIntraRS;
    AlgTempResReq resReqInterRS;
    AlgTempResReq resReqIntraAG;
    AlgTempResReq resReqInterAG;

    CHK_RET(tempAlgIntraRS.CalcRes(resReqIntraRS));
    CHK_RET(tempAlgInterRS.CalcRes(resReqInterRS));
    CHK_RET(tempAlgIntraAG.CalcRes(resReqIntraAG));
    CHK_RET(tempAlgInterAG.CalcRes(resReqInterAG));

    // 申请算法模板所需资源
    if(!(resReqIntraRS.queNum > 0 && resReqInterRS.queNum > 0 && resReqIntraAG.queNum > 0 && resReqInterAG.queNum > 0)) {
        HCCL_ERROR("resReqIntra.queNum and resReqInter.queNum must larger than 0.");
        return HcclResult::HCCL_E_INTERNAL;
    }
    u32 totalQueueNum = std::max(resReqIntraRS.queNum + resReqInterRS.queNum, resReqIntraAG.queNum + resReqInterAG.queNum);
    CHK_RET(InitQueue(totalQueueNum, requiredQue_));
    for(u32 i = 0 ; i < requiredQue_.size(); i++) {
        if (i < resReqIntraRS.queNum) {
            intraQue_.push_back(requiredQue_[i]);
        } else {
            interQue_.push_back(requiredQue_[i]);
        }
    }
    syncQueues_.emplace_back(intraQue_[0]);
    syncQueues_.emplace_back(interQue_[0]);

    CHK_RET(PrepResLinks(myRank_, resReqIntraRS.links, linkMgr, intraRSLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqInterRS.links, linkMgr, interRSLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqIntraAG.links, linkMgr, intraAGLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqInterAG.links, linkMgr, interAGLinks_));
    HCCL_INFO("[InsAllReduceParallelExecutorV2] intraRSLinks_ size[%zu], interRSLinks_ size[%zu], intraAGLinks_ size[%zu], interAGLinks_ size[%zu]",
        intraRSLinks_.size(),
        interRSLinks_.size(),
        intraAGLinks_.size(),
        interAGLinks_.size());
    return HCCL_SUCCESS;
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::CalcSendDataSize(
    u64 &memBlockSize, float &SplitRate, u32 &multipleIntraRS, u32 &multipleInterRS, u32 &multipleIntraAG, u32 &multipleInterAG)
{
    std::vector<float> dataSplitSize;
    GetParallelDataSplitRate(dataSplitSize);
    SplitRate = dataSplitSize[0];
    uint64_t templateNum = 2; // 并行跑的template的数量
    if (multipleIntraRS == 0 && multipleInterRS == 0 && multipleIntraAG == 0 && multipleInterAG == 0) {
        memBlockSize = UB_MAX_DATA_SIZE + UB_MAX_DATA_SIZE; // 暂时先定这么大，小的话效率比较低
    } else if (multipleIntraRS > 0 && multipleInterRS == 0 && multipleIntraAG == 0 && multipleInterAG == 0) {
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleIntraRS) * templateNum;
        Intra0ScratchSize = maxTmpMemSize_;
        Intra1ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS == 0 && multipleInterRS > 0 && multipleIntraAG == 0 && multipleInterAG == 0) {
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleInterRS) * templateNum;
        Inter0ScratchSize = maxTmpMemSize_;
        Inter1ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS == 0 && multipleInterRS == 0 && multipleIntraAG > 0 && multipleInterAG == 0) {
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleIntraAG) * templateNum;
        Intra2ScratchSize = maxTmpMemSize_;
        Intra3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS == 0 && multipleInterRS == 0 && multipleIntraAG == 0 && multipleInterAG > 0) {
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleInterAG) * templateNum;
        Inter2ScratchSize = maxTmpMemSize_;
        Inter3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS > 0 && multipleInterRS > 0 && multipleIntraAG == 0 && multipleInterAG == 0) {
        // multipleIntra >0 && multipleInter >0, 理论上dataSplitSize[0]=0.5时，scratch buffer利用率最大
        u32 subMultiple0 = static_cast<u32>(std::ceil(SplitRate * multipleIntraRS + (1 - SplitRate) * multipleInterRS));
        u32 subMultiple1 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraRS + SplitRate * multipleInterRS));
        u64 totalScratchMultiple = std::max(subMultiple0, subMultiple1);
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_/totalScratchMultiple);

        interScratchOffset0 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraRS);
        interScratchOffset1 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraRS);
        Intra0ScratchSize = interScratchOffset0;
        Inter0ScratchSize = interScratchOffset1;
        Intra1ScratchSize = interScratchOffset1;
        Inter1ScratchSize = interScratchOffset0;
    } else if (multipleIntraRS > 0 && multipleInterRS == 0 && multipleIntraAG > 0 && multipleInterAG == 0) {
        u32 multipleNum = std::max(multipleIntraRS, multipleIntraAG);
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleNum) * templateNum;
        Intra0ScratchSize = maxTmpMemSize_;
        Intra1ScratchSize = maxTmpMemSize_;
        Intra2ScratchSize = maxTmpMemSize_;
        Intra3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS > 0 && multipleInterRS == 0 && multipleIntraAG == 0 && multipleInterAG > 0) {
        u32 multipleNum = std::max(multipleIntraRS, multipleIntraAG);
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleNum) * templateNum;
        Intra0ScratchSize = maxTmpMemSize_;
        Intra1ScratchSize = maxTmpMemSize_;
        Inter2ScratchSize = maxTmpMemSize_;
        Inter3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS == 0 && multipleInterRS > 0 && multipleIntraAG > 0 && multipleInterAG == 0) {
        u32 multipleNum = std::max(multipleIntraRS, multipleIntraAG);
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleNum) * templateNum;
        Inter0ScratchSize = maxTmpMemSize_;
        Inter1ScratchSize = maxTmpMemSize_;
        Intra2ScratchSize = maxTmpMemSize_;
        Intra3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS == 0 && multipleInterRS > 0 && multipleIntraAG == 0 && multipleInterAG > 0) {
        u32 multipleNum = std::max(multipleIntraRS, multipleIntraAG);
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleNum) * templateNum;
        Inter0ScratchSize = maxTmpMemSize_;
        Inter1ScratchSize = maxTmpMemSize_;
        Inter2ScratchSize = maxTmpMemSize_;
        Inter3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS == 0 && multipleInterRS == 0 && multipleIntraAG > 0 && multipleInterAG > 0) {
        u32 subMultiple0 = static_cast<u32>(std::ceil(SplitRate * multipleIntraAG + (1 - SplitRate) * multipleInterAG));
        u32 subMultiple1 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraAG + SplitRate * multipleInterAG));
        u64 totalScratchMultiple = std::max(subMultiple0, subMultiple1);
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_/totalScratchMultiple);

        interScratchOffset2 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraAG);
        interScratchOffset3 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraAG);
        Intra2ScratchSize = interScratchOffset2;
        Inter2ScratchSize = interScratchOffset3;
        Intra3ScratchSize = interScratchOffset3;
        Inter3ScratchSize = interScratchOffset2;
    } else if (multipleIntraRS > 0 && multipleInterRS > 0 && multipleIntraAG > 0 && multipleInterAG == 0) {
        u32 subMultiple0 = static_cast<u32>(std::ceil(SplitRate * multipleIntraRS + (1 - SplitRate) * multipleInterRS));
        u32 subMultiple1 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraRS + SplitRate * multipleInterRS));
        u64 totalScratchMultiple = std::max(subMultiple0, subMultiple1);

        u64 memBlockSizeAG = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleIntraAG) * templateNum;
        u64 memBlockSizeRS = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_/ totalScratchMultiple);
        memBlockSize = std::min(memBlockSizeRS, memBlockSizeAG);

        interScratchOffset0 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraRS);
        interScratchOffset1 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraRS);
        Intra0ScratchSize = interScratchOffset0;
        Inter0ScratchSize = interScratchOffset1;
        Intra1ScratchSize = interScratchOffset1;
        Inter1ScratchSize = interScratchOffset0;

        Intra2ScratchSize = maxTmpMemSize_;
        Intra3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS > 0 && multipleInterRS > 0 && multipleIntraAG == 0 && multipleInterAG > 0) {
        u32 subMultiple0 = static_cast<u32>(std::ceil(SplitRate * multipleIntraRS + (1 - SplitRate) * multipleInterRS));
        u32 subMultiple1 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraRS + SplitRate * multipleInterRS));
        u64 totalScratchMultiple = std::max(subMultiple0, subMultiple1);

        u64 memBlockSizeAG = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleInterAG) * templateNum;
        u64 memBlockSizeRS = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_/ totalScratchMultiple);
        memBlockSize = std::min(memBlockSizeRS, memBlockSizeAG);

        interScratchOffset0 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraRS);
        interScratchOffset1 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraRS);
        Intra0ScratchSize = interScratchOffset0;
        Inter0ScratchSize = interScratchOffset1;
        Intra1ScratchSize = interScratchOffset1;
        Inter1ScratchSize = interScratchOffset0;

        Inter2ScratchSize = maxTmpMemSize_;
        Inter3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS > 0 && multipleInterRS == 0 && multipleIntraAG > 0 && multipleInterAG > 0) {
        u32 subMultiple0 = static_cast<u32>(std::ceil(SplitRate * multipleIntraAG + (1 - SplitRate) * multipleInterAG));
        u32 subMultiple1 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraAG + SplitRate * multipleInterAG));
        u64 totalScratchMultiple = std::max(subMultiple0, subMultiple1);

        u64 memBlockSizeRS = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleInterRS) * templateNum;
        u64 memBlockSizeAG = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_/ totalScratchMultiple);
        memBlockSize = std::min(memBlockSizeRS, memBlockSizeAG);

        interScratchOffset0 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraRS);
        interScratchOffset1 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraRS);
        Intra0ScratchSize = interScratchOffset0;
        Inter0ScratchSize = interScratchOffset1;
        Intra1ScratchSize = interScratchOffset1;
        Inter1ScratchSize = interScratchOffset0;

        Inter2ScratchSize = maxTmpMemSize_;
        Inter3ScratchSize = maxTmpMemSize_;
    } else if (multipleIntraRS == 0 && multipleInterRS > 0 && multipleIntraAG > 0 && multipleInterAG > 0) {
        u32 subMultiple0 = static_cast<u32>(std::ceil(SplitRate * multipleIntraAG + (1 - SplitRate) * multipleInterAG));
        u32 subMultiple1 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraAG + SplitRate * multipleInterAG));
        u64 totalScratchMultiple = std::max(subMultiple0, subMultiple1);

        u64 memBlockSizeRS = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_ / multipleInterRS) * templateNum;
        u64 memBlockSizeAG = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_/ totalScratchMultiple);
        memBlockSize = std::min(memBlockSizeRS, memBlockSizeAG);

        interScratchOffset0 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraAG);
        interScratchOffset1 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraAG);
        Intra0ScratchSize = interScratchOffset0;
        Inter0ScratchSize = interScratchOffset1;
        Intra1ScratchSize = interScratchOffset1;
        Inter1ScratchSize = interScratchOffset0;

        Inter2ScratchSize = maxTmpMemSize_;
        Inter3ScratchSize = maxTmpMemSize_;
    } else {  
        u32 subMultiple0 = static_cast<u32>(std::ceil(SplitRate * multipleIntraRS + (1 - SplitRate) * multipleInterRS));
        u32 subMultiple1 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraRS + SplitRate * multipleInterRS));
        u32 subMultiple2 = static_cast<u32>(std::ceil(SplitRate * multipleIntraAG + (1 - SplitRate) * multipleInterAG));
        u32 subMultiple3 = static_cast<u32>(std::ceil((1 - SplitRate) * multipleIntraAG + SplitRate * multipleInterAG));
        u64 totalScratchMultiple1 = std::max(subMultiple0, subMultiple1);
        u64 totalScratchMultiple2 = std::max(subMultiple2, subMultiple3);
        u64 totalScratchMultiple = std::max(totalScratchMultiple1, totalScratchMultiple2);
        memBlockSize = std::min(static_cast<u64>(UB_MAX_DATA_SIZE), maxTmpMemSize_/totalScratchMultiple);

        interScratchOffset0 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraRS);
        interScratchOffset1 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraRS);
        interScratchOffset2 = static_cast<u64>(memBlockSize * SplitRate * multipleIntraAG);
        interScratchOffset3 = static_cast<u64>(memBlockSize * (1 - SplitRate) * multipleIntraAG);
        Intra0ScratchSize = interScratchOffset0;
        Inter0ScratchSize = interScratchOffset1;
        Intra1ScratchSize = interScratchOffset1;
        Inter1ScratchSize = interScratchOffset0;
        Intra2ScratchSize = interScratchOffset2;
        Inter2ScratchSize = interScratchOffset3;
        Intra3ScratchSize = interScratchOffset3;
        Inter3ScratchSize = interScratchOffset2;
    }

    return HCCL_SUCCESS;
}

/*
 *@Desc: HOST算法编排
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::Orchestrate(
    const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params, InsQuePtr insQue)
{
    HCCL_INFO("[InsAllReduceParallelExecutor] Host Orchestrate begins.");
    
    // init and check params
    CHK_RET(Init(op, params, insQue));

    // Topo Match
    AlgTopoMatch topoMatch(myRank_, rankSize_, rankGraph, devType_);
    CHK_RET(topoMatch.MatchTopo(vTopo_, virtRanks_, virtRankMap_));

    CHK_RET(CalcLocalRankSize());

    // 实例化算法模板类
    InsAlgTemplate0 tempAlgIntraRS(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);  // server内算法，比如mesh
    InsAlgTemplate1 tempAlgInterRS(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);  // server间算法，比如nhr

    InsAlgTemplate2 tempAlgIntraAG(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);  // server内算法，比如mesh
    InsAlgTemplate3 tempAlgInterAG(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);  // server间算法，比如nhr

    tempAlgIntraRS.SetDmaMode(dmaMode_);
    tempAlgIntraRS.InitReduceInfo(redOp_, dataType_);
    tempAlgIntraRS.SetCollOp(op);

    tempAlgInterAG.SetDmaMode(dmaMode_);
    tempAlgInterAG.InitReduceInfo(redOp_, dataType_);
    tempAlgInterAG.SetCollOp(op);

    // 计算算法模板所需资源
    CHK_RET(PrepareResForTemplate(rankGraph, tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG));
    CHK_RET(GenInsQues(tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG));
    HCCL_INFO("[InsAllReduceParallelExecutorV2] Orchestrate success.");

    return HcclResult::HCCL_SUCCESS;
}

/*
 *@Desc: AICPU算法编排
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::Orchestrate(
    const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params, ConnectedLinkMgr *linkMgr,
    InsQuePtr insQue)
{
    HCCL_INFO("[InsAllReduceParallelExecutorV2] AICPU Orchestrate begins.");
    // init and check params
    CHK_RET(Init(op, params, insQue));
    // 获取当前通信域的信息
    vTopo_ = topoInfo.vTopo;              // 本通信域内的通信平面
    virtRankMap_ = topoInfo.virtRankMap;  // 本通信域内的 rank 映射表
    virtRanks_ = topoInfo.virtRanks;      // 本通信域内的 rank 集合
    CHK_RET(CalcLocalRankSize());

    // 实例化算法模板类
    InsAlgTemplate0 tempAlgIntraRS(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);  // server内算法，比如mesh
    InsAlgTemplate1 tempAlgInterRS(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);  // server间算法，比如nhr

    InsAlgTemplate2 tempAlgIntraAG(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);  // server内算法，比如mesh
    InsAlgTemplate3 tempAlgInterAG(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);  // server间算法，比如nhr

    tempAlgIntraRS.SetDmaMode(dmaMode_);
    tempAlgIntraRS.InitReduceInfo(redOp_, dataType_);
    tempAlgIntraRS.SetCollOp(op);

    tempAlgInterAG.SetDmaMode(dmaMode_);
    tempAlgInterAG.InitReduceInfo(redOp_, dataType_);
    tempAlgInterAG.SetCollOp(op);

    // 计算算法模板所需资源
    CHK_RET(PrepareResForTemplate(linkMgr, tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG));
    CHK_RET(GenInsQues(tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG));
    HCCL_INFO("[InsAllReduceParallelExecutorV2] Orchestrate success.");

    return HcclResult::HCCL_SUCCESS;
}

/*
@Desc: 本方法主要实现的是跨框算法实现，如下图，框内和框间分别用不同的算法实现
/-------------------\    /-------------------\
|   /----\ /----\   |    |   /----\ /----\   |
|   |card| |card|   |    |   |card| |card|   |
|   \----/ \----/   |    |   \----/ \----/   |
|                   |    |                   |
|      Machine 1    |    |      Machine 2    |
\-------------------/    \-------------------/
*/
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenInsQues(
    InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG)
{
    u64 alignedSize = 128;  // 假设需要128字节对齐，太大会导致后续maxCountPerLoop计算有问题
    u32 multipleIntraRS = tempAlgIntraRS.CalcScratchMultiple(BufferType::INPUT, BufferType::OUTPUT);
    u32 multipleInterRS = tempAlgInterRS.CalcScratchMultiple(BufferType::INPUT, BufferType::OUTPUT);
    u32 multipleIntraAG = tempAlgIntraAG.CalcScratchMultiple(BufferType::INPUT, BufferType::OUTPUT);
    u32 multipleInterAG = tempAlgInterAG.CalcScratchMultiple(BufferType::INPUT, BufferType::OUTPUT);
    u64 memBlockSize = UB_MAX_DATA_SIZE;
    CalcSendDataSize(memBlockSize, dataSplitRate, multipleIntraRS, multipleInterRS, multipleIntraAG, multipleInterAG);
    // dataSplitSize为分数，这里maxCountPerLoop对10取整，ScratchBufferSize为1M时可能会导致maxCountPerLoop为0；
    u64 maxCountPerLoop = (memBlockSize / dataTypeSize_ / 10 / alignedSize) * 10 * alignedSize;
    CHK_PRT_RET(maxCountPerLoop == 0,
        HCCL_ERROR("[InsAllReduceParallelExecutorV2] memBlockSize:%llu,maxCountPerLoop==0!.", memBlockSize),
        HcclResult::HCCL_E_INTERNAL);
    u32 loopTimes = dataCount_ / maxCountPerLoop + ((dataCount_ % maxCountPerLoop == 0) ? 0 : 1);

    TemplateDataParams tempAlgParamsIntra0;
    TemplateDataParams tempAlgParamsInter0;
    TemplateDataParams tempAlgParamsInter1;
    TemplateDataParams tempAlgParamsIntra1;

    TemplateDataParams tempAlgParamsIntra2;
    TemplateDataParams tempAlgParamsInter2;
    TemplateDataParams tempAlgParamsInter3;
    TemplateDataParams tempAlgParamsIntra3;
    TempFuncs tempFuncs;
    tempFuncs.opMode = opMode_;
    tempFuncs.enableCounterNotify = false;
    tempFuncs.isForepart = true;
    tempFuncs.isBottom = true;

    for (u32 loopIndex = 0; loopIndex < loopTimes; loopIndex++) {
        u64 currCount = (loopIndex == loopTimes - 1) ? (dataCount_ - loopIndex * maxCountPerLoop) : maxCountPerLoop;
        u64 dataCountPerLoopAixs0 = static_cast<u64>(dataSplitRate * currCount);
        u64 dataCountPerLoopAixs1 = currCount - dataCountPerLoopAixs0;
        // 第一步开始前同步
        CHK_RET(PreSyncQues(syncQueues_, 0));
        u64 dataOffset0 = loopIndex * maxCountPerLoop * dataTypeSize_;
        u64 dataOffset1 = dataOffset0 + dataCountPerLoopAixs0 * dataTypeSize_;

        // 第一阶段：前50%的数据做框间ReduceScatter，后50%的数据做框内ReduceScatter
        tempAlgParamsIntra0.buffInfo.scratchBuffSize = Intra0ScratchSize;
        GenTemplateAlgParams0(dataOffset0, dataCountPerLoopAixs0, 0, tempAlgParamsIntra0);
        // 把每个template需要的queue传进去，比如stars的mesh要传多条queue
        CHK_RET(tempAlgIntraRS.GenExtIns(tempFuncs, tempAlgParamsIntra0, intraRSLinks_, intraQue_));

        tempAlgParamsInter0.buffInfo.scratchBuffSize = Inter0ScratchSize;
        GenTemplateAlgParams0(dataOffset1, dataCountPerLoopAixs1, interScratchOffset0, tempAlgParamsInter0);
        CHK_RET(tempAlgInterRS.GenExtIns(tempFuncs, tempAlgParamsInter0, interRSLinks_, interQue_));
        CHK_RET(PostSyncQues(syncQueues_, 0));

        // 第二阶段：前50%的数据做框内框间ReduceScatter，后50%的数据做框内ReduceScatter
        CHK_RET(PostSyncQues(syncQueues_, 0));
        tempAlgParamsInter1.buffInfo.scratchBuffSize = Inter1ScratchSize;
        GenTemplateAlgParams1(dataOffset0, dataCountPerLoopAixs0, interScratchOffset1, tempAlgParamsInter1);
        CHK_RET(tempAlgInterRS.GenExtIns(tempFuncs, tempAlgParamsInter1, interRSLinks_, interQue_));

        tempAlgParamsIntra1.buffInfo.scratchBuffSize = Intra1ScratchSize;
        GenTemplateAlgParams1(dataOffset1, dataCountPerLoopAixs1, 0, tempAlgParamsIntra1);
        CHK_RET(tempAlgIntraRS.GenExtIns(tempFuncs, tempAlgParamsIntra1, intraRSLinks_, intraQue_));
        CHK_RET(PostSyncQues(syncQueues_, 0));

        // 第三阶段：前50%的数据做框间AllGather，后50%的数据做框内AllGather
        CHK_RET(PreSyncQues(syncQueues_, 0));
        tempAlgParamsIntra2.buffInfo.scratchBuffSize = Intra2ScratchSize;
        GenTemplateAlgParams0(dataOffset0, dataCountPerLoopAixs0, 0, tempAlgParamsIntra2);
        CHK_RET(tempAlgIntraAG.GenExtIns(tempFuncs, tempAlgParamsIntra2, intraAGLinks_, intraQue_));

        tempAlgParamsInter2.buffInfo.scratchBuffSize = Inter2ScratchSize;
        GenTemplateAlgParams0(dataOffset1, dataCountPerLoopAixs1, interScratchOffset2, tempAlgParamsInter2);
        CHK_RET(tempAlgInterAG.GenExtIns(tempFuncs, tempAlgParamsInter2, interAGLinks_, interQue_));
        CHK_RET(PostSyncQues(syncQueues_, 0));

        // 第四阶段：前50%的数据做框内框间AllGather，后50%的数据做框内AllGather
        CHK_RET(PreSyncQues(syncQueues_, 0));
        tempAlgParamsInter3.buffInfo.scratchBuffSize = Inter3ScratchSize;
        GenTemplateAlgParams2(dataOffset0, dataCountPerLoopAixs0, interScratchOffset3, tempAlgParamsInter3);
        CHK_RET(tempAlgInterAG.GenExtIns(tempFuncs, tempAlgParamsInter3, interAGLinks_, interQue_));

        tempAlgParamsIntra3.buffInfo.scratchBuffSize = Intra3ScratchSize;
        GenTemplateAlgParams2(dataOffset1, dataCountPerLoopAixs1, 0, tempAlgParamsIntra3);
        CHK_RET(tempAlgIntraAG.GenExtIns(tempFuncs, tempAlgParamsIntra3, intraAGLinks_, intraQue_));
        CHK_RET(PostSyncQues(syncQueues_, 0));
    }

    return HcclResult::HCCL_SUCCESS;
}

// 算法注册
INS_REGISTER_IMPL_BY_FOUR_TEMPS(OpType::ALLREDUCE, InsAllReduceFourTemplateMesh1DNHR, InsAllReduceParallelExecutorV2,
    TopoMatchMeshNHR, InsTempReduceScatterMesh1D, InsTempReduceScatterNHR, InsTempAllGatherMesh1D, InsTempAllGatherNHR);

INS_REGISTER_IMPL_BY_FOUR_TEMPS(OpType::ALLREDUCE, InsAllReduceFourTemplateMesh2DNHR, InsAllReduceParallelExecutorV2,
    TopoMatchConcurrMeshNHR, InsTempReduceScatterMesh2D, InsTempReduceScatterNHR, InsTempAllGatherMesh2D, InsTempAllGatherNHR);

INS_REGISTER_IMPL_BY_FOUR_TEMPS(OpType::ALLREDUCE, InsAllReduceFourTemplateNHRNHR, InsAllReduceParallelExecutorV2,
    TopoMatchMeshNHR, InsTempReduceScatterNHR, InsTempReduceScatterNHR, InsTempAllGatherNHR, InsTempAllGatherNHR);
}
