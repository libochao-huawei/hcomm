/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_reduce_scatter_v_order_preserved_for_910_93_executor.h"
#include "alg_template_register.h"

namespace hccl {

CollReduceScatterVOrderPreservedFor91093Executor::CollReduceScatterVOrderPreservedFor91093Executor(
    const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollReduceScatterVExecutor(dispatcher, topoMatcher)
{
    DMAReduceFlag_ = true;
    CCLMemSlice_ = false;
    desc_.deterministic = DETERMINISTIC_STRICT;
}

void CollReduceScatterVOrderPreservedFor91093Executor::ParseParam(const OpParam& param)
{
    // 是否需要scratch memory（图模式没有cclbuffer，需要额外申请scratchMem）
    scratchMemFlag_ = (workflowMode_ != HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    // 记录图模式总数据量
    if( scratchMemFlag_ ) {
        u64 maxCount = 0;
        const u64* counts = static_cast<const u64*>(param.VDataDes.counts);
        for( u32 i = 0; i < topoAttr_.userRankSize; i++ ){
            maxCount = counts[i] > maxCount ? counts[i] : maxCount;
        }
        maxCount_ = maxCount;
        totalSize_ = maxCount * topoAttr_.userRankSize * SIZE_TABLE[param.VDataDes.dataType];
    }
}

u64 CollReduceScatterVOrderPreservedFor91093Executor::CalcLoopMaxCount(const u32 unitSize)
{
    u64 maxCountPerLoop ;
    if(scratchMemFlag_) {
        maxCountPerLoop = maxCount_;
    } else {
        maxCountPerLoop = inCCLbufferSize_ / topoAttr_.userRankSize / HCCL_MIN_SLICE_ALIGN
        * HCCL_MIN_SLICE_ALIGN / unitSize;
    }
    HCCL_INFO("[CollReduceScatterVOrderPreservedFor91093Executor][CalcLoopMaxCount] maxCountPerLoop = [%llu] .", maxCountPerLoop);
    return maxCountPerLoop;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::CalcCurCountsAndCurDispls(const u64 maxTotalCount,
    std::vector<u64> &countsLeft, std::vector<u64> &displs, std::vector<u64> &curCounts, std::vector<u64> &curDispls,
    bool &finished)
{
    finished = true;
    curCounts.resize(countsLeft.size(), 0);
    curDispls.resize(displs.size(), 0);

    // 先设置本轮的displacements，等于入参displs
    std::copy(displs.begin(), displs.end(), curDispls.begin());

    // 分配好每个rank的counts
    for (auto i = 0U; i < countsLeft.size(); ++i) {
        const auto curCount = countsLeft[i] < maxTotalCount ? countsLeft[i] : maxTotalCount;

        curCounts[i] = curCount;
        countsLeft[i] -= curCount;
        displs[i] += curCount;

        if(countsLeft[i] != 0) {
            finished = false;
        }
    }
    return HCCL_SUCCESS;
}

u32 CollReduceScatterVOrderPreservedFor91093Executor::CalReduceStreamNum(const u32& localRankSize) const
{
    return (1 << static_cast<int>(std::floor(log2(localRankSize))));
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::CalcStreamNum(u32& streamNum)
{
    // 获取超节点内rank数
    u32 devNumInlocalPod = 0;
    u32 rankIdxInPod = 0;
    CHK_RET(topoMatcher_->GetLocalSuperPodRankSize(topoAttr_.userRank, devNumInlocalPod, rankIdxInPod));
    // all2allStreamNum条流给alltoall
    u32 all2allStreamNum = std::min(devNumInlocalPod, DEVICE_EIGHT);
    // reduceStreamNum主流分给alltoall，从流给LocalReduce使用
    u32 reduceStreamNum = std::min(CalReduceStreamNum(devNumInlocalPod) - 1, DEVICE_FOUR);
    // level2StreamNum超节点间reducescatter
    u32 level2StreamNum = std::min(CalReduceStreamNum(topoAttr_.superPodNum) - 1, DEVICE_FOUR);
    // 总流数上限：7（alltoall使用，提前的本地拷贝任务不需要并行）+ 4（LocalReduce使用）
    streamNum = std::max(all2allStreamNum + reduceStreamNum - 1, level2StreamNum);
    
    HCCL_INFO("[%s]tag[%s] all2allStreamNum[%u], reduceStreamNum[%u], level2StreamNum[%u], streamNum[%u]", __func__, tag_.c_str(),
        all2allStreamNum, reduceStreamNum, level2StreamNum, streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::CalcScratchMemSize(u64& scratchMemSize)
{
    scratchMemSize = scratchMemFlag_ ? totalSize_ : 0U;
    HCCL_INFO("[%s]tag[%s] scratchMemSize[%llu]", __func__, tag_.c_str(), scratchMemSize);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::CalcTransportMemType(TransportMemType &inputType,
    TransportMemType &outputType) const
{
    // scratchMemFlag_ 对应图模式场景（图模式没有cclbuffer）, PARAM_INPUT -> userInput
    inputType = scratchMemFlag_ ? TransportMemType::PARAM_INPUT : TransportMemType::CCL_INPUT;
    outputType = scratchMemFlag_ ? TransportMemType::SCRATCH : TransportMemType::CCL_OUTPUT;
    HCCL_INFO("[%s]tag[%s] inputType[%d], outputType[%d]", __func__, tag_.c_str(), inputType, outputType);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::CalcCommInfo(
    std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;
    CHK_RET(CalcTransportMemType(inputType, outputType));
    CHK_RET(CalcLevel1CommInfo(inputType, outputType, opTransport));
    CHK_RET(CalcLevel2CommInfo(inputType, outputType, opTransport));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::CalcLevel1CommInfo(TransportMemType inputType,
    TransportMemType outputType,
    std::vector<LevelNSubCommTransport>& opTransport)
{
    CommParaInfo commParaLevel1(COMM_COMBINE_L1, CommType::COMM_TAG_MESH);
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel1, opTransport[COMM_COMBINE_L1], inputType, outputType));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::CalcLevel2CommInfo(TransportMemType inputType,
    TransportMemType outputType,
    std::vector<LevelNSubCommTransport>& opTransport)
{
    if (topoAttr_.superPodNum > 1) {
        CommParaInfo commParaLevel2(COMM_LEVEL2, CommType::COMM_TAG_MESH);
        CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel2, opTransport[COMM_LEVEL2], inputType, outputType));
    }
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::RunReduceScatterVLevel1(const OpParam &param, ExecMem &execMem,
    SubCommInfo &level1CommInfo)
{
    CHK_RET(ActiveSlaveStreams(param.stream));
    HcclDataType dataType = param.VDataDes.dataType;
    const u32 unitSize = SIZE_TABLE[dataType];
    u32 level1RankSize = level1CommInfo.localRankSize;

    const auto curCounts = static_cast<u64*>(param.VDataDes.counts);
    const auto curDispls = static_cast<u64*>(param.VDataDes.displs);
    GroupSlicesInfo groupSlicesInfoLevel1;
    for (u32 groupId = 0; groupId < topoAttr_.superPodNum; groupId++) {
        MemBlockInfo memInfo;
        u32 groupSlicesOffset = groupId * level1RankSize ;
        for (u32 localRankId = 0; localRankId < level1RankSize; localRankId++) {
            u64 size = curCounts[localRankId + groupSlicesOffset] * unitSize;
            u64 userMemInOffset = curDispls[localRankId + groupSlicesOffset] * unitSize;
            
            memInfo.size.push_back(size);
            memInfo.userInputOffsets.push_back(userMemInOffset);
            memInfo.inputOffsets.push_back(minBiasOffset_ * unitSize * (localRankId + groupSlicesOffset));
            memInfo.outputOffsets.push_back(minBiasOffset_ * unitSize * (localRankId + groupSlicesOffset));
        }
        groupSlicesInfoLevel1.push_back(memInfo);
    }

    all2allOffset_ = topoAttr_.superPodNum > 1 ? 1 : 0;  // 多机场景需要偏移1（给L2预留计算位，减少拷贝次数）
    std::unique_ptr<AlgTemplateBase> level1TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_PLANT_LOCAL_REDUCE, dispatcher_);
    CHK_SMART_PTR_NULL(level1TempAlg);

    // execMem.scratchMem在单算子模式下为cclout，图模式为scrach，因此output传入scrach即可
    CHK_RET(level1TempAlg->Prepare(execMem.inputPtr, execMem.inputMem, execMem.scratchMem, param.stream, 
        algResResp_->slaveStreams, algResResp_->notifiesMain, algResResp_->notifiesAux,
        groupSlicesInfoLevel1, param.reduceType, all2allOffset_, dataType, false, false, true));

    CHK_RET(level1TempAlg->RegisterProfiler(
        (level1CommInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + level1CommInfo.localRank,
        PROF_STAGE_2, HCCL_EXEC_STEP_NOT_SET, param.stream));
    CHK_RET(RunTemplate(level1TempAlg, level1CommInfo));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::RunReduceScatterVLevel2(const OpParam &param, ExecMem &execMem,
    SubCommInfo &level1CommInfo)
{
    u32 level1RankId = level1CommInfo.localRank;
    CHK_RET(CheckCommSize(COMM_LEVEL2, level1RankId + 1));
    SubCommInfo level2CommInfo = GetSubCommInfo(COMM_LEVEL2, level1RankId);
    u32 level1Ranksize = level1CommInfo.localRankSize;
    
    // 切分数据，记录每组的起始偏移和大小（仅1组）
    auto unitSize = SIZE_TABLE[param.VDataDes.dataType];
    u32 level2Ranksize = level2CommInfo.localRankSize;
    const auto curCounts = static_cast<u64*>(param.VDataDes.counts);
    MemBlockInfo memInfo;
    for (u32 localRank = 0; localRank < level2Ranksize; localRank++) {
        u64 size = curCounts[level1RankId + localRank * level1Ranksize] * unitSize;

        memInfo.userInputOffsets.push_back(0);
        memInfo.inputOffsets.push_back(0);
        memInfo.outputOffsets.push_back(0);
        memInfo.size.push_back(size);
    }

    std::unique_ptr<AlgTemplateBase> level2TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_PLANT_LOCAL_REDUCE_COMBINE, dispatcher_);
    CHK_SMART_PTR_NULL(level2TempAlg);

    u32 level1LastRank = level1Ranksize - 1;
    CHK_RET(level2TempAlg->Prepare(execMem.inputMem, execMem.scratchMem,
        param.stream, algResResp_->slaveStreams, algResResp_->notifiesMain, algResResp_->notifiesAux,
        memInfo, param.reduceType, param.VDataDes.dataType, level1RankId == level1LastRank - 1,
        level1RankId == level1LastRank, false));

    CHK_RET(level2TempAlg->RegisterProfiler((level1Ranksize << PROF_RANKSIZE_OFFSET_OF_PLANEID) +
        level1CommInfo.localRank, PROF_STAGE_2, HCCL_EXEC_STEP_NOT_SET, param.stream));
    CHK_RET(RunTemplate(level2TempAlg, level2CommInfo));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVOrderPreservedFor91093Executor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s][CollReduceScatterVOrderPreservedFor91093Executor] ReduceScatterV order preserved run start, tag[%s]", __func__, tag_.c_str());
    
    CHK_RET(CheckCommSize(COMM_COMBINE_L1, COMM_INDEX_0 + 1));
    SubCommInfo level1CommInfo = GetSubCommInfo(COMM_COMBINE_L1, COMM_INDEX_0);

    auto unitSize = SIZE_TABLE[param.VDataDes.dataType];
    const auto curCounts = static_cast<u64*>(param.VDataDes.counts);
    u64 dataSize = execMem.count * unitSize;
    DeviceMem srcMem;

    // 计算minBiasOffset_
    u64 maxCount = *std::max_element(curCounts, curCounts + topoAttr_.userRankSize);
    u64 maxCountPerloop = CalcLoopMaxCount(unitSize);
    minBiasOffset_ = maxCount < maxCountPerloop ? maxCount : maxCountPerloop;

    // COMM_COMBINE_L1 节点内 reduce scatter v
    CHK_RET(RunReduceScatterVLevel1(param, execMem, level1CommInfo));
    // L2 节点间 reduce scatter v
    if (topoAttr_.superPodNum > 1) {
        // TODO: 暂不支持超节点间操作
        HCCL_WARNING("[CollReduceScatterVOrderPreservedFor91093Executor][KernelRun] superPodNum > 1 is not supported yet");
        return HCCL_E_NOT_SUPPORT;
    }
    srcMem = execMem.scratchMem.range(minBiasOffset_ * topoAttr_.userRank * unitSize, dataSize);// Opbase: CO/Sr->UO

    Stream stream = param.stream;
    DeviceMem dstMem = DeviceMem::create(execMem.outputPtr, dataSize);
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, stream));
    HCCL_CONFIG_INFO(HCCL_ALG,"[%s]ReduceScatterV order preserved run success, tag[%s]", __func__, tag_.c_str());
    return HCCL_SUCCESS;
}

REGISTER_EXEC("ReduceScatterVOrderPreservedFor91093Executor", ReduceScatterVOrderPreservedFor91093Executor, CollReduceScatterVOrderPreservedFor91093Executor);
}
