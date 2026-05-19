/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_reduce_scatter_ring_layered_executor.h"

#include "reduce_scatter_layered.h"

namespace hccl {
CollReduceScatterRingLayeredExecutor::CollReduceScatterRingLayeredExecutor(const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollReduceScatterRingFor91093Executor(dispatcher, topoMatcher)
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

HcclResult CollReduceScatterRingLayeredExecutor::ActiveSlaveStreamsForTest(const Stream &stream)
{
    return ActiveSlaveStreams(stream);
}

HcclResult CollReduceScatterRingLayeredExecutor::MultiRingReduceScatterForTest(const std::string &tag,
    DeviceMem inputMem, DeviceMem outputMem, u64 count, HcclDataType dataType, HcclReduceOp reductionOp,
    const std::vector<std::vector<Slice>> &multRingsSliceZero, Stream stream, s32 profStage, u64 baseOffset,
    const HcomCollOpInfo *opInfo, const std::vector<std::vector<Slice>> &multRingsUserMemSlice)
{
    return MultiRingReduceScatter(tag, inputMem, outputMem, count, dataType, reductionOp, multRingsSliceZero,
        stream, profStage, baseOffset, opInfo, multRingsUserMemSlice);
}

HcclResult CollReduceScatterRingLayeredExecutor::CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;
    CHK_RET(CalcTransportMemType(inputType, outputType));
    CHK_RET(CalcLevel0CommInfo(inputType, outputType, opTransport));

    CommParaInfo commParaLevel1(COMM_LAYERED_LEVEL1,
        LayeredBase::GetLevel1CommType(algoAttr_, algType_));
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel1, opTransport[COMM_LAYERED_LEVEL1], inputType, outputType));

    const HcclAlgoType interAlgType = topoMatcher_ != nullptr ?
        topoMatcher_->GetAlgoConfig(HcclCMDType::HCCL_CMD_REDUCE_SCATTER)[HCCL_ALGO_LEVEL_2] :
        HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    CommParaInfo commParaLevel2(COMM_LAYERED_LEVEL2, LayeredBase::GetLevel2CommType(interAlgType));
    CHK_RET(CalcCommPlaneInfo(tag_, commParaLevel2, opTransport[COMM_LAYERED_LEVEL2], inputType, outputType));
    return HCCL_SUCCESS;
}

void CollReduceScatterRingLayeredExecutor::FillMultiRingSlice(const ExecMem &execMem,
    const std::vector<std::vector<Slice>> &multiStreamSlice, u32 sliceNum, u32 innerRankSize, u32 crossRankSize,
    const u32 ringIndex, std::vector<Slice> &dataSlice)
{
    for (u32 level0Idx = 0; level0Idx < sliceNum; level0Idx++) {
        for (u32 crossIdx = 0; crossIdx < crossRankSize; crossIdx++) {
            for (u32 innerIdx = 0; innerIdx < innerRankSize; innerIdx++) {
                Slice sliceTemp;
                sliceTemp.size = multiStreamSlice[ringIndex][level0Idx].size;
                sliceTemp.offset = multiStreamSlice[ringIndex][level0Idx].offset +
                    innerIdx * sliceNum * execMem.outputMem.size() +
                    crossIdx * sliceNum * innerRankSize * execMem.outputMem.size();
                dataSlice.push_back(sliceTemp);
            }
        }
    }
}

HcclResult CollReduceScatterRingLayeredExecutor::CalLevel0DataSegsSlice(const ExecMem &execMem, const OpParam &param,
    u32 ringNum, u32 sliceNum, u32 innerRankSize, u32 crossRankSize, HcclDataType dataType,
    std::vector<std::vector<Slice>> &multiStreamSlice, std::vector<std::vector<Slice>> &level0DataSegsSlice)
{
    bool isInlineReduce = IsSupportSDMAReduce(execMem.inputMem.ptr(), execMem.scratchMem.ptr(), dataType,
        param.reduceType);
    bool useInlineReduce = isInlineReduce && algoAttr_.inlineReduceSwitchOn;
    std::vector<Slice> dataSegsSlice;
    multiStreamSlice = ReduceScatterRingSlicePrepare(ringNum, sliceNum, useInlineReduce, execMem.outputMem,
        dataSegsSlice, param.tag);

    for (u32 ringIndex = 0; ringIndex < multiStreamSlice.size(); ringIndex++) {
        std::vector<Slice> dataSlice;
        FillMultiRingSlice(execMem, multiStreamSlice, sliceNum, innerRankSize, crossRankSize, ringIndex, dataSlice);
        level0DataSegsSlice.push_back(dataSlice);
    }
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterRingLayeredExecutor::CalcLevel1DataSegsSlice(const ExecMem &execMem, u32 commIndex,
    u32 sliceNum, u32 innerRankSize, u32 crossRankSize, std::vector<Slice> &innerDataSegsSlice)
{
    for (u32 innerRank = 0; innerRank < innerRankSize; innerRank++) {
        u32 innerUserRank = 0;
        CHK_RET(GetUserRankByRank(COMM_LAYERED_LEVEL1, commIndex, innerRank, innerUserRank));
        if (crossRankSize <= 1) {
            innerDataSegsSlice.push_back(Slice{innerUserRank * execMem.outputMem.size(), execMem.outputMem.size()});
            continue;
        }
        for (u32 crossIdx = 0; crossIdx < crossRankSize; crossIdx++) {
            Slice sliceTemp;
            sliceTemp.size = execMem.outputMem.size();
            sliceTemp.offset = (innerUserRank % (innerRankSize * sliceNum)) * execMem.outputMem.size() +
                crossIdx * sliceNum * innerRankSize * execMem.outputMem.size();
            innerDataSegsSlice.push_back(sliceTemp);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterRingLayeredExecutor::CalcLevel2DataSegsSlice(const ExecMem &execMem, u32 crossRankSize,
    std::vector<Slice> &crossDataSegsSlice)
{
    for (u32 crossRank = 0; crossRank < crossRankSize; crossRank++) {
        u32 crossUserRank = 0;
        CHK_RET(GetUserRankByRank(COMM_LAYERED_LEVEL2, COMM_INDEX_0, crossRank, crossUserRank));
        crossDataSegsSlice.push_back(Slice{crossUserRank * execMem.outputMem.size(), execMem.outputMem.size()});
    }
    return HCCL_SUCCESS;
}

HcclResult CollReduceScatterRingLayeredExecutor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] layered executor starts.", __func__);
    u32 perDataSize = 0;
    const HcclDataType dataType = param.GetDataType();
    CHK_RET(SalGetDataTypeSize(dataType, perDataSize));

    CHK_RET(CheckCommSize(COMM_LEVEL0, COMM_INDEX_0 + 1));
    const SubCommInfo level0CommInfo = GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);

    const u32 ringNum = (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING) ?
        LEVEL0_PLANE_NUM_IN_NPRING_DOUBLE : LEVEL0_PLANE_NUM_IN_NPRING_SINGLE;
    const u32 sliceNum = level0CommInfo.localRankSize;
    const u32 commIndex = level0CommInfo.localRank;

    CHK_RET(CheckCommSize(COMM_LAYERED_LEVEL1, commIndex + 1));
    SubCommInfo innerCommInfo = GetSubCommInfo(COMM_LAYERED_LEVEL1, commIndex);
    CHK_RET(CheckCommSize(COMM_LAYERED_LEVEL2, COMM_INDEX_0 + 1));
    SubCommInfo crossCommInfo = GetSubCommInfo(COMM_LAYERED_LEVEL2, COMM_INDEX_0);

    const u32 innerRankSize = innerCommInfo.localRankSize;
    const u32 crossRankSize = crossCommInfo.localRankSize;

    CHK_RET(ActiveSlaveStreamsForTest(param.stream));

    std::vector<std::vector<Slice>> multiStreamSlice;
    std::vector<std::vector<Slice>> level0DataSegsSlice;
    CHK_RET(CalLevel0DataSegsSlice(execMem, param, ringNum, sliceNum, innerRankSize, crossRankSize, dataType,
        multiStreamSlice, level0DataSegsSlice));

    HcomCollOpInfo opInfo = {"", execMem.inputPtr, execMem.outputPtr, param.GetDataCount(topoAttr_.userRank), dataType,
        param.root, param.reduceType, param.GetStrideCount()};
    HcomCollOpInfo *opInfoPtr = DMAReduceFlag_ ? &opInfo : nullptr;

    LayeredParam layeredParam;
    layeredParam.baseOffset = 0;
    layeredParam.inputMem = execMem.inputMem;
    layeredParam.outputMem = execMem.outputMem;
    layeredParam.scratchMem = execMem.scratchMem;
    layeredParam.algType = algType_;
    layeredParam.algoAttr = algoAttr_;
    layeredParam.topoAttr = topoAttr_;
    layeredParam.interAlgType = topoMatcher_->GetAlgoConfig(HcclCMDType::HCCL_CMD_REDUCE_SCATTER)[HCCL_ALGO_LEVEL_2];
    layeredParam.count = execMem.count;
    layeredParam.outerDataSlices = level0DataSegsSlice.empty() ? std::vector<Slice>() : level0DataSegsSlice[0];
    layeredParam.outerCommInfo = level0CommInfo;
    layeredParam.innerCommInfo = innerCommInfo;
    layeredParam.crossCommInfo = crossCommInfo;
    CHK_RET(CalcLevel1DataSegsSlice(execMem, commIndex, sliceNum, innerRankSize, crossRankSize,
        layeredParam.innerDataSlices));
    CHK_RET(CalcLevel2DataSegsSlice(execMem, crossRankSize, layeredParam.crossDataSlices));

    const u64 reduceAttr = GetReduceAttr(execMem.inputMem, execMem.scratchMem, dataType, param.reduceType);
    ReduceScatterLayered layeredExecutor(dispatcher_, reduceAttr);
    HcomCollOpInfo stage0OpInfo = opInfo;
    HcomCollOpInfo *stage0OpInfoPtr = opInfoPtr;
    if (stage0OpInfoPtr != nullptr && (innerRankSize > 1 || crossRankSize > 1)) {
        stage0OpInfo.outputAddr = nullptr;
        stage0OpInfoPtr = &stage0OpInfo;
    }

    layeredExecutor.SetOuterReduceScatterContext(stage0OpInfoPtr, multiStreamSlice, level0DataSegsSlice, perDataSize);
    CHK_RET(layeredExecutor.Prepare(param, execMem, layeredParam));
    CHK_RET(layeredExecutor.CopyIn(param, execMem, layeredParam));

    std::vector<std::vector<Slice>> multRingsUserMemSlice = layeredExecutor.GetOuterUserMemSlices();
    CHK_RET(MultiRingReduceScatterForTest(param.tag, execMem.inputMem, execMem.scratchMem, execMem.count, dataType,
        param.reduceType, level0DataSegsSlice, param.stream, PROF_STAGE_1, 0, stage0OpInfoPtr,
        multRingsUserMemSlice));

    CHK_RET(layeredExecutor.RunAsync(param, execMem, layeredParam));
    return layeredExecutor.CopyOut(param, execMem, layeredParam);
}

REGISTER_EXEC("ReduceScatterRingLayeredExecutor", ReduceScatterRingLayered, CollReduceScatterRingLayeredExecutor);
}  // namespace hccl
