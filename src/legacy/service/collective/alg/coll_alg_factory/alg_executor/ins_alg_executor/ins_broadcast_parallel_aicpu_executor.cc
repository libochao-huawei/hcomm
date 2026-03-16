/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "log.h"
#include "ins_coll_alg_registry.h"
#include "topo_match_mesh_nhr.h"
#include "alg_data_trans_wrapper.h"

#include "ins_temp_scatter_mesh_1d.h"
#include "ins_temp_scatter_nhr.h"
#include "ins_temp_all_gather_nhr.h"
#include "ins_temp_all_gather_mesh.h"
#include "ins_broadcast_parallel_aicpu_executor.h"

namespace Hccl {

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
template <typename ResourceType>
HcclResult
InsBroadcastParallelAiCpuExecutor<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::
    ProcessAlgResources(
        InsAlgTemplate0& intraScatter, InsAlgTemplate1& interScatter, InsAlgTemplate2& intraAllReduce,
        InsAlgTemplate3& interAllReduce, ResourceType* resource, AlgTempResReq& resReqIntraScatter,
        AlgTempResReq& resReqInterScatter, AlgTempResReq& resReqIntraAllReduce, AlgTempResReq& resReqInterAllReduce)
{
    if (enableDetour_) {
        HCCL_DEBUG("[%s] Rank[%d], CalcRes with detouring enabled.", __func__, myRank_);
        CHK_RET(intraScatter.CalcResDetour(resource, resReqIntraScatter));
        CHK_RET(intraAllReduce.CalcResDetour(resource, resReqIntraAllReduce));
    } else {
        HCCL_DEBUG("[%s] Rank[%d], CalcRes with detouring disabled.", __func__, myRank_);
        CHK_RET(intraScatter.CalcRes(resReqIntraScatter));
        CHK_RET(intraAllReduce.CalcRes(resReqIntraAllReduce));
    }
    CHK_RET(interScatter.CalcRes(resReqInterScatter));
    CHK_RET(interAllReduce.CalcRes(resReqInterAllReduce));
    return HcclResult::HCCL_SUCCESS;
}

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult InsBroadcastParallelAiCpuExecutor<
    AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2,
    InsAlgTemplate3>::CalcRes(const RankGraph* rankGraph, CollAlgResReq& algResReq)
{
    HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] CalcRes start, rank[%d]", myRank_);

    // Topo Match
    AlgTopoMatch topoMatch(myRank_, rankSize_, rankGraph, devType_);
    CHK_RET(topoMatch.MatchTopo(vTopo_, virtRanks_, virtRankMap_));
    algResReq.topoInfo.UpdateMultiLevelTopo(virtRanks_, virtRankMap_, vTopo_);

    // 计算localRankSize
    CHK_RET(CalcLocalRankSize());

    // 实例化算法模板类
    InsAlgTemplate0 intraScatterTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 interScatterTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);
    InsAlgTemplate2 intraAllReduceTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 interAllReduceTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);

    // 计算和准备Queue资源
    AlgTempResReq resReqIntraScatter;
    AlgTempResReq resReqInterScatter;
    AlgTempResReq resReqIntraAllReduce;
    AlgTempResReq resReqInterAllReduce;

    CHK_RET(ProcessAlgResources(
        intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg, rankGraph,
        resReqIntraScatter, resReqInterScatter, resReqIntraAllReduce, resReqInterAllReduce));

    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqIntraScatter.links, algResReq.levelRankPairs));
    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqInterScatter.links, algResReq.levelRankPairs));
    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqIntraAllReduce.links, algResReq.levelRankPairs));
    CHK_RET(CalcLinkInfo(myRank_, rankGraph, resReqInterAllReduce.links, algResReq.levelRankPairs));
    algResReq.primQueueNum = resReqIntraScatter.streamNum + resReqInterScatter.streamNum +
                             resReqIntraAllReduce.streamNum + resReqInterAllReduce.streamNum;

    std::vector<std::tuple<QId, QId, u32>> notifyRequests;

    u32 slaveNum = algResReq.primQueueNum - 1;
    notifyRequests.reserve(slaveNum); // 每个从流需要1个
    for (QId q = 1; q < algResReq.primQueueNum; q++) {
        notifyRequests.emplace_back(std::make_tuple(0, q, 0));
        notifyRequests.emplace_back(std::make_tuple(q, 0, 0));
    }
    algResReq.queueNotifys = notifyRequests;
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqIntraScatter.links, algResReq.links));
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqInterScatter.links, algResReq.links));
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqIntraAllReduce.links, algResReq.links));
    CHK_RET(CalcResLinks(myRank_, rankGraph, linkPriority_, resReqInterAllReduce.links, algResReq.links));

    HCCL_INFO(
        "[InsBroadcastParallelAiCpuExecutor] CalcRes end, rank[%d], required total que num [%u], que notify num [%u]",
        myRank_, algResReq.primQueueNum, algResReq.queueNotifys.size());

    return HcclResult::HCCL_SUCCESS;
}

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult InsBroadcastParallelAiCpuExecutor<
    AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2,
    InsAlgTemplate3>::CalcResOffload(const RankGraph* rankGraph, const u64& dataSize, CollOffloadOpResReq& resReq)
{
    HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] CalcResOffload start, rank[%d]", myRank_);

    (void)dataSize;
    u64 scratchMemSize = 200 * 1024 * 1024;
    resReq.requiredScratchMemSize = scratchMemSize; // 200MB
    // Topo Match
    AlgTopoMatch topoMatch(myRank_, rankSize_, rankGraph, devType_);
    CHK_RET(topoMatch.MatchTopo(vTopo_, virtRanks_, virtRankMap_));

    // 计算localRankSize
    CHK_RET(CalcLocalRankSize());

    // 实例化算法模板类
    InsAlgTemplate0 intraScatterTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 interScatterTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);
    InsAlgTemplate2 intraAllReduceTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 interAllReduceTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);

    // 计算和准备Queue资源
    AlgTempResReq resReqIntraScatter;
    AlgTempResReq resReqInterScatter;
    AlgTempResReq resReqIntraAllReduce;
    AlgTempResReq resReqInterAllReduce;
    CHK_RET(ProcessAlgResources(
        intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg, rankGraph,
        resReqIntraScatter, resReqInterScatter, resReqIntraAllReduce, resReqInterAllReduce));

    resReq.requiredSubQueNum = resReqIntraScatter.streamNum + resReqInterScatter.streamNum +
                               resReqIntraAllReduce.streamNum + resReqInterAllReduce.streamNum - 1;

    HCCL_INFO(
        "[InsBroadcastParallelAiCpuExecutor] CalcResOffload end, rank[%d], required sub que num is [%u]", myRank_,
        resReq.requiredSubQueNum);

    return HcclResult::HCCL_SUCCESS;
}

// Host展开
template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult
InsBroadcastParallelAiCpuExecutor<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::
    Orchestrate(const RankGraph* rankGraph, const CollAlgOperator& op, const CollAlgParams& params, InsQuePtr insQue)
{
    HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Host orchestrate begins.");

    // 初始化参数
    CHK_RET(Init(op, params, insQue));

    // 获取算法Topo信息
    AlgTopoMatch topoMatch(myRank_, rankSize_, rankGraph, devType_);
    CHK_RET(topoMatch.MatchTopo(vTopo_, virtRanks_, virtRankMap_));

    // 计算localRankSize和localRoot
    CHK_RET(CalcLocalRankSize());
    CHK_RET(CalcLocalRoot());

    // 实例化算法模板类
    InsAlgTemplate0 intraScatterTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 interScatterTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);
    InsAlgTemplate2 intraAllReduceTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 interAllReduceTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);

    // 传入Template参数
    AlgTemplateInitPara(op, intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg);

    // 计算算法模板所需资源
    CHK_RET(PrepareResForTemplate(
        rankGraph, intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg));

    // 算法展开
    CHK_RET(GenInsQues(intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg));

    HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Host orchestrate success.");
    return HcclResult::HCCL_SUCCESS;
}

// Aicpu展开
template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult
InsBroadcastParallelAiCpuExecutor<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::
    Orchestrate(
        const AlgTopoInfo& topoInfo, const CollAlgOperator& op, const CollAlgParams& params, ConnectedLinkMgr* linkMgr,
        InsQuePtr insQue)
{
    HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Aicpu orchestrate begins.");

    // 初始化参数
    CHK_RET(Init(op, params, insQue));

    // 获取算法Topo信息
    vTopo_ = topoInfo.vTopo;             // 本通信域内的通信平面
    virtRanks_ = topoInfo.virtRanks;     // 本通信域内的 rank 集合
    virtRankMap_ = topoInfo.virtRankMap; // 本通信域内的 rank 映射表

    // 计算localRankSize和localRoot
    CHK_RET(CalcLocalRankSize());
    CHK_RET(CalcLocalRoot());

    // 实例化算法模板类
    InsAlgTemplate0 intraScatterTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 interScatterTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);
    InsAlgTemplate2 intraAllReduceTempAlg(myRank_, intraLocalRankSize_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 interAllReduceTempAlg(myRank_, interLocalRankSize_, vTopo_[1], virtRankMap_[1]);

    // 传入Template参数
    AlgTemplateInitPara(op, intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg);

    // 计算算法模板所需资源
    CHK_RET(PrepareResForTemplate(
        linkMgr, intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg));

    // 算法展开
    CHK_RET(GenInsQues(intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg));

    HCCL_INFO("[InsBroadcastParallelAiCpuExecutor] Aicpu orchestrate success.");
    return HcclResult::HCCL_SUCCESS;
}

// Host
template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult
InsBroadcastParallelAiCpuExecutor<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::
    PrepareResForTemplate(
        const RankGraph* rankGraph, InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllReduceTempAlg, InsAlgTemplate3& interAllReduceTempAlg)
{
    AlgTempResReq resReqIntraScatter;
    AlgTempResReq resReqInterScatter;
    AlgTempResReq resReqIntraAllReduce;
    AlgTempResReq resReqInterAllReduce;
    CHK_RET(ProcessAlgResources(
        intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg, rankGraph,
        resReqIntraScatter, resReqInterScatter, resReqIntraAllReduce, resReqInterAllReduce));

    // 申请算法模板所需资源
    if (resReqIntraScatter.queNum == 0 || resReqInterScatter.queNum == 0 || resReqIntraAllReduce.queNum == 0 ||
        resReqInterAllReduce.queNum == 0) {
        HCCL_ERROR("queNum  must larger than 0.");
        return HcclResult::HCCL_E_INTERNAL;
    }
    u32 intraQueNum = max(resReqIntraScatter.queNum, resReqIntraAllReduce.queNum);
    u32 interQueNum = max(resReqInterScatter.queNum, resReqInterAllReduce.queNum);
    CHK_RET(InitQueue(intraQueNum, intraQue_));
    CHK_RET(InitQueue(interQueNum, interQue_));

    // 每个算法的第0条流用于同步
    syncQueues_.emplace_back(intraQue_.at(0));
    syncQueues_.emplace_back(interQue_.at(0));

    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqIntraScatter.links, intraLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqInterScatter.links, interLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqIntraAllReduce.links, intraLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqInterAllReduce.links, interLinks_));
    HCCL_INFO(
        "[InsBroadcastParallelAiCpuExecutor] intraLinks size[%zu], interLinks size[%zu]", intraLinks_.size(),
        interLinks_.size());

    return HcclResult::HCCL_SUCCESS;
}

// Aicpu
template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult
InsBroadcastParallelAiCpuExecutor<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::
    PrepareResForTemplate(
        ConnectedLinkMgr* linkMgr, InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllReduceTempAlg, InsAlgTemplate3& interAllReduceTempAlg)
{
    AlgTempResReq resReqIntraScatter;
    AlgTempResReq resReqInterScatter;
    AlgTempResReq resReqIntraAllReduce;
    AlgTempResReq resReqInterAllReduce;
    CHK_RET(ProcessAlgResources(
        intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg, linkMgr,
        resReqIntraScatter, resReqInterScatter, resReqIntraAllReduce, resReqInterAllReduce));

    // 申请算法模板所需资源
    if (resReqIntraScatter.queNum == 0 || resReqInterScatter.queNum == 0 || resReqIntraAllReduce.queNum == 0 ||
        resReqInterAllReduce.queNum == 0) {
        HCCL_ERROR("queNum  must larger than 0.");
        return HcclResult::HCCL_E_INTERNAL;
    }
    u32 intraQueNum = max(resReqIntraScatter.queNum, resReqIntraAllReduce.queNum);
    u32 interQueNum = max(resReqInterScatter.queNum, resReqInterAllReduce.queNum);
    CHK_RET(InitQueue(intraQueNum, intraQue_));
    CHK_RET(InitQueue(interQueNum, interQue_));

    // 每个算法的第0条流用于同步
    syncQueues_.emplace_back(intraQue_.at(0));
    syncQueues_.emplace_back(interQue_.at(0));

    CHK_RET(PrepResLinks(myRank_, resReqIntraScatter.links, linkMgr, intraLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqInterScatter.links, linkMgr, interLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqIntraAllReduce.links, linkMgr, intraLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqInterAllReduce.links, linkMgr, interLinks_));
    HCCL_INFO(
        "[InsBroadcastParallelAiCpuExecutor] intraLinks size[%zu], interLinks size[%zu]", intraLinks_.size(),
        interLinks_.size());

    return HcclResult::HCCL_SUCCESS;
}

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
void InsBroadcastParallelAiCpuExecutor<
    AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2,
    InsAlgTemplate3>::CalcSlice(std::vector<float>& splitDataSize, float scratchMaxMultiple, SliceConfig& slice)
{
    // 数据切分
    u64 sliceCount = std::min(static_cast<u64>(UB_MAX_DATA_SIZE) / dataTypeSize_, dataCount_);
    if (scratchMaxMultiple > 0 && maxTmpMemSize_ > 0) {
        u64 scratchCount = maxTmpMemSize_ / dataTypeSize_; // 按照count来切分
        sliceCount =
            min(sliceCount, static_cast<u64>(float(scratchCount) / scratchMaxMultiple)); // 向下取整，防止Scratch溢出
    }
    /* 刷新slicecout0 和slicecout1确保是interLocalRankSize_ * intraLocalRankSize_整倍数 */
    u64 sliceCountPart0 = static_cast<u64>(float(sliceCount) * splitDataSize.at(0));
    u64 sliceCountPart1 = sliceCount - sliceCountPart0;
    sliceCountPart0 =
        (sliceCountPart0 / interLocalRankSize_ / intraLocalRankSize_) * interLocalRankSize_ * intraLocalRankSize_;
    sliceCountPart1 =
        (sliceCountPart1 / interLocalRankSize_ / intraLocalRankSize_) * interLocalRankSize_ * intraLocalRankSize_;
    if (sliceCountPart0 != 0 && sliceCountPart1 != 0) {
        /*如果sliceCountPart0或者sliceCountPart1为0说明只有一块数据都是尾块*/
        sliceCount = sliceCountPart0 + sliceCountPart1;
    }
    // 计算循环次数
    u32 loopTimes = (dataCount_ + sliceCount - 1) / sliceCount;
    // 计算尾块
    u64 finalSliceCount = dataCount_ - (loopTimes - 1) * sliceCount;
    u64 finalSliceCountPart0 = static_cast<u64>(float(finalSliceCount) * splitDataSize.at(0));
    u64 finalSliceCountPart1 = finalSliceCount - finalSliceCountPart0;
    slice.loopTimes = loopTimes;
    slice.sliceCount = sliceCount;
    slice.sliceCountPart0 = sliceCountPart0;
    slice.sliceCountPart1 = sliceCountPart1;
    slice.finalSliceCount = finalSliceCount;
    slice.finalSliceCountPart0 = finalSliceCountPart0;
    slice.finalSliceCountPart1 = finalSliceCountPart1;
    return;
}

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
void InsBroadcastParallelAiCpuExecutor<
    AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::
    CalcScratchOffset(SliceConfig& slice, ScratchMultiple& scratchMultiple, ScratchOffset& scratchOffset)
{
    // 计算Scratch偏移，数据尾块必然小于常规块，不用额外计算尾块时的Scratch偏移
    scratchOffset.interScatterStage0 = 0;
    scratchOffset.intraScatterStage0 = slice.sliceCountPart0 * scratchMultiple.interScatter;

    scratchOffset.intraScatterStage1 = 0;
    scratchOffset.interScatterStage1 = slice.sliceCountPart0 / interLocalRankSize_ * scratchMultiple.intraScatter;

    scratchOffset.intraAllReduceStage2 = 0;
    scratchOffset.interAllReduceStage2 =
        (slice.sliceCountPart0 / interLocalRankSize_ / intraLocalRankSize_) * scratchMultiple.intraAllReduce;

    scratchOffset.interAllReduceStage3 = 0;
    scratchOffset.intraAllReduceStage3 = (slice.sliceCountPart0 / interLocalRankSize_) * scratchMultiple.interAllReduce;
    return;
}

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult InsBroadcastParallelAiCpuExecutor<
    AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2,
    InsAlgTemplate3>::StageProcess(DataParameters& dataParameters, std::vector<StageProcAlgPara>& algParaVec)
{
    TemplateDataParams tempAlgParams;
    TempFuncs tempFuncs;
    tempFuncs.opMode = opMode_;
    tempFuncs.enableCounterNotify = false;
    for (u32 i = 0; i < algParaVec.size(); i++) {
        bool isFirst = (i == 0);
        // 先处理part0数据
        CHK_RET(PreSyncQues(syncQueues_, 0));
        // 第一步的时候server间topo包含root_的rank进行展开，其它rank不展开
        if ((!isFirst || interLocalRoot_ == root_) && (dataParameters.CountPart0) > 0) {
            GenDataParams(
                dataParameters.dataOffset0, dataParameters.CountPart0, algParaVec.at(i).part0ScratchOffset,
                tempAlgParams);
            CHK_RET(algParaVec.at(i).part0FuncPtr(
                tempFuncs, tempAlgParams, algParaVec.at(i).part0links, algParaVec.at(i).part0Que));
        }
        CHK_RET(PostSyncQues(syncQueues_, 0));
        // 再处理part1数据
        CHK_RET(PreSyncQues(syncQueues_, 0));
        // 第一步的时候server内topo包含root_的rank进行展开，其它rank不展开
        if ((!isFirst || intraLocalRoot_ == root_) && dataParameters.CountPart1 > 0) {
            // 数据1的server内的scatter算法
            GenDataParams(
                dataParameters.dataOffset1, dataParameters.CountPart1, algParaVec.at(i).part1ScratchOffset,
                tempAlgParams);
            CHK_RET(algParaVec.at(i).part1FuncPtr(
                tempFuncs, tempAlgParams, algParaVec.at(i).part1links, algParaVec.at(i).part1Que));
        }
        CHK_RET(PostSyncQues(syncQueues_, 0));
    }
    return HcclResult::HCCL_SUCCESS;
}

template <
    typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2,
    typename InsAlgTemplate3>
HcclResult
InsBroadcastParallelAiCpuExecutor<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::
    GenInsQues(
        InsAlgTemplate0& intraScatterTempAlg, InsAlgTemplate1& interScatterTempAlg,
        InsAlgTemplate2& intraAllReduceTempAlg, InsAlgTemplate3& interAllReduceTempAlg)
{
    LogAlgInfo(intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg, interAllReduceTempAlg);

    std::vector<float> dataSplitSize;
    GetParallelDataSplit(dataSplitSize);
    ScratchMultiple scratchMultiple;
    CalcScratchMultiple(
        dataSplitSize, scratchMultiple, intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg,
        interAllReduceTempAlg);
    SliceConfig slice;
    CalcSlice(dataSplitSize, scratchMultiple.maxMultiple, slice);
    if (slice.sliceCount == 0) {
        HCCL_WARNING("The divisor cannot be zero.");
        return HcclResult::HCCL_SUCCESS;
    }
    ScratchOffset scratchOffset;
    CalcScratchOffset(slice, scratchMultiple, scratchOffset);

    std::vector<StageProcAlgPara> stageProcAlgParaVec;
    InitStageProcAlgParaVec(
        stageProcAlgParaVec, scratchOffset, intraScatterTempAlg, interScatterTempAlg, intraAllReduceTempAlg,
        interAllReduceTempAlg);

    DataParameters dataParameters;
    dataParameters.CountPart0 = slice.sliceCountPart0;
    dataParameters.CountPart1 = slice.sliceCountPart1;
    for (u32 loopIndex = 0; loopIndex < slice.loopTimes - 1; loopIndex++) {
        dataParameters.dataOffset0 = loopIndex * slice.sliceCount * dataTypeSize_;
        dataParameters.dataOffset1 = dataParameters.dataOffset0 + dataParameters.CountPart0 * dataTypeSize_;
        CHK_RET(StageProcess(dataParameters, stageProcAlgParaVec));
    }
    dataParameters.CountPart0 = slice.finalSliceCountPart0;
    dataParameters.CountPart1 = slice.finalSliceCountPart1;
    dataParameters.dataOffset0 = (slice.loopTimes - 1) * slice.sliceCount * dataTypeSize_;
    dataParameters.dataOffset1 = dataParameters.dataOffset0 + dataParameters.CountPart0 * dataTypeSize_;
    CHK_RET(StageProcess(dataParameters, stageProcAlgParaVec));
    return HcclResult::HCCL_SUCCESS;
}

// 算法注册
INS_REGISTER_IMPL_BY_FOUR_TEMPS(
    OpType::BROADCAST, AiCpuInsBroadcastParallelMesh1DNHR, InsBroadcastParallelAiCpuExecutor, TopoMatchMeshNHR,
    InsTempScatterMesh1D, InsTempScatterNHR, InsTempAllGatherMesh1D, InsTempAllGatherNHR);
} // namespace Hccl