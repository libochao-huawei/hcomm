/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_reduce_scatter_v_deter_executor.h"
#include "alg_template_register.h"

namespace hccl {

CollReduceScatterVDeterExecutor::CollReduceScatterVDeterExecutor(
    const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollReduceScatterVExecutor(dispatcher, topoMatcher)
{
    DMAReduceFlag_ = true;
    CCLMemSlice_ = false;
    isNeedSpaceBorrow_ = false;
}

void CollReduceScatterVDeterExecutor::ParseParam(const OpParam& param)
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

u64 CollReduceScatterVDeterExecutor::CalcLoopMaxCount(const u32 unitSize)
{
    u64 maxCountPerLoop ;
    if(scratchMemFlag_) {
        maxCountPerLoop = maxCount_;
    } else {
        maxCountPerLoop = inCCLbufferSize_ / topoAttr_.userRankSize / HCCL_MIN_SLICE_ALIGN
        * HCCL_MIN_SLICE_ALIGN / unitSize;
    }
    HCCL_INFO("[CollReduceScatterVDeterExecutor][CalcLoopMaxCount] maxCountPerLoop = [%llu] .", maxCountPerLoop);
    return maxCountPerLoop;
}

HcclResult CollReduceScatterVDeterExecutor::CalcCurCountsAndCurDispls(const u64 maxTotalCount,
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

u32 CollReduceScatterVDeterExecutor::CalReduceStreamNum(const u32& localRankSize)
{
    return (1 << static_cast<int>(std::floor(log2(localRankSize))));
}

HcclResult CollReduceScatterVDeterExecutor::CalcStreamNum(u32& streamNum)
{
    // Level0RankSize条流给alltoall，剩下的流给LocalReduce使用
    u32 level0StreamNum = topoAttr_.deviceNumPerAggregation - 1 + CalReduceStreamNum(topoAttr_.deviceNumPerAggregation);
    // level1主流分给alltoall，从流给LocalReduce使用
    u32 level1StreamNum = CalReduceStreamNum(topoAttr_.moduleNum);
    // 总流数上限：7（alltoall使用，提前的本地拷贝任务不需要并行）+ 4（LocalReduce使用）
    streamNum = std::min(std::max(level0StreamNum - 1, level1StreamNum), 
        DEVICE_EIGHT + DEVICE_EIGHT / FACTOR_NUM_TWO - 1);
    HCCL_INFO("[%s]tag[%s] level0StreamNum[%u], level1StreamNum[%u], streamNum[%u]", __func__, tag_.c_str(),
        level0StreamNum, level1StreamNum, streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVDeterExecutor::CalcScratchMemSize(u64& scratchMemSize)
{
    scratchMemSize = scratchMemFlag_ ? totalSize_ : 0U;
    HCCL_INFO("[%s]tag[%s] scratchMemSize[%llu]", __func__, tag_.c_str(), scratchMemSize);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVDeterExecutor::CalcCommInfo(
    std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;
    CHK_RET(CalcTransportMemType(inputType, outputType));
    CHK_RET(CalcLevel0CommInfo(inputType, outputType, opTransport));
    CHK_RET(CalcLevel1CommInfo(inputType, outputType, opTransport));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVDeterExecutor::CalcTransportMemType(TransportMemType &inputType,
    TransportMemType &outputType)
{
    // scratchMemFlag_ 对应图模式场景（图模式没有cclbuffer）, PARAM_INPUT -> userInput
    inputType = scratchMemFlag_ ? TransportMemType::PARAM_INPUT : TransportMemType::CCL_INPUT;
    outputType = scratchMemFlag_ ? TransportMemType::SCRATCH : TransportMemType::CCL_OUTPUT;
    HCCL_INFO("[%s]tag[%s] inputType[%d], outputType[%d]", __func__, tag_.c_str(), inputType, outputType);
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVDeterExecutor::CalcLevel0CommInfo(TransportMemType inputType,
    TransportMemType outputType,
    std::vector<LevelNSubCommTransport>& opTransport)
{
    CommParaInfo commParaLevel0(COMM_LEVEL0, CommType::COMM_TAG_MESH);
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel0, opTransport[COMM_LEVEL0], inputType, outputType));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVDeterExecutor::CalcLevel1CommInfo(TransportMemType inputType,
    TransportMemType outputType, std::vector<LevelNSubCommTransport>& opTransport)
{
    if (topoAttr_.moduleNum > 1) {
        CommParaInfo commParaLevel1(COMM_LEVEL1, CommType::COMM_TAG_MESH);
        CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel1, opTransport[COMM_LEVEL1], inputType, outputType));
    }
    return HCCL_SUCCESS;
}
bool CollReduceScatterVDeterExecutor::IsContainZeroSlice(const OpParam &param)
{
    // 对于RSv counts中含0的场景，不进行子图复用
    bool isContainZero = false;
    const auto curCounts = static_cast<u64*>(param.VDataDes.counts);
    auto it = std::find(curCounts, curCounts + topoAttr_.userRankSize, 0ULL); 
    return (it != curCounts + topoAttr_.userRankSize);
}

bool CollReduceScatterVDeterExecutor::IsHugeData(const u64 curSize, const OpParam &param)
{
    bool hugeData = (curSize * topoAttr_.userRankSize / HCCL_INTERNODE_MAX_DATA_RATE > RDMA_SEND_MAX_SIZE) ||
                            (curSize > SDMA_SEND_MAX_SIZE);
    return hugeData || IsContainZeroSlice(param);
}

HcclResult CollReduceScatterVDeterExecutor::RunReduceScattervLevel0(const OpParam &param, ExecMem &execMem,
    SubCommInfo &level0CommInfo)
{
    CHK_RET(ActiveSlaveStreams(param.stream));
    HcclDataType dataType = param.VDataDes.dataType;
    const u32 unitSize = SIZE_TABLE[dataType];
    u32 level0RankSize = level0CommInfo.localRankSize;

    const auto curCounts = static_cast<u64*>(param.VDataDes.counts);
    const auto curDispls = static_cast<u64*>(param.VDataDes.displs);
    GroupSlicesInfo groupSlicesInfoLevel0;
    for (u32 groupId = 0; groupId < topoAttr_.moduleNum; groupId++) {
        MemBlockInfo memInfo;
        u32 groupSlicesOffset = groupId * level0RankSize ;
        for (u32 localRankId = 0; localRankId < level0RankSize; localRankId++) {
            u64 size = curCounts[localRankId + groupSlicesOffset] * unitSize;
            u64 userMemInOffset = curDispls[localRankId + groupSlicesOffset] * unitSize;
            
            memInfo.size.push_back(size);
            memInfo.userInputOffsets.push_back(userMemInOffset);
            memInfo.inputOffsets.push_back(minBiasOffset_ * unitSize * (localRankId + groupSlicesOffset));
            memInfo.outputOffsets.push_back(minBiasOffset_ * unitSize * (localRankId + groupSlicesOffset));
        }
        groupSlicesInfoLevel0.push_back(memInfo);
    }

    all2allOffset_ = topoAttr_.moduleNum > 1 ? 1 : 0;  // 多机场景需要偏移1（给L1预留计算位，减少拷贝次数） 
    std::unique_ptr<AlgTemplateBase> level0TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_PLANT_LOCAL_REDUCE, dispatcher_);
    CHK_SMART_PTR_NULL(level0TempAlg);

    // execMem.scratchMem在单算子模式下为cclout，图模式为scrach，因此output传入scrach即可
    CHK_RET(level0TempAlg->Prepare(execMem.inputPtr, execMem.inputMem, execMem.scratchMem, param.stream, 
        algResResp_->slaveStreams, algResResp_->notifiesMain, algResResp_->notifiesAux,
        groupSlicesInfoLevel0, param.reduceType, all2allOffset_, dataType, isNeedSpaceBorrow_)); 

    CHK_RET(level0TempAlg->RegisterProfiler(
        (level0CommInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + level0CommInfo.localRank,
        PROF_STAGE_2, HCCL_EXEC_STEP_NOT_SET, param.stream));
    CHK_RET(RunTemplate(level0TempAlg, level0CommInfo));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVDeterExecutor::RunReduceScattervLevel1(const OpParam &param, ExecMem &execMem,
    SubCommInfo &level0CommInfo)
{
    u32 level0RankId = level0CommInfo.localRank;
    CHK_RET(CheckCommSize(COMM_LEVEL1, level0RankId + 1));
    SubCommInfo level1CommInfo = GetSubCommInfo(COMM_LEVEL1, level0RankId);
    u32 level0Ranksize = level0CommInfo.localRankSize;
    
    // 切分数据，记录每组的起始偏移和大小（仅1组）
    auto unitSize = SIZE_TABLE[param.VDataDes.dataType];
    u32 inputBaseIndex = (all2allOffset_ + level0RankId) % level0Ranksize; // 多机场景需要偏移1（给L1预留计算位，减少拷贝次数） 
    u32 level1Ranksize = level1CommInfo.localRankSize;
    const auto curCounts = static_cast<u64*>(param.VDataDes.counts);
    MemBlockInfo memInfo;
    for (u32 localRank = 0; localRank < level1Ranksize; localRank++) {
        u64 inputIndex = inputBaseIndex + localRank * level0Ranksize;
        u64 outputIndex = level0RankId + localRank * level0Ranksize;
        u64 size = curCounts[level0RankId +  localRank * level0Ranksize] * unitSize;
 
        memInfo.userInputOffsets.push_back(minBiasOffset_ * unitSize * outputIndex);
        memInfo.inputOffsets.push_back(minBiasOffset_ * unitSize* inputIndex);
        memInfo.outputOffsets.push_back(minBiasOffset_ * unitSize * outputIndex);
        memInfo.size.push_back(size);
    }

    std::unique_ptr<AlgTemplateBase> level1TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_PLANT_LOCAL_REDUCE_COMBINE, dispatcher_);
    CHK_SMART_PTR_NULL(level1TempAlg);

    u32 level0LastRank = level0Ranksize - 1;
    CHK_RET(level1TempAlg->Prepare(execMem.inputMem, execMem.scratchMem,
        param.stream, algResResp_->slaveStreams, algResResp_->notifiesMain, algResResp_->notifiesAux,
        memInfo, param.reduceType, param.VDataDes.dataType, level0RankId == level0LastRank - 1,
        level0RankId == level0LastRank, isNeedSpaceBorrow_));

    CHK_RET(level1TempAlg->RegisterProfiler((level0Ranksize << PROF_RANKSIZE_OFFSET_OF_PLANEID) +
        level0CommInfo.localRank, PROF_STAGE_2, HCCL_EXEC_STEP_NOT_SET, param.stream));
    CHK_RET(RunTemplate(level1TempAlg, level1CommInfo));
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterVDeterExecutor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s][CollReduceScatterVDeterExecutor] ReduceScatterV deter run start, tag[%s]", __func__, tag_.c_str());
    
    auto unitSize = SIZE_TABLE[param.VDataDes.dataType];
    const auto curCounts = static_cast<u64*>(param.VDataDes.counts);
    u64 maxCount = *std::max_element(curCounts, curCounts + topoAttr_.userRankSize);
    u64 maxCountPerloop = CalcLoopMaxCount(unitSize);
    minBiasOffset_ = maxCount < maxCountPerloop ? maxCount : maxCountPerloop;

    CHK_RET(CheckCommSize(COMM_LEVEL0, COMM_INDEX_0 + 1));
    SubCommInfo level0CommInfo = GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);
    // L0 节点内 reduce scatter v
    CHK_RET(RunReduceScattervLevel0(param, execMem, level0CommInfo));
    
    // L1 节点间 reduce scatter v
    if (topoAttr_.moduleNum > 1) {
        CHK_RET(RunReduceScattervLevel1(param, execMem, level0CommInfo));
    }

    u64 dataSize = execMem.count * unitSize;
    DeviceMem srcMem = execMem.scratchMem.range(minBiasOffset_ * topoAttr_.userRank * unitSize, dataSize);
    DeviceMem dstMem = DeviceMem::create(execMem.outputPtr, dataSize);
    Stream stream = param.stream;
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, stream));

    HCCL_INFO("[%s]ReduceScatterV deter run success, tag[%s]", __func__, tag_.c_str());
    return HCCL_SUCCESS;    
}

REGISTER_EXEC("ReduceScatterVDeterExecutor", ReduceScatterVDeterExecutor, CollReduceScatterVDeterExecutor);
}
