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
    resReq.requiredScratchMemSize = MAX_OFFLOAD_SCRATCH_SIZE; // 200MB
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

    algResReq.primQueueNum = std::max((resReqRSIntra.queNum + resReqRSInter.queNum),
                                      (resReqAGIntra.queNum + resReqAGInter.queNum));
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

    // 计算当前 rank 在各层级中的索引
    if (virtRankMap_[0].find(myRank_) != virtRankMap_[0].end()) {
        rankIdxLevel0_ = virtRankMap_[0][myRank_];
    } else {
        HCCL_ERROR("[CalcLocalRankSize] rank [%d] is not in level 0 topo", myRank_);
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (virtRankMap_[1].find(myRank_) != virtRankMap_[1].end()) {
        rankIdxLevel1_ = virtRankMap_[1][myRank_];
    } else {
        HCCL_ERROR("[CalcLocalRankSize] rank [%d] is not in level 1 topo", myRank_);
        return HcclResult::HCCL_E_INTERNAL;
    }

    HCCL_INFO("[CalcLocalRankSize] localRankSize: myRank[%d] rankSizeLevel0_[%u] rankSizeLevel1_[%u] "
              "rankIdxLevel0_[%llu] rankIdxLevel1_[%llu]",
        myRank_, rankSizeLevel0_, rankSizeLevel1_, rankIdxLevel0_, rankIdxLevel1_);
    return HcclResult::HCCL_SUCCESS;
};

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::PrepareResForTemplate(
    const RankGraph *rankGraph, InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG)
{
    AlgTempResReq resReqIntraRS, resReqInterRS, resReqIntraAG, resReqInterAG;

    CHK_RET(tempAlgIntraRS.CalcRes(resReqIntraRS));
    CHK_RET(tempAlgInterRS.CalcRes(resReqInterRS));
    CHK_RET(tempAlgIntraAG.CalcRes(resReqIntraAG));
    CHK_RET(tempAlgInterAG.CalcRes(resReqInterAG));

    CHK_RET(CalcQue(resReqIntraRS, resReqInterRS, resReqIntraAG, resReqInterAG));

    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqIntraRS.links, intraRSLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqInterRS.links, interRSLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqIntraAG.links, intraAGLinks_));
    CHK_RET(PrepResLinks(myRank_, rankGraph, linkPriority_, resReqInterAG.links, interAGLinks_));
    HCCL_INFO("[InsAllReduceParallelExecutorV2] intraRSLinks_ size[%zu], interRSLinks_ size[%zu], intraAGLinks_ size[%zu], interAGLinks_ size[%zu]",
        intraRSLinks_.size(), interRSLinks_.size(), intraAGLinks_.size(), interAGLinks_.size());
    return HCCL_SUCCESS;
}

template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::PrepareResForTemplate(ConnectedLinkMgr *linkMgr,
                                                                                                               InsAlgTemplate0 &tempAlgIntraRS,
                                                                                                               InsAlgTemplate1 &tempAlgInterRS,
                                                                                                               InsAlgTemplate2 &tempAlgIntraAG,
                                                                                                               InsAlgTemplate3 &tempAlgInterAG)
{
    AlgTempResReq resReqIntraRS, resReqInterRS, resReqIntraAG, resReqInterAG;

    CHK_RET(tempAlgIntraRS.CalcRes(resReqIntraRS));
    CHK_RET(tempAlgInterRS.CalcRes(resReqInterRS));
    CHK_RET(tempAlgIntraAG.CalcRes(resReqIntraAG));
    CHK_RET(tempAlgInterAG.CalcRes(resReqInterAG));

    CHK_RET(CalcQue(resReqIntraRS, resReqInterRS, resReqIntraAG, resReqInterAG));

    CHK_RET(PrepResLinks(myRank_, resReqIntraRS.links, linkMgr, intraRSLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqInterRS.links, linkMgr, interRSLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqIntraAG.links, linkMgr, intraAGLinks_));
    CHK_RET(PrepResLinks(myRank_, resReqInterAG.links, linkMgr, interAGLinks_));
    HCCL_INFO("[InsAllReduceParallelExecutorV2] intraRSLinks_ size[%zu], interRSLinks_ size[%zu], intraAGLinks_ size[%zu], interAGLinks_ size[%zu]",
        intraRSLinks_.size(), interRSLinks_.size(), intraAGLinks_.size(), interAGLinks_.size());
    return HCCL_SUCCESS;
}

// ============================================================================
// RS 阶段的参数生成 (参照 InsReduceScatterParallelExecutor)
// ============================================================================

/*
 * Phase 1, 前半数据: 框内 Mesh ReduceScatter
 *
 * 数据流: INPUT → SCRATCH
 * 含义: 把 M 个框内 rank 的前半数据在共享 INPUT 上做 allreduce，结果写入 SCRATCH。
 *       repeatNum = N, 对每个框间 group 重复一次。
 *
 * Mesh RS 模板内部:
 *   RunReduceScatter: 对 M 个 intra-rank 的数据做 alltoall + reduce
 *   PostCopy: 将归约结果搬运到 outBuff (SCRATCH)
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenRSIntraParams0(
    const u64 dataOffset, const u64 dataCount, const std::vector<u64> &scratchOffVec, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::INPUT;
    params.buffInfo.outBuffType     = BufferType::SCRATCH;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = dataOffset;
    params.buffInfo.outBuffBaseOff  = scratchOffVec[0] + rankIdxLevel0_ * sliceBytes;
    params.buffInfo.scratchBuffBaseOff = scratchOffVec[0];
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = dataSize_;
    params.outputSliceStride    = sliceBytes;
    params.repeatNum            = rankSizeLevel1_;
    params.inputRepeatStride    = dataSize_ * rankSizeLevel0_;
    params.outputRepeatStride   = sliceBytes * rankSizeLevel0_;
}

/*
 * Phase 1, 后半数据: 框间 NHR ReduceScatter
 *
 * 数据流: INPUT → SCRATCH
 * 含义: 把 N 台机器的后半数据通过 NHR reduce-scatter，每台机器得到 1/N 的归约结果。
 *       repeatNum = M, 对每个框内 rank 重复一次。
 *
 * NHR RS 模板内部:
 *   LocalDataCopy: 把本机数据搬到 scratch (isBottom=true 时按 rpt*stride + k*sliceStride 读)
 *   RunNHR: 跨框 NHR 通信 + reduce
 *   PostLocalCopy: 把 myAlgIdx-th slice 从 scratch 搬到 outBuff
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenRSInterParams1(
    const u64 dataOffset, const u64 dataCount, const std::vector<u64> &scratchOffVec, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::INPUT;
    params.buffInfo.outBuffType     = BufferType::SCRATCH;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = dataOffset;
    params.buffInfo.outBuffBaseOff  = scratchOffVec[3];
    params.buffInfo.scratchBuffBaseOff = scratchOffVec[3];
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = dataSize_;
    params.outputSliceStride    = sliceBytes;
    params.repeatNum            = rankSizeLevel0_;
    params.inputRepeatStride    = dataSize_ * rankSizeLevel0_;
    params.outputRepeatStride   = sliceBytes * rankSizeLevel1_;
}

/*
 * Phase 2, 前半数据: 框间 NHR ReduceScatter (第二阶段)
 *
 * 数据流: SCRATCH → OUTPUT
 * 含义: 从 Phase1 Intra RS 的 scratch 结果中读取本 rank 的 N 份数据,
 *       通过 NHR reduce-scatter 跨框归约，结果写入 OUTPUT。
 *       repeatNum = 1。
 *
 * NHR RS 模板内部:
 *   LocalDataCopy: 从 SCRATCH 的 N 个 inter-group 槽位读取数据到新的 scratch 区域
 *   RunNHR: 跨框 NHR 通信 + reduce
 *   PostLocalCopy: 最终结果写入 OUTPUT[dataOffset]
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenRSInterParams0(
    const u64 dataOffset, const u64 dataCount, const std::vector<u64> &scratchOffVec, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::SCRATCH;
    params.buffInfo.outBuffType     = BufferType::OUTPUT;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = scratchOffVec[0] + rankIdxLevel0_ * sliceBytes;
    params.buffInfo.outBuffBaseOff  = dataOffset;
    params.buffInfo.scratchBuffBaseOff = scratchOffVec[2];
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = sliceBytes * rankSizeLevel0_;
    params.outputSliceStride    = sliceBytes;
    params.repeatNum            = 1;
    params.inputRepeatStride    = 0;
    params.outputRepeatStride   = 0;
}

/*
 * Phase 2, 后半数据: 框内 Mesh ReduceScatter (第二阶段)
 *
 * 数据流: SCRATCH → OUTPUT
 * 含义: 从 Phase1 Inter RS 的 scratch 结果中读取数据,
 *       在框内做 mesh reduce-scatter，结果写入 OUTPUT。
 *       repeatNum = 1。
 *
 * Mesh RS 模板内部:
 *   RunReduceScatter: 在框内交换 + reduce
 *   PostCopy: 把归约结果搬到 OUTPUT[dataOffset]
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenRSIntraParams1(
    const u64 dataOffset, const u64 dataCount, const std::vector<u64> &scratchOffVec, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::SCRATCH;
    params.buffInfo.outBuffType     = BufferType::OUTPUT;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = scratchOffVec[3];
    params.buffInfo.outBuffBaseOff  = dataOffset;
    params.buffInfo.scratchBuffBaseOff = scratchOffVec[1];
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = sliceBytes * rankSizeLevel1_;
    params.outputSliceStride    = sliceBytes;
    params.repeatNum            = 1;
    params.inputRepeatStride    = 0;
    params.outputRepeatStride   = 0;
}

// ============================================================================
// AG 阶段的参数生成 (参照 InsAllGatherParallelExecutor, inBuffType 改为 OUTPUT)
// ============================================================================

/*
 * Phase 3, 前半数据: 框内 Mesh AllGather
 *
 * 数据流: OUTPUT → OUTPUT
 * 含义: RS 结果在 OUTPUT 中，同一框内各 rank 拥有相同数据(框间 scatter 的结果)。
 *       Mesh AG 在框内广播/收集，为后续框间 AG 准备数据布局。
 *
 * Mesh AG 模板内部:
 *   LocalCopyToScratch: 如果 OPBASE, 把 output[myRank] 搬到 scratch[myRank]
 *   LocalCopyToUsrOut: 如果 inType != outType 才搬 (这里 OUTPUT==OUTPUT, 跳过)
 *   RunMesh: M 个 rank 互相交换数据
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenAGIntraParams0(
    const u64 dataOffset, const u64 dataCount, const u64 scratchOffset, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::OUTPUT;
    params.buffInfo.outBuffType     = BufferType::OUTPUT;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = dataOffset;
    params.buffInfo.outBuffBaseOff  = rankIdxLevel1_ * rankSizeLevel0_ * dataSize_ + dataOffset;
    params.buffInfo.scratchBuffBaseOff = scratchOffset;
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = 0;
    params.outputSliceStride    = dataSize_;
    params.repeatNum            = 1;
    params.inputRepeatStride    = 0;
    params.outputRepeatStride   = 0;
}

/*
 * Phase 3, 后半数据: 框间 NHR AllGather
 *
 * 数据流: OUTPUT → OUTPUT
 * 含义: RS 结果在 OUTPUT 中，不同框拥有不同数据(框内 scatter 的结果)。
 *       NHR AG 跨框收集，每个 rank 获取所有框的数据。
 *
 * NHR AG 模板内部:
 *   LocalDataCopy: 把 output[myAlgRank] 搬到 scratch[myAlgRank]
 *   RunNHR: 跨框 NHR 通信收集
 *   PostLocalCopy: 把 scratch 中所有 slice 搬到 output 对应位置
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenAGInterParams1(
    const u64 dataOffset, const u64 dataCount, const u64 scratchOffset, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::OUTPUT;
    params.buffInfo.outBuffType     = BufferType::OUTPUT;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = dataOffset;
    params.buffInfo.outBuffBaseOff  = rankIdxLevel0_ * dataSize_ + dataOffset;
    params.buffInfo.scratchBuffBaseOff = scratchOffset;
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = 0;
    params.outputSliceStride    = dataSize_ * rankSizeLevel0_;
    params.repeatNum            = 1;
    params.inputRepeatStride    = 0;
    params.outputRepeatStride   = 0;
}

/*
 * Phase 4, 前半数据: 框间 NHR AllGather (第二阶段)
 *
 * 数据流: OUTPUT → OUTPUT
 * 含义: Phase3 Intra AG 结果在 OUTPUT 中，现在需要跨框分发。
 *       NHR AG 把框内聚合好的数据在框间广播。
 *       repeatNum = M, 每个框内 rank 的数据都需要分发。
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenAGInterParams0(
    const u64 dataOffset, const u64 dataCount, const u64 scratchOffset, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::OUTPUT;
    params.buffInfo.outBuffType     = BufferType::OUTPUT;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = dataOffset;
    params.buffInfo.outBuffBaseOff  = dataOffset;
    params.buffInfo.scratchBuffBaseOff = scratchOffset;
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = dataSize_ * rankSizeLevel0_;
    params.outputSliceStride    = dataSize_ * rankSizeLevel0_;
    params.repeatNum            = rankSizeLevel0_;
    params.inputRepeatStride    = dataSize_;
    params.outputRepeatStride   = dataSize_;
}

/*
 * Phase 4, 后半数据: 框内 Mesh AllGather (第二阶段)
 *
 * 数据流: OUTPUT → OUTPUT
 * 含义: Phase3 Inter AG 结果在 OUTPUT 中，现在需要在框内分发。
 *       Mesh AG 把框间聚合好的数据在框内广播。
 *       repeatNum = N, 每个框间 group 的数据都需要分发。
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
void InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenAGIntraParams1(
    const u64 dataOffset, const u64 dataCount, const u64 scratchOffset, TemplateDataParams &params) const
{
    u64 sliceBytes = dataCount * dataTypeSize_;
    params.buffInfo.inBuffType      = BufferType::OUTPUT;
    params.buffInfo.outBuffType     = BufferType::OUTPUT;
    params.buffInfo.scratBuffType   = BufferType::SCRATCH;
    params.buffInfo.inBuffBaseOff   = dataOffset;
    params.buffInfo.outBuffBaseOff  = dataOffset;
    params.buffInfo.scratchBuffBaseOff = scratchOffset;
    params.sliceSize            = sliceBytes;
    params.inputSliceStride     = dataSize_;
    params.outputSliceStride    = dataSize_;
    params.repeatNum            = rankSizeLevel1_;
    params.inputRepeatStride    = dataSize_ * rankSizeLevel0_;
    params.outputRepeatStride   = dataSize_ * rankSizeLevel0_;
}

/*
 *@Desc: HOST算法编排
 */
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::Orchestrate(
    const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params, InsQuePtr insQue)
{
    HCCL_INFO("[InsAllReduceParallelExecutorV2] Host Orchestrate begins.");

    // init and check params
    CHK_RET(Init(op, params, insQue));

    // Topo Match
    AlgTopoMatch topoMatch(myRank_, rankSize_, rankGraph, devType_);
    CHK_RET(topoMatch.MatchTopo(vTopo_, virtRanks_, virtRankMap_));

    CHK_RET(CalcLocalRankSize());

    // 实例化算法模板类
    InsAlgTemplate0 tempAlgIntraRS(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 tempAlgInterRS(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);
    InsAlgTemplate2 tempAlgIntraAG(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 tempAlgInterAG(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);

    InitAlgCommonParams(tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG, op);

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
    vTopo_ = topoInfo.vTopo;
    virtRankMap_ = topoInfo.virtRankMap;
    virtRanks_ = topoInfo.virtRanks;
    CHK_RET(CalcLocalRankSize());

    // 实例化算法模板类
    InsAlgTemplate0 tempAlgIntraRS(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate1 tempAlgInterRS(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);
    InsAlgTemplate2 tempAlgIntraAG(myRank_, rankSizeLevel0_, vTopo_[0], virtRankMap_[0]);
    InsAlgTemplate3 tempAlgInterAG(myRank_, rankSizeLevel1_, vTopo_[1], virtRankMap_[1]);

    InitAlgCommonParams(tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG, op);

    // 计算算法模板所需资源
    CHK_RET(PrepareResForTemplate(linkMgr, tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG));
    CHK_RET(GenInsQues(tempAlgIntraRS, tempAlgInterRS, tempAlgIntraAG, tempAlgInterAG));
    HCCL_INFO("[InsAllReduceParallelExecutorV2] Orchestrate success.");

    return HcclResult::HCCL_SUCCESS;
}

/*
@Desc: AllReduce = ReduceScatter + AllGather，4 个 template 并行执行

数据流总览 (以 2 机 8 卡为例, M=8 intra-ranks, N=2 inter-ranks):

AllReduce 输入: 每个 rank 在 INPUT 中有 dataSize_ 字节的数据
AllReduce 输出: 每个 rank 在 OUTPUT 中得到 dataSize_ 字节的全归约结果

每次循环迭代处理 currCount 个元素，前后各 50% 并行:

  Phase 1 (RS-1): 框内 RS(前半) ‖ 框间 RS(后半)
  ┌─────────────────────────┐   ┌─────────────────────────┐
  │ Intra Mesh RS (前半)     │   │ Inter NHR RS (后半)      │
  │ INPUT → SCRATCH         │   │ INPUT → SCRATCH          │
  │ M ranks allreduce       │   │ N ranks reduce-scatter   │
  │ repeat=N                │   │ repeat=M                 │
  └─────────────────────────┘   └─────────────────────────┘
          ↓ sync ↓                       ↓ sync ↓

  Phase 2 (RS-2): 框间 RS(前半) ‖ 框内 RS(后半)
  ┌─────────────────────────┐   ┌─────────────────────────┐
  │ Inter NHR RS (前半)      │   │ Intra Mesh RS (后半)     │
  │ SCRATCH → OUTPUT        │   │ SCRATCH → OUTPUT         │
  │ N ranks reduce-scatter  │   │ M ranks allreduce        │
  │ repeat=1                │   │ repeat=1                 │
  └─────────────────────────┘   └─────────────────────────┘
          ↓ sync ↓                       ↓ sync ↓

  Phase 3 (AG-1): 框内 AG(前半) ‖ 框间 AG(后半)
  ┌─────────────────────────┐   ┌─────────────────────────┐
  │ Intra Mesh AG (前半)     │   │ Inter NHR AG (后半)      │
  │ OUTPUT → OUTPUT         │   │ OUTPUT → OUTPUT          │
  │ M ranks gather          │   │ N ranks gather           │
  └─────────────────────────┘   └─────────────────────────┘
          ↓ sync ↓                       ↓ sync ↓

  Phase 4 (AG-2): 框间 AG(前半) ‖ 框内 AG(后半)
  ┌─────────────────────────┐   ┌─────────────────────────┐
  │ Inter NHR AG (前半)      │   │ Intra Mesh AG (后半)     │
  │ OUTPUT → OUTPUT         │   │ OUTPUT → OUTPUT          │
  │ repeat=M                │   │ repeat=N                 │
  └─────────────────────────┘   └─────────────────────────┘
*/
template <typename AlgTopoMatch, typename InsAlgTemplate0, typename InsAlgTemplate1, typename InsAlgTemplate2, typename InsAlgTemplate3>
HcclResult InsAllReduceParallelExecutorV2<AlgTopoMatch, InsAlgTemplate0, InsAlgTemplate1, InsAlgTemplate2, InsAlgTemplate3>::GenInsQues(
    InsAlgTemplate0 &tempAlgIntraRS, InsAlgTemplate1 &tempAlgInterRS, InsAlgTemplate2 &tempAlgIntraAG, InsAlgTemplate3 &tempAlgInterAG)
{
    // ===== 1. 数据切分比例 =====
    std::vector<float> dataSplitSize;
    GetParallelDataSplitRate(dataSplitSize);
    u64 alignedSize = 16 * 1024;  // 16K 对齐

    // ===== 2. RS 阶段 scratch 计算 (参照 InsReduceScatterParallelExecutor) =====
    BufferType inBT = BufferType::INPUT;
    BufferType outBT = BufferType::OUTPUT;
    u32 intraRSMultiple0 = tempAlgIntraRS.CalcScratchMultiple(inBT, outBT);
    u32 interRSMultiple0 = tempAlgInterRS.CalcScratchMultiple(inBT, outBT);
    u32 intraRSMultiple1 = tempAlgIntraRS.CalcScratchMultiple(outBT, outBT);
    u32 interRSMultiple1 = tempAlgInterRS.CalcScratchMultiple(outBT, outBT);

    // NHR RS 的 CalcScratchMultiple 可能返回 0, 需要 fallback 到 rankSize
    if (interRSMultiple0 == 0 || interRSMultiple1 == 0) {
        interRSMultiple0 = rankSizeLevel1_;
        interRSMultiple1 = rankSizeLevel1_;
    }

    u32 rsScratchIntra0 = static_cast<u32>(std::ceil(dataSplitSize[0] * intraRSMultiple0 * rankSizeLevel1_));
    u32 rsScratchIntra1 = static_cast<u32>(std::ceil(dataSplitSize[1] * intraRSMultiple1));
    u32 rsScratchInter1 = static_cast<u32>(std::ceil(dataSplitSize[1] * interRSMultiple0 * rankSizeLevel0_));
    u32 rsScratchInter0 = static_cast<u32>(std::ceil(dataSplitSize[0] * interRSMultiple1));
    u32 rsTotalMultiple = rsScratchIntra0 + rsScratchIntra1 + rsScratchInter0 + rsScratchInter1;

    u64 rsBlockSize = maxTmpMemSize_;
    if (rsTotalMultiple > 0) {
        rsBlockSize = (maxTmpMemSize_ / alignedSize / rsTotalMultiple) * alignedSize;
    }

    u64 rsIntra0Off = 0;
    u64 rsIntra1Off = rsIntra0Off + rsScratchIntra0 * rsBlockSize;
    u64 rsInter0Off = rsIntra1Off + rsScratchIntra1 * rsBlockSize;
    u64 rsInter1Off = rsInter0Off + rsScratchInter0 * rsBlockSize;
    std::vector<u64> rsScratchOffVec = {rsIntra0Off, rsIntra1Off, rsInter0Off, rsInter1Off};

    HCCL_INFO("[GenInsQues] RS scratch: intra0=%llu intra1=%llu inter0=%llu inter1=%llu blockSize=%llu",
              rsIntra0Off, rsIntra1Off, rsInter0Off, rsInter1Off, rsBlockSize);

    // ===== 3. AG 阶段 scratch 计算 (参照 InsAllGatherParallelExecutor) =====
    // AG 和 RS 顺序执行，scratch 空间可复用
    u32 intraAGMultiple0 = tempAlgIntraAG.CalcScratchMultiple(outBT, outBT);
    u32 interAGMultiple0 = tempAlgInterAG.CalcScratchMultiple(outBT, outBT);
    u32 intraAGMultiple1 = tempAlgIntraAG.CalcScratchMultiple(outBT, outBT);
    u32 interAGMultiple1 = tempAlgInterAG.CalcScratchMultiple(outBT, outBT);

    u32 agScratchIntra = static_cast<u32>(std::max(std::ceil(dataSplitSize[0] * intraAGMultiple0),
        std::ceil(dataSplitSize[1] * intraAGMultiple1 * rankSizeLevel1_)));
    u32 agScratchInter = static_cast<u32>(std::max(std::ceil(dataSplitSize[1] * interAGMultiple0),
        std::ceil(dataSplitSize[0] * interAGMultiple1 * rankSizeLevel0_)));
    u32 agTotalMultiple = agScratchIntra + agScratchInter;

    u64 agBlockSize = maxTmpMemSize_;
    if (agTotalMultiple > 0) {
        agBlockSize = (maxTmpMemSize_ / alignedSize / agTotalMultiple) * alignedSize;
    }

    u64 agIntraOff = 0;
    u64 agInterOff = agScratchIntra * agBlockSize;

    HCCL_INFO("[GenInsQues] AG scratch: intraOff=%llu interOff=%llu blockSize=%llu", agIntraOff, agInterOff, agBlockSize);

    // ===== 4. 计算 maxCountPerLoop =====
    u64 rsMaxCount = (std::min(rsBlockSize, static_cast<u64>(UB_MAX_DATA_SIZE)) / dataTypeSize_ / 10) * 10;
    u64 agMaxCount = (std::min(agBlockSize, static_cast<u64>(UB_MAX_DATA_SIZE)) / dataTypeSize_ / 10) * 10;
    u64 maxCountPerLoop = std::min(rsMaxCount, agMaxCount);

    CHK_PRT_RET(maxCountPerLoop == 0,
        HCCL_ERROR("[InsAllReduceParallelExecutorV2] maxCountPerLoop==0! rsBlock=%llu agBlock=%llu", rsBlockSize, agBlockSize),
        HcclResult::HCCL_E_INTERNAL);

    u32 loopTimes = dataCount_ / maxCountPerLoop + ((dataCount_ % maxCountPerLoop == 0) ? 0 : 1);

    HCCL_INFO("[GenInsQues] maxCountPerLoop=%llu loopTimes=%u dataCount_=%llu", maxCountPerLoop, loopTimes, dataCount_);

    // ===== 5. 构建 intraRSQue (mesh RS 需要的 queue, 不含主流) =====
    std::vector<InsQuePtr> intraRSQue;
    if (intraQue_.size() > 1) {
        intraRSQue.assign(intraQue_.begin(), intraQue_.begin() + intraQue_.size() - 1);
    } else {
        intraRSQue = intraQue_;
    }

    // ===== 6. 主循环: 4 个 Phase =====
    TempFuncs tempFuncs;
    tempFuncs.opMode = opMode_;
    tempFuncs.enableCounterNotify = false;

    TemplateDataParams rsIntra0Params, rsInter1Params;
    TemplateDataParams rsInter0Params, rsIntra1Params;
    TemplateDataParams agIntra0Params, agInter1Params;
    TemplateDataParams agInter0Params, agIntra1Params;

    for (u32 loopIndex = 0; loopIndex < loopTimes; loopIndex++) {
        u64 currCount = (loopIndex == loopTimes - 1) ? (dataCount_ - loopIndex * maxCountPerLoop) : maxCountPerLoop;
        u64 countAxis0 = static_cast<u64>(dataSplitSize[0] * currCount);
        u64 countAxis1 = currCount - countAxis0;

        u64 dataOffset0 = loopIndex * maxCountPerLoop * dataTypeSize_;
        u64 dataOffset1 = dataOffset0 + countAxis0 * dataTypeSize_;

        // ────────────── Phase 1: RS-1 ──────────────
        // 前半: 框内 Mesh RS,  后半: 框间 NHR RS
        CHK_RET(PreSyncQues(syncQueues_, 0));

        tempFuncs.isForepart = true;
        tempFuncs.isBottom = false;

        GenRSIntraParams0(dataOffset0, countAxis0, rsScratchOffVec, rsIntra0Params);
        CHK_RET(tempAlgIntraRS.GenExtIns(tempFuncs, rsIntra0Params, intraRSLinks_, intraRSQue));

        GenRSInterParams1(dataOffset1, countAxis1, rsScratchOffVec, rsInter1Params);
        CHK_RET(tempAlgInterRS.GenExtIns(tempFuncs, rsInter1Params, interRSLinks_, interQue_));

        CHK_RET(PostSyncQues(syncQueues_, 0));

        // ────────────── Phase 2: RS-2 ──────────────
        // 前半: 框间 NHR RS,  后半: 框内 Mesh RS
        CHK_RET(PreSyncQues(syncQueues_, 0));

        tempFuncs.isBottom = true;
        GenRSInterParams0(dataOffset0, countAxis0, rsScratchOffVec, rsInter0Params);
        CHK_RET(tempAlgInterRS.GenExtIns(tempFuncs, rsInter0Params, interRSLinks_, interQue_));

        tempFuncs.isBottom = false;
        GenRSIntraParams1(dataOffset1, countAxis1, rsScratchOffVec, rsIntra1Params);
        CHK_RET(tempAlgIntraRS.GenExtIns(tempFuncs, rsIntra1Params, intraRSLinks_, intraRSQue));

        CHK_RET(PostSyncQues(syncQueues_, 0));

        // ────────────── Phase 3: AG-1 ──────────────
        // 前半: 框内 Mesh AG,  后半: 框间 NHR AG
        CHK_RET(PreSyncQues(syncQueues_, 0));

        tempFuncs.isBottom = true;
        tempFuncs.isForepart = true;

        GenAGIntraParams0(dataOffset0, countAxis0, agIntraOff, agIntra0Params);
        CHK_RET(tempAlgIntraAG.GenExtIns(tempFuncs, agIntra0Params, intraAGLinks_, intraQue_));

        GenAGInterParams1(dataOffset1, countAxis1, agInterOff, agInter1Params);
        CHK_RET(tempAlgInterAG.GenExtIns(tempFuncs, agInter1Params, interAGLinks_, interQue_));

        CHK_RET(PostSyncQues(syncQueues_, 0));

        // ────────────── Phase 4: AG-2 ──────────────
        // 前半: 框间 NHR AG,  后半: 框内 Mesh AG
        CHK_RET(PreSyncQues(syncQueues_, 0));

        GenAGInterParams0(dataOffset0, countAxis0, agInterOff, agInter0Params);
        CHK_RET(tempAlgInterAG.GenExtIns(tempFuncs, agInter0Params, interAGLinks_, interQue_));

        GenAGIntraParams1(dataOffset1, countAxis1, agIntraOff, agIntra1Params);
        CHK_RET(tempAlgIntraAG.GenExtIns(tempFuncs, agIntra1Params, intraAGLinks_, intraQue_));

        CHK_RET(PostSyncQues(syncQueues_, 0));
    }

    return HcclResult::HCCL_SUCCESS;
}

// 算法注册
INS_REGISTER_IMPL_BY_FOUR_TEMPS(OpType::ALLREDUCE, InsAllReduceFourTemplateMesh1DNHR, InsAllReduceParallelExecutorV2,
    TopoMatchMeshNHR, InsTempReduceScatterMesh1D, InsTempReduceScatterNHR, InsTempAllGatherMesh1D, InsTempAllGatherNHR);
}
