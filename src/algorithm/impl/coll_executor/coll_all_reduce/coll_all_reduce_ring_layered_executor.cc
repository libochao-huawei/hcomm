/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_all_reduce_ring_layered_executor.h"

#include "all_reduce_layered.h"

namespace hccl {
CollAllReduceRingLayeredExecutor::CollAllReduceRingLayeredExecutor(const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollAllReduceRingFor91093Executor(dispatcher, topoMatcher)
{
}

HcclResult CollAllReduceRingLayeredExecutor::ActiveSlaveStreamsForTest(const Stream &stream)
{
    return ActiveSlaveStreams(stream);
}

HcclResult CollAllReduceRingLayeredExecutor::MultiRingReduceScatterForTest(const std::string &tag,
    DeviceMem inputMem, DeviceMem outputMem, u64 count, HcclDataType dataType, HcclReduceOp reductionOp,
    const std::vector<std::vector<Slice>> &multRingsSliceZero, Stream stream, s32 profStage, u64 baseOffset,
    const HcomCollOpInfo *opInfo)
{
    return MultiRingReduceScatter(tag, inputMem, outputMem, count, dataType, reductionOp, multRingsSliceZero,
        stream, profStage, baseOffset, opInfo);
}

HcclResult CollAllReduceRingLayeredExecutor::MultiRingAllGatherForTest(const std::string &tag,
    DeviceMem inputMem, DeviceMem outputMem, u64 count, HcclDataType dataType,
    const std::vector<std::vector<Slice>> &multRingsSliceZero, Stream stream, s32 profStage, u64 baseOffset,
    const HcomCollOpInfo *opInfo)
{
    return MultiRingAllGather(tag, inputMem, outputMem, count, dataType, multRingsSliceZero,
        stream, profStage, baseOffset, opInfo);
}

HcclResult CollAllReduceRingLayeredExecutor::CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;
    if (workflowMode_ == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE) {
        inputType = TransportMemType::CCL_INPUT;
        outputType = TransportMemType::CCL_OUTPUT;
    } else {
        inputType = TransportMemType::PARAM_INPUT;
        outputType = TransportMemType::PARAM_OUTPUT;
    }

    CommParaInfo commParaLevel0(COMM_LEVEL0, CommType::COMM_TAG_RING_INNER);
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel0, opTransport[COMM_LEVEL0], inputType, outputType));

    CommParaInfo commParaLevel1(COMM_LAYERED_LEVEL1, LayeredBase::GetLevel1CommType(algoAttr_, algType_));
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel1, opTransport[COMM_LAYERED_LEVEL1], inputType, outputType));

    const HcclAlgoType interAlgType = topoMatcher_ != nullptr
        ? topoMatcher_->GetAlgoConfig(HcclCMDType::HCCL_CMD_ALLREDUCE)[HCCL_ALGO_LEVEL_2]
        : HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    CommParaInfo commParaLevel2(COMM_LAYERED_LEVEL2, LayeredBase::GetLevel2CommType(interAlgType));
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel2, opTransport[COMM_LAYERED_LEVEL2], inputType, outputType));
    return HCCL_SUCCESS;
}

HcclResult CollAllReduceRingLayeredExecutor::CalcLevel1DataSegsSlice(const SubCommInfo &outerCommInfo,
    const SubCommInfo &innerCommInfo, const std::vector<Slice> &outerDataSlices, HcclDataType dataType,
    std::vector<Slice> &innerDataSlices) const
{
    CHK_PRT_RET(outerDataSlices.empty(),
        HCCL_ERROR("[CollAllReduceRingLayeredExecutor][CalcLevel1DataSegsSlice] outerDataSlices is empty."),
        HCCL_E_INTERNAL);
    CHK_PRT_RET(outerCommInfo.localRank >= outerDataSlices.size(),
        HCCL_ERROR("[CollAllReduceRingLayeredExecutor][CalcLevel1DataSegsSlice] outerRank[%u] >= sliceSize[%zu].",
            outerCommInfo.localRank, outerDataSlices.size()), HCCL_E_INTERNAL);
    const Slice &outerSlice = outerDataSlices[outerCommInfo.localRank];
    const u64 innerCount = outerSlice.size / SIZE_TABLE[dataType];
    return ExecutorBase::PrepareSliceData(innerCount, SIZE_TABLE[dataType], innerCommInfo.localRankSize, 0,
        innerDataSlices);
}

HcclResult CollAllReduceRingLayeredExecutor::CalcLevel2DataSegsSlice(const SubCommInfo &innerCommInfo,
    const SubCommInfo &crossCommInfo, const std::vector<Slice> &innerDataSlices, HcclDataType dataType,
    std::vector<Slice> &crossDataSlices) const
{
    CHK_PRT_RET(innerDataSlices.empty(),
        HCCL_ERROR("[CollAllReduceRingLayeredExecutor][CalcLevel2DataSegsSlice] innerDataSlices is empty."),
        HCCL_E_INTERNAL);
    CHK_PRT_RET(innerCommInfo.localRank >= innerDataSlices.size(),
        HCCL_ERROR("[CollAllReduceRingLayeredExecutor][CalcLevel2DataSegsSlice] innerRank[%u] >= sliceSize[%zu].",
            innerCommInfo.localRank, innerDataSlices.size()), HCCL_E_INTERNAL);
    const Slice &innerSlice = innerDataSlices[innerCommInfo.localRank];
    const u64 crossCount = innerSlice.size / SIZE_TABLE[dataType];
    return ExecutorBase::PrepareSliceData(crossCount, SIZE_TABLE[dataType], crossCommInfo.localRankSize, 0,
        crossDataSlices);
}

HcclResult CollAllReduceRingLayeredExecutor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] layered executor starts.", __func__);
    CHK_RET(ActiveSlaveStreamsForTest(param.stream));

    u32 perDataSize = 0;
    CHK_RET(SalGetDataTypeSize(param.DataDes.dataType, perDataSize));

    CHK_RET(CheckCommSize(COMM_LEVEL0, COMM_INDEX_0 + 1));
    const SubCommInfo outerCommInfo = GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);
    const u32 sliceNum = outerCommInfo.localRankSize;
    std::vector<Slice> outerDataSlices;
    CHK_RET(AlgTemplateBase::PrepareSliceData(execMem.count, perDataSize, sliceNum, 0, outerDataSlices));

    std::vector<std::vector<Slice>> multRingsSliceZero;
    if (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING) {
        multRingsSliceZero = PrepareMultiRingSlice(outerDataSlices, param.tag, false, topoAttr_.nicList);
    } else {
        multRingsSliceZero.push_back(outerDataSlices);
    }

    HcomCollOpInfo reduceScatterOpInfo = {
        "", execMem.inputPtr, nullptr, execMem.count, param.DataDes.dataType, param.root, param.reduceType
    };
    HcomCollOpInfo graphModeReduceScatterOpInfo = {
        "", execMem.inputMem.ptr(), nullptr, execMem.count, param.DataDes.dataType, param.root, param.reduceType
    };
    HcomCollOpInfo *reduceScatterOpInfoPtr = nullptr;
    if (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING) {
        reduceScatterOpInfoPtr = &graphModeReduceScatterOpInfo;
    }
    if (DMAReduceFlag_) {
        reduceScatterOpInfoPtr = &reduceScatterOpInfo;
    }

    CHK_RET(MultiRingReduceScatterForTest(param.tag, execMem.inputMem, execMem.outputMem, execMem.count,
        param.DataDes.dataType, param.reduceType, multRingsSliceZero, param.stream, PROF_STAGE_0, 0,
        reduceScatterOpInfoPtr));

    CHK_RET(CheckCommSize(COMM_LAYERED_LEVEL1, outerCommInfo.localRank + 1));
    const SubCommInfo innerCommInfo = GetSubCommInfo(COMM_LAYERED_LEVEL1, outerCommInfo.localRank);
    CHK_RET(CheckCommSize(COMM_LAYERED_LEVEL2, COMM_INDEX_0 + 1));
    const SubCommInfo crossCommInfo = GetSubCommInfo(COMM_LAYERED_LEVEL2, COMM_INDEX_0);

    std::vector<Slice> innerDataSlices;
    CHK_RET(CalcLevel1DataSegsSlice(outerCommInfo, innerCommInfo, outerDataSlices, param.DataDes.dataType, innerDataSlices));
    std::vector<Slice> crossDataSlices;
    CHK_RET(CalcLevel2DataSegsSlice(innerCommInfo, crossCommInfo, innerDataSlices, param.DataDes.dataType, crossDataSlices));

    LayeredParam layeredParam;
    layeredParam.baseOffset = 0;
    layeredParam.inputMem = execMem.outputMem;
    layeredParam.outputMem = execMem.outputMem;
    layeredParam.scratchMem = execMem.scratchMem;
    layeredParam.algType = algType_;
    layeredParam.algoAttr = algoAttr_;
    layeredParam.topoAttr = topoAttr_;
    layeredParam.interAlgType = topoMatcher_ != nullptr ?
        topoMatcher_->GetAlgoConfig(HcclCMDType::HCCL_CMD_ALLREDUCE)[HCCL_ALGO_LEVEL_2] : HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    layeredParam.count = execMem.count;
    layeredParam.outerDataSlices = outerDataSlices;
    layeredParam.innerDataSlices = innerDataSlices;
    layeredParam.crossDataSlices = crossDataSlices;
    layeredParam.outerCommInfo = outerCommInfo;
    layeredParam.innerCommInfo = innerCommInfo;
    layeredParam.crossCommInfo = crossCommInfo;

    const u64 reduceAttr = GetReduceAttr(execMem.outputMem, execMem.outputMem, param.DataDes.dataType, param.reduceType);
    AllReduceLayered layeredExecutor(dispatcher_, reduceAttr);
    CHK_RET(layeredExecutor.RunAsync(param, execMem, layeredParam));

    HcomCollOpInfo allGatherOpInfo = {
        "", nullptr, execMem.outputPtr, execMem.count, param.DataDes.dataType, param.root, param.reduceType
    };
    HcomCollOpInfo graphModeAllGatherOpInfo = {
        "", nullptr, execMem.outputMem.ptr(), execMem.count, param.DataDes.dataType, param.root, param.reduceType
    };
    HcomCollOpInfo *allGatherOpInfoPtr = nullptr;
    if (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING) {
        allGatherOpInfoPtr = &graphModeAllGatherOpInfo;
    }
    if (DMAReduceFlag_) {
        allGatherOpInfoPtr = &allGatherOpInfo;
    }

    CHK_RET(MultiRingAllGatherForTest(param.tag, execMem.outputMem, execMem.outputMem, execMem.count,
        param.DataDes.dataType, multRingsSliceZero, param.stream, PROF_STAGE_2, 0, allGatherOpInfoPtr));
    return HCCL_SUCCESS;
}

REGISTER_EXEC("AllReduceRingLayeredExecutor", AllReduceRingLayered, CollAllReduceRingLayeredExecutor);
}  // namespace hccl
