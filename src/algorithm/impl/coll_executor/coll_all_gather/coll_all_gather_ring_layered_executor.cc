/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_all_gather_ring_layered_executor.h"

#include "all_gather_layered.h"

namespace hccl {
CollAllGatherRingLayeredExecutor::CollAllGatherRingLayeredExecutor(const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollAllGatherRingFor91093Executor(dispatcher, topoMatcher)
{
    desc_.level1SupportedAlgos = {
        AlgTypeLevel1::ALG_LEVEL1_HD,
        AlgTypeLevel1::ALG_LEVEL1_NB,
        AlgTypeLevel1::ALG_LEVEL1_NHR,
        AlgTypeLevel1::ALG_LEVEL1_RING
    };
    desc_.level2SupportedAlgos = {
        AlgTypeLevel2::ALG_LEVEL2_HD,
        AlgTypeLevel2::ALG_LEVEL2_NB,
        AlgTypeLevel2::ALG_LEVEL2_NHR,
        AlgTypeLevel2::ALG_LEVEL2_RING
    };
}

HcclResult CollAllGatherRingLayeredExecutor::ActiveSlaveStreamsForTest(const Stream &stream)
{
    return ActiveSlaveStreams(stream);
}

HcclResult CollAllGatherRingLayeredExecutor::MultiRingAllGatherForTest(const std::string &tag, DeviceMem inputMem,
    DeviceMem outputMem, u64 count, HcclDataType dataType, const std::vector<std::vector<Slice>> &multRingsSliceZero,
    Stream stream, s32 profStage)
{
    return MultiRingAllGather(tag, inputMem, outputMem, count, dataType, multRingsSliceZero, stream, profStage);
}

HcclResult CollAllGatherRingLayeredExecutor::CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport)
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

    const HcclAlgoType interAlgType = topoMatcher_ != nullptr ?
        topoMatcher_->GetAlgoConfig(HcclCMDType::HCCL_CMD_ALLGATHER)[HCCL_ALGO_LEVEL_2] :
        HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    CommParaInfo commParaLevel2(COMM_LAYERED_LEVEL2, LayeredBase::GetLevel2CommType(interAlgType));
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel2, opTransport[COMM_LAYERED_LEVEL2], inputType, outputType));
    return HCCL_SUCCESS;
}

HcclResult CollAllGatherRingLayeredExecutor::CalcOuterDataSegsSlice(u64 inputMemSize, u32 outerRankSize,
    u32 innerRankSize, u32 crossRankSize, std::vector<Slice> &outerDataSlices) const
{
    return PrepareAllgatherSlice(outerRankSize, inputMemSize * innerRankSize * crossRankSize, outerDataSlices);
}

HcclResult CollAllGatherRingLayeredExecutor::CalcLevel1DataSegsSlice(u64 inputMemSize, u32 innerRankSize,
    u32 crossRankSize, std::vector<Slice> &innerDataSlices) const
{
    return PrepareAllgatherSlice(innerRankSize, inputMemSize * crossRankSize, innerDataSlices);
}

HcclResult CollAllGatherRingLayeredExecutor::CalcLevel2DataSegsSlice(u64 inputMemSize, u32 crossRankSize,
    std::vector<Slice> &crossDataSlices) const
{
    return PrepareAllgatherSlice(crossRankSize, inputMemSize, crossDataSlices);
}

HcclResult CollAllGatherRingLayeredExecutor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] layered executor starts.", __func__);
    CHK_RET(ActiveSlaveStreamsForTest(param.stream));

    CHK_RET(CheckCommSize(COMM_LEVEL0, COMM_INDEX_0 + 1));
    const SubCommInfo outerCommInfo = GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);
    CHK_RET(CheckCommSize(COMM_LAYERED_LEVEL1, outerCommInfo.localRank + 1));
    const SubCommInfo innerCommInfo = GetSubCommInfo(COMM_LAYERED_LEVEL1, outerCommInfo.localRank);
    CHK_RET(CheckCommSize(COMM_LAYERED_LEVEL2, COMM_INDEX_0 + 1));
    const SubCommInfo crossCommInfo = GetSubCommInfo(COMM_LAYERED_LEVEL2, COMM_INDEX_0);

    std::vector<Slice> outerDataSlices;
    CHK_RET(CalcOuterDataSegsSlice(execMem.inputMem.size(), outerCommInfo.localRankSize, innerCommInfo.localRankSize,
        crossCommInfo.localRankSize, outerDataSlices));
    std::vector<Slice> innerDataSlices;
    CHK_RET(CalcLevel1DataSegsSlice(execMem.inputMem.size(), innerCommInfo.localRankSize, crossCommInfo.localRankSize,
        innerDataSlices));
    std::vector<Slice> crossDataSlices;
    CHK_RET(CalcLevel2DataSegsSlice(execMem.inputMem.size(), crossCommInfo.localRankSize, crossDataSlices));

    LayeredParam layeredParam;
    layeredParam.baseOffset = 0;
    layeredParam.inputMem = execMem.inputMem;
    layeredParam.outputMem = execMem.outputMem;
    layeredParam.scratchMem = execMem.scratchMem;
    layeredParam.algType = algType_;
    layeredParam.algoAttr = algoAttr_;
    layeredParam.topoAttr = topoAttr_;
    layeredParam.interAlgType = topoMatcher_ != nullptr ?
        topoMatcher_->GetAlgoConfig(HcclCMDType::HCCL_CMD_ALLGATHER)[HCCL_ALGO_LEVEL_2] :
        HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    layeredParam.count = execMem.count;
    layeredParam.outerDataSlices = outerDataSlices;
    layeredParam.innerDataSlices = innerDataSlices;
    layeredParam.crossDataSlices = crossDataSlices;
    layeredParam.outerCommInfo = outerCommInfo;
    layeredParam.innerCommInfo = innerCommInfo;
    layeredParam.crossCommInfo = crossCommInfo;

    AllGatherLayered layeredExecutor(dispatcher_);
    CHK_RET(layeredExecutor.Prepare(param, execMem, layeredParam));
    CHK_RET(layeredExecutor.CopyIn(param, execMem, layeredParam));
    CHK_RET(layeredExecutor.RunAsync(param, execMem, layeredParam));

    if (outerCommInfo.localRankSize > 1) {
        std::vector<std::vector<Slice>> multRingsSlice;
        multRingsSlice.push_back(outerDataSlices);
        CHK_RET(MultiRingAllGatherForTest(param.tag, execMem.outputMem, execMem.outputMem,
            execMem.count * innerCommInfo.localRankSize * crossCommInfo.localRankSize,
            param.DataDes.dataType, multRingsSlice, param.stream, PROF_STAGE_2));
    }

    CHK_RET(layeredExecutor.CopyOut(param, execMem, layeredParam));
    return HCCL_SUCCESS;
}

REGISTER_EXEC("AllGatherRingLayeredExecutor", AllGatherRingLayered, CollAllGatherRingLayeredExecutor);
}  // namespace hccl
