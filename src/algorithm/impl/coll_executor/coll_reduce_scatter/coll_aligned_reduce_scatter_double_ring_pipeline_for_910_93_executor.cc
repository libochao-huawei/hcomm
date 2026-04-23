/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_aligned_reduce_scatter_double_ring_pipeline_for_910_93_executor.h"
#include "alg_template_register.h"

namespace hccl {
CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::
    CollAlignedReduceScatterDoubleRingPipelineFor91093Executor(
        const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollAlignedReduceScatterDoubleRingFor91093Executor(dispatcher, topoMatcher)
{
}

HcclResult CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::CalcStreamNum(u32 &streamNum)
{
    CHK_RET(CollReduceScatterRingFor91093Executor::CalcStreamNum(streamNum));
    streamNum += 1;
    HCCL_INFO("[CollAlignedReduceScatterDoubleRingPipelineFor91093Executor][CalcStreamNum] tag[%s] streamNum[%u]",
        tag_.c_str(), streamNum);
    return HCCL_SUCCESS;
}

u64 CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::CalcLoopMaxCount(const u32 unitSize)
{
    u64 maxCountPerLoop = CollReduceScatterRingFor91093Executor::CalcLoopMaxCount(unitSize);
    maxCountPerLoop /= 2; // double buffer: split CclIn/CclOut in half
    HCCL_INFO("[CollAlignedReduceScatterDoubleRingPipelineFor91093Executor][CalcLoopMaxCount] "
        "tag[%s] maxCountPerLoop[%llu]", tag_.c_str(), maxCountPerLoop);
    return maxCountPerLoop;
}

HcclResult CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::RunLoop(
    OpParam &param, AlgResourceResponse &algRes)
{
    const u32 unitSize = SIZE_TABLE[param.DataDes.dataType];
    const ReduceType reduceType = ((param.reduceType != HCCL_REDUCE_PROD) &&
        (param.DataDes.dataType != HCCL_DATA_TYPE_INT64)) ?
        ReduceType::INLINE_REDUCE : ReduceType::TBE_REDUCE;

    // double buffer: split CclIn and CclOut in half
    const u64 cclInputSizeHalved = algRes.cclInputMem.size() / 2;
    DeviceMem cclInputAMem = algRes.cclInputMem.range(0, cclInputSizeHalved);
    DeviceMem cclInputBMem = algRes.cclInputMem.range(cclInputSizeHalved, cclInputSizeHalved);

    const u64 cclOutputSizeHalved = algRes.cclOutputMem.size() / 2;
    DeviceMem cclOutputAMem = algRes.cclOutputMem.range(0, cclOutputSizeHalved);
    DeviceMem cclOutputBMem = algRes.cclOutputMem.range(cclOutputSizeHalved, cclOutputSizeHalved);

    u8 *curInputPtr = static_cast<u8 *>(param.inputPtr);
    u8 *curOutputPtr = static_cast<u8 *>(param.outputPtr);
    CHK_PTR_NULL(curInputPtr);
    CHK_PTR_NULL(curOutputPtr);

    if (param.DataDes.count == 0) {
        return HCCL_SUCCESS;
    }

    const u64 countDataPerLoop = CalcLoopMaxCount(unitSize);
    CHK_PRT_RET(countDataPerLoop == 0,
        HCCL_ERROR("[CollAlignedReduceScatterDoubleRingPipelineFor91093Executor][RunLoop]"
            "countDataPerLoop is zero."),
        HCCL_E_INTERNAL);

    const u64 countDataLastLoopTemp = param.DataDes.count % countDataPerLoop;
    const u64 countDataLastLoop = countDataLastLoopTemp > 0 ? countDataLastLoopTemp : countDataPerLoop;
    const u64 sizeDataPerLoop = countDataPerLoop * unitSize;
    const u64 numBlockTotal = (param.DataDes.count - countDataLastLoop) / countDataPerLoop + 1;
    const u64 numLoopTotal = numBlockTotal + 1; // +1 for drain phase

    Stream streamL0L1 = param.stream;
    Stream streamL2 = algResResp_->slaveStreams.back();
    auto notifyL0L1toL2 = algResResp_->notifiesAux.back();
    auto notifyL2toL0L1 = algResResp_->notifiesMain.back();

    HCCL_INFO("[CollAlignedReduceScatterDoubleRingPipelineFor91093Executor][RunLoop] "
        "tag[%s] numBlockTotal[%llu] countDataPerLoop[%llu] countDataLastLoop[%llu]",
        param.tag.c_str(), numBlockTotal, countDataPerLoop, countDataLastLoop);

    for (u64 i = 0; i < numLoopTotal; ++i) {
        // L0+L1 phase on StreamL0L1
        if (i < numBlockTotal) {
            if (i >= 2) {
                CHK_RET(LocalNotify::Wait(streamL0L1, dispatcher_, notifyL2toL0L1, INVALID_VALUE_STAGE));
            }

            const bool useBufferA = (i % 2 == 0);
            const bool isLastBlock = (i == numBlockTotal - 1);
            ExecMem execMem;
            execMem.count = isLastBlock ? countDataLastLoop : countDataPerLoop;
            execMem.inputMem = useBufferA ? cclInputAMem : cclInputBMem;
            execMem.outputMem = useBufferA ? cclOutputAMem : cclOutputBMem;
            execMem.scratchMem = execMem.outputMem;
            execMem.inputPtr = curInputPtr + i * sizeDataPerLoop;
            execMem.outputPtr = curOutputPtr + i * sizeDataPerLoop;

            CHK_RET(RunLevel0To1(param, reduceType, execMem, streamL0L1));

            CHK_RET(LocalNotify::Post(streamL0L1, dispatcher_, notifyL0L1toL2, INVALID_VALUE_STAGE));
        }

        // L2 phase on StreamL2
        if (i >= 1 && i <= numBlockTotal) {
            CHK_RET(LocalNotify::Wait(streamL2, dispatcher_, notifyL0L1toL2, INVALID_VALUE_STAGE));

            const u64 prevIdx = i - 1;
            const bool prevUseBufferA = (prevIdx % 2 == 0);
            const bool isLastBlock = (prevIdx == numBlockTotal - 1);
            ExecMem execMem;
            execMem.count = isLastBlock ? countDataLastLoop : countDataPerLoop;
            execMem.inputMem = prevUseBufferA ? cclInputAMem : cclInputBMem;
            execMem.outputMem = prevUseBufferA ? cclOutputAMem : cclOutputBMem;
            execMem.scratchMem = execMem.outputMem;
            execMem.inputPtr = curInputPtr + prevIdx * sizeDataPerLoop;
            execMem.outputPtr = curOutputPtr + prevIdx * sizeDataPerLoop;

            CHK_RET(RunLevel2(param, reduceType, execMem, streamL2));

            CHK_RET(LocalNotify::Post(streamL2, dispatcher_, notifyL2toL0L1, INVALID_VALUE_STAGE));
        }
    }

    // PostSync: consume remaining notifyL2toL0L1 signals
    const u64 remainingSignals = (numBlockTotal >= 2) ? 2 : numBlockTotal;
    for (u64 s = 0; s < remainingSignals; ++s) {
        CHK_RET(LocalNotify::Wait(streamL0L1, dispatcher_, notifyL2toL0L1, INVALID_VALUE_STAGE));
    }

    return HCCL_SUCCESS;
}

HcclResult CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::RunLevel0To1(
    OpParam &param, const ReduceType &reduceType, ExecMem &execMem, Stream &streamL0L1)
{
    (void)reduceType;
    u32 unitSize = SIZE_TABLE[param.DataDes.dataType];
    u64 curSize = execMem.count * unitSize;

    if (CCLMemSlice_) {
        u32 sliceNum = topoAttr_.userRankSize;
        execMem.inputMem = execMem.inputMem.range(0, curSize * sliceNum);
        execMem.outputMem = execMem.outputMem.range(0, curSize);
    }

    HcclResult ret = KernelRunLevel0To1(param, execMem, streamL0L1);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RunLevel0To1] kernel run error, tag[%s]", param.tag.c_str()), ret);

    return HCCL_SUCCESS;
}

HcclResult CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::RunLevel2(
    OpParam &param, const ReduceType &reduceType, ExecMem &execMem, Stream &streamL2)
{
    (void)reduceType;
    u32 unitSize = SIZE_TABLE[param.DataDes.dataType];
    u64 curSize = execMem.count * unitSize;

    if (CCLMemSlice_) {
        u32 sliceNum = topoAttr_.userRankSize;
        execMem.inputMem = execMem.inputMem.range(0, curSize * sliceNum);
        execMem.outputMem = execMem.outputMem.range(0, curSize);
    }

    HcclResult ret = KernelRunLevel2(param, execMem, streamL2);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RunLevel2] kernel run error, tag[%s]", param.tag.c_str()), ret);

    return HCCL_SUCCESS;
}

HcclResult CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::KernelRunLevel0To1(
    const OpParam &param, ExecMem &execMem, Stream &streamL0L1)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] executor starts", __func__);
    CHK_RET(GetLevelCommInfo());
    u32 perDataSize = 0;
    const HcclDataType dataType = param.GetDataType();
    CHK_RET(SalGetDataTypeSize(dataType, perDataSize));

    u32 ringNum;
    u32 level0RankSize = logicalLevel0CommInfo_.localRankSize;
    bool ARSFlag = topoMatcher_->GetARSFlag();
    bool ARSDoubleRing = (ARSFlag && (level0RankSize > FACTOR_TWO) && topoAttr_.isARSDoubleRing);
    u32 sliceNum = logicalLevel0CommInfo_.localRankSize;
    u32 commIndex = logicalLevel0CommInfo_.localRank;

    if ((topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING && !IsUnifiedMarch(param) && !ARSFlag) || ARSDoubleRing) {
        ringNum = LEVEL0_PLANE_NUM_IN_NPRING_DOUBLE;
    } else {
        ringNum = LEVEL0_PLANE_NUM_IN_NPRING_SINGLE;
    }

    bool isSelectAHC = (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_AHC ||
        algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_AHC_BROKE);

    SubCommInfo level2CommInfo;
    if (isSelectAHC) {
        level2CommInfo = logicalLevel1CommInfo_;
        level2CommInfo.localRankSize = 1;
    } else {
        CHK_RET(CheckCommSize(COMM_LEVEL2, COMM_INDEX_0 + 1));
        level2CommInfo = GetSubCommInfo(COMM_LEVEL2, COMM_INDEX_0);
    }
    const u32 level2RankSize = level2CommInfo.localRankSize;
    const u32 level1RankSize = logicalLevel1CommInfo_.localRankSize;

    CHK_RET(ActiveSlaveStreams(streamL0L1));

    std::vector<std::vector<Slice>> multiStreamSlice;
    std::vector<std::vector<Slice>> level0DataSegsSlice;
    CHK_RET(CalLevel0DataSegsSlice(execMem, multiStreamSlice, param, ringNum, sliceNum, level1RankSize, level2RankSize,
        dataType, level0DataSegsSlice));

    HcomCollOpInfo opInfo = GetHcomCollOpInfo(param, execMem);
    HcomCollOpInfo *opInfoPtr = &opInfo;

    bool disableDMAReduce = algOpContext_.opRetryHandler.retryEnable &&
        (algOpContext_.opRetryHandler.inPlaceSupportRetryStatus ==
            InplaceSupportRetryStatus::RETRY_1_ALLOW_NO_DMA_REDUCE_CASE1 ||
         algOpContext_.opRetryHandler.inPlaceSupportRetryStatus ==
            InplaceSupportRetryStatus::RETRY_1_ALLOW_NO_DMA_REDUCE_CASE2);
    std::vector<std::vector<Slice>> multRingsUserMemSlice;
    CalUserMemDataSegsSlice(execMem, level0DataSegsSlice, multiStreamSlice, param, ringNum, sliceNum, level1RankSize,
        level2RankSize, dataType, perDataSize, opInfoPtr, disableDMAReduce, multRingsUserMemSlice);

    // Level 0: intra-server reduce scatter (DMA reduce branch, outputAddr = nullptr)
    HcomCollOpInfo opInfoByReduceScatterDMAreduce = *opInfoPtr;
    opInfoByReduceScatterDMAreduce.outputAddr = nullptr;
    CHK_RET(RunIntraSeverReduceScatter(param.tag, execMem.inputMem, execMem.scratchMem, execMem.count,
        dataType, param.reduceType, level0DataSegsSlice, streamL0L1, PROF_STAGE_1, 0,
        &opInfoByReduceScatterDMAreduce, multRingsUserMemSlice, disableDMAReduce));

    // Level 1: inter-server reduce scatter
    if (level1RankSize > 1) {
        u64 reduceAttr = GetReduceAttr(execMem.inputMem, execMem.scratchMem, dataType, param.reduceType);
        std::unique_ptr<AlgTemplateBase> level1TempAlg;

        std::vector<Slice> level1DataSegsSlice;
        CHK_RET(CalLevel1DataSegsSlice(execMem, param, logicalLevel1plane_, commIndex, sliceNum, level1RankSize,
            level2RankSize, perDataSize, level1DataSegsSlice));

        if (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_RING) {
            level1TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher_);
            CHK_SMART_PTR_NULL(level1TempAlg);
            CHK_RET(level1TempAlg->Prepare(reduceAttr));
        } else if (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_NB) {
            level1TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_REDUCESCATTER_NB, dispatcher_);
            CHK_SMART_PTR_NULL(level1TempAlg);
            CHK_RET(level1TempAlg->Prepare(reduceAttr));
        } else if (isSelectAHC) {
            std::vector<std::vector<std::vector<u32>>> globalSubGroups;
            std::map<AHCConcOpType, TemplateType> ahcAlgOption;
            CHK_RET(topoMatcher_->GetGlobalSubGroups(logicalLevel1plane_, globalSubGroups));
            topoMatcher_->GetAHCAlgOption(ahcAlgOption);
            if (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_AHC) {
                level1TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                    TemplateType::TEMPLATE_REDUCESCATTER_AHC, dispatcher_);
            } else {
                level1TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                    TemplateType::TEMPLATE_REDUCESCATTER_AHC_BROKE, dispatcher_);
            }
            CHK_SMART_PTR_NULL(level1TempAlg);
            CHK_RET(level1TempAlg->Prepare(execMem.count, globalSubGroups, ahcAlgOption));
            CHK_RET(level1TempAlg->Prepare(reduceAttr));
        } else {
            level1TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_REDUCESCATTER_NHR, dispatcher_);
            CHK_SMART_PTR_NULL(level1TempAlg);
            CHK_RET(level1TempAlg->Prepare(reduceAttr, false));
        }

        CHK_RET(level1TempAlg->Prepare(execMem.inputMem, execMem.inputMem, execMem.scratchMem, execMem.count,
            dataType, streamL0L1, param.reduceType, LEVEL0_BRIDGE_RANK_ID, level1DataSegsSlice));
        CHK_RET(level1TempAlg->RegisterProfiler(
            (level1RankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + logicalLevel1CommInfo_.localRank,
            PROF_STAGE_2, HCCL_EXEC_STEP_NOT_SET, streamL0L1));
        CHK_RET(RunTemplate(level1TempAlg, logicalLevel1CommInfo_));
    }

    return HCCL_SUCCESS;
}

HcclResult CollAlignedReduceScatterDoubleRingPipelineFor91093Executor::KernelRunLevel2(
    const OpParam &param, ExecMem &execMem, Stream &streamL2)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] executor starts", __func__);
    CHK_RET(GetLevelCommInfo());
    u32 perDataSize = 0;
    const HcclDataType dataType = param.GetDataType();
    CHK_RET(SalGetDataTypeSize(dataType, perDataSize));

    bool isSelectAHC = (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_AHC ||
        algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_AHC_BROKE);

    SubCommInfo level2CommInfo;
    if (isSelectAHC) {
        level2CommInfo = logicalLevel1CommInfo_;
        level2CommInfo.localRankSize = 1;
    } else {
        CHK_RET(CheckCommSize(COMM_LEVEL2, COMM_INDEX_0 + 1));
        level2CommInfo = GetSubCommInfo(COMM_LEVEL2, COMM_INDEX_0);
    }
    const u32 level2RankSize = level2CommInfo.localRankSize;

    // Level 2: inter-superpod reduce scatter
    if (level2RankSize > 1) {
        u64 reduceAttr = GetReduceAttr(execMem.inputMem, execMem.scratchMem, dataType, param.reduceType);

        std::vector<Slice> level2DataSegsSlice;
        CHK_RET(CalLevel2DataSegsSlice(execMem, param, level2RankSize, perDataSize, level2DataSegsSlice));

        std::unique_ptr<AlgTemplateBase> level2TempAlg;
        if (algType_.algoLevel2 == AlgTypeLevel2::ALG_LEVEL2_NB) {
            level2TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_REDUCESCATTER_NB, dispatcher_);
            CHK_SMART_PTR_NULL(level2TempAlg);
            CHK_RET(level2TempAlg->Prepare(reduceAttr));
        } else if (algType_.algoLevel2 == AlgTypeLevel2::ALG_LEVEL2_NHR) {
            level2TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_REDUCESCATTER_NHR, dispatcher_);
            CHK_SMART_PTR_NULL(level2TempAlg);
            CHK_RET(level2TempAlg->Prepare(reduceAttr, false));
            if (algoAttr_.isSupportAtomicWrite) {
                level2TempAlg->CloseBarrier();
            }
        } else {
            level2TempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher_);
            CHK_SMART_PTR_NULL(level2TempAlg);
            CHK_RET(level2TempAlg->Prepare(reduceAttr));
        }

        CHK_RET(level2TempAlg->Prepare(execMem.inputMem, execMem.inputMem, execMem.scratchMem, execMem.count,
            dataType, streamL2, param.reduceType, LEVEL0_BRIDGE_RANK_ID, level2DataSegsSlice));
        CHK_RET(level2TempAlg->RegisterProfiler(
            (level2RankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + level2CommInfo.localRank,
            PROF_STAGE_2, HCCL_EXEC_STEP_NOT_SET, streamL2));
        CHK_RET(RunTemplate(level2TempAlg, level2CommInfo));
    }

    // copyOut: CclIn result -> UserOut
    HcomCollOpInfo opInfo = GetHcomCollOpInfo(param, execMem);
    HcomCollOpInfo *opInfoPtr = &opInfo;

    const u64 offset = CalcSrcMemOffset(execMem, param, perDataSize);
    DeviceMem srcMem = execMem.inputMem.range(offset, execMem.outputMem.size());
    if (opInfoPtr != nullptr) {
        DeviceMem dstMem = DeviceMem::create(static_cast<u8 *>(opInfoPtr->outputAddr), execMem.outputMem.size());
        CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, streamL2));
    } else {
        CHK_RET(HcclD2DMemcpyAsync(dispatcher_, execMem.outputMem, srcMem, streamL2));
    }

    return HCCL_SUCCESS;
}

REGISTER_EXEC("AlignedReduceScatterDoubleRingPipelineFor91093Executor",
    AlignedReduceScatterDoubleRingPipelineFor91093,
    CollAlignedReduceScatterDoubleRingPipelineFor91093Executor);
}
