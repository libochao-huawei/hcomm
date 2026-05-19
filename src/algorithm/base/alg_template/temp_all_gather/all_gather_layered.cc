/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "all_gather_layered.h"

#include <numeric>

#include "op_context.h"
#include "sal_pub.h"
#include "topoinfo_plane_transformer.h"

namespace hccl {
AllGatherLayered::AllGatherLayered(const HcclDispatcher dispatcher)
    : LayeredBase(dispatcher)
{
}

AllGatherLayered::~AllGatherLayered() = default;

HcclResult AllGatherLayered::CopyBypassForTest(DeviceMem &dstMem, const DeviceMem &srcMem, Stream &stream)
{
    return HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, stream);
}

HcclResult AllGatherLayered::Prepare(const OpParam &opParam, hccl::ExecMem &execMem,
    const LayeredParam &layeredParam)
{
    CHK_RET(LayeredBase::Prepare(opParam, execMem, layeredParam));
    CHK_RET(PrepareSlicesAndBuffers(execMem));
    return HCCL_SUCCESS;
}

HcclResult AllGatherLayered::PrepareSlicesAndBuffers(const ExecMem &execMem)
{
    CHK_PRT_RET(dataUnit_ == 0, HCCL_ERROR("[AllGatherLayered][PrepareSlicesAndBuffers] dataUnit is zero."),
        HCCL_E_INTERNAL);
    CHK_PRT_RET(outerCommSize_ == 0 || innerCommSize_ == 0 || crossCommSize_ == 0,
        HCCL_ERROR("[AllGatherLayered][PrepareSlicesAndBuffers] invalid comm sizes outer[%u] inner[%u] cross[%u].",
            outerCommSize_, innerCommSize_, crossCommSize_), HCCL_E_INTERNAL);

    unitBytes_ = execMem.inputMem.size();
    CHK_PRT_RET(unitBytes_ == 0,
        HCCL_ERROR("[AllGatherLayered][PrepareSlicesAndBuffers] inputMem size is zero."), HCCL_E_INTERNAL);

    crossBytes_ = crossCommSize_ * unitBytes_;
    crossCount_ = crossBytes_ / dataUnit_;
    crossOffset_ = outerCommRank_ * innerCommSize_ * crossBytes_ + innerCommRank_ * crossBytes_;
    localCopyOffset_ = crossOffset_ + crossCommRank_ * unitBytes_;
    CHK_RET(ExecutorBase::PrepareSliceData(crossCount_, dataUnit_, crossCommSize_, 0, crossDataSlices_));
    CHK_PRT_RET(crossCommRank_ >= crossDataSlices_.size(),
        HCCL_ERROR("[AllGatherLayered][PrepareSlicesAndBuffers] crossRank[%u] >= sliceSize[%zu].",
            crossCommRank_, crossDataSlices_.size()), HCCL_E_INTERNAL);

    innerBytes_ = innerCommSize_ * crossBytes_;
    innerCount_ = innerBytes_ / dataUnit_;
    innerOffset_ = outerCommRank_ * innerBytes_;
    CHK_RET(ExecutorBase::PrepareSliceData(innerCount_, dataUnit_, innerCommSize_, 0, innerDataSlices_));
    CHK_PRT_RET(innerCommRank_ >= innerDataSlices_.size(),
        HCCL_ERROR("[AllGatherLayered][PrepareSlicesAndBuffers] innerRank[%u] >= sliceSize[%zu].",
            innerCommRank_, innerDataSlices_.size()), HCCL_E_INTERNAL);

    crossCclIn_ = outputMem_.range(localCopyOffset_, unitBytes_);
    CHK_SMART_PTR_NULL(crossCclIn_);
    crossCclOut_ = outputMem_.range(crossOffset_, crossBytes_);
    CHK_SMART_PTR_NULL(crossCclOut_);
    innerCclIn_ = crossCclOut_;
    CHK_SMART_PTR_NULL(innerCclIn_);
    innerCclOut_ = outputMem_.range(innerOffset_, innerBytes_);
    CHK_SMART_PTR_NULL(innerCclOut_);

    HCCL_INFO("[AllGatherLayered][PrepareSlicesAndBuffers] unitBytes[%llu] innerOffset[%llu] innerBytes[%llu] "
        "crossOffset[%llu] crossBytes[%llu] localCopyOffset[%llu].", unitBytes_, innerOffset_, innerBytes_,
        crossOffset_, crossBytes_, localCopyOffset_);
    return HCCL_SUCCESS;
}

HcclResult AllGatherLayered::CopyIn(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam)
{
    (void)opParam;
    (void)layeredParam;
    DeviceMem srcMem = DeviceMem::create(execMem.inputPtr, execMem.inputMem.size());
    CHK_SMART_PTR_NULL(srcMem);
    DeviceMem dstMem = outputMem_.range(localCopyOffset_, unitBytes_);
    CHK_SMART_PTR_NULL(dstMem);
    CHK_RET(CopyBypassForTest(dstMem, srcMem, stream_));
    return HCCL_SUCCESS;
}

HcclResult AllGatherLayered::RunAsync(const OpParam &opParam, hccl::ExecMem &execMem,
    const LayeredParam &layeredParam)
{
    (void)opParam;
    (void)execMem;
    (void)layeredParam;
    CHK_RET(RunCrossAllGather());
    CHK_RET(RunInnerAllGather());
    HCCL_INFO("[AllGatherLayered][RunAsync] success.");
    return HCCL_SUCCESS;
}

std::unique_ptr<AlgTemplateBase> AllGatherLayered::CreateLevel1Template() const
{
    switch (algType_.algoLevel1) {
        case AlgTypeLevel1::ALG_LEVEL1_HD:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_ALL_GATHER_RECURSIVE_HALVING_DOUBLING, dispatcher_);
        case AlgTypeLevel1::ALG_LEVEL1_NB:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NB, dispatcher_);
        case AlgTypeLevel1::ALG_LEVEL1_NHR:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHR, dispatcher_);
        default:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RING, dispatcher_);
    }
}

std::unique_ptr<AlgTemplateBase> AllGatherLayered::CreateLevel2Template() const
{
    switch (interAlgType_) {
        case HcclAlgoType::HCCL_ALGO_TYPE_HDR:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(
                TemplateType::TEMPLATE_ALL_GATHER_RECURSIVE_HALVING_DOUBLING, dispatcher_);
        case HcclAlgoType::HCCL_ALGO_TYPE_NB:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NB, dispatcher_);
        case HcclAlgoType::HCCL_ALGO_TYPE_NHR:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHR, dispatcher_);
        case HcclAlgoType::HCCL_ALGO_TYPE_NHR_V1:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHRV1, dispatcher_);
        default:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RING, dispatcher_);
    }
}

HcclResult AllGatherLayered::PrepareTemplate(AlgTemplateBase &tempAlg) const
{
    if (dynamic_cast<AllGatherNHR *>(&tempAlg) != nullptr) {
        return static_cast<AllGatherNHR &>(tempAlg).Prepare(true);
    }
    return HCCL_SUCCESS;
}

HcclResult AllGatherLayered::RunTemplateOnComm(std::unique_ptr<AlgTemplateBase> tempAlg, const SubCommInfo &commInfo,
    DeviceMem &inputMem, DeviceMem &outputMem, DeviceMem &scratchMem, u64 count, u64 offset,
    const std::vector<Slice> &slices, s32 stage)
{
    CHK_SMART_PTR_NULL(tempAlg);
    CHK_RET(PrepareTemplate(*tempAlg));
    CHK_RET(tempAlg->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, HCCL_REDUCE_RESERVED,
        LEVEL0_BRIDGE_RANK_ID, slices, offset));
    CHK_RET(tempAlg->RegisterProfiler((commInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + commInfo.localRank,
        stage, HCCL_EXEC_STEP_NOT_SET, stream_));
    return CollExecutorBase::RunTemplate(tempAlg, commInfo);
}

HcclResult AllGatherLayered::RunCrossAllGather()
{
    if (crossCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }
    const u64 perCrossInputCount = unitBytes_ / dataUnit_;
    return RunTemplateOnComm(CreateLevel2Template(), crossCommInfo_, crossCclOut_, crossCclOut_, crossCclIn_,
        perCrossInputCount, crossOffset_, crossDataSlices_, PROF_STAGE_0);
}

HcclResult AllGatherLayered::RunInnerAllGather()
{
    if (innerCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }
    const u64 perInnerInputCount = crossBytes_ / dataUnit_;
    return RunTemplateOnComm(CreateLevel1Template(), innerCommInfo_, innerCclOut_, innerCclOut_, innerCclIn_,
        perInnerInputCount, innerOffset_, innerDataSlices_, PROF_STAGE_1);
}

HcclResult AllGatherLayered::CopyOut(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam)
{
    (void)execMem;
    (void)layeredParam;
    const u32 unitSize = SIZE_TABLE[opParam.DataDes.dataType];
    CHK_PRT_RET(unitSize == 0,
        HCCL_ERROR("[AllGatherLayered][CopyOut] unitSize is zero for dataType[%d].", opParam.DataDes.dataType),
        HCCL_E_PARA);

    std::vector<u32> indexVector(crossCommSize_);
    std::iota(indexVector.begin(), indexVector.end(), 0);
    std::vector<std::vector<u32>> indexGroups(crossCommSize_);
    for (u32 i = 0; i < crossCommSize_; ++i) {
        indexGroups[i] = std::vector<u32>(1, i);
    }
    CHK_RET(TopoinfoPlaneTransformer::TransformPlaneByAlgo(
        interAlgType_, topoAttr_.netPlaneId, 0, indexGroups, indexVector));

    for (u32 crossIdx = 0; crossIdx < crossCommSize_; ++crossIdx) {
        for (u32 innerIdx = 0; innerIdx < innerCommSize_; ++innerIdx) {
            for (u32 outerIdx = 0; outerIdx < outerCommSize_; ++outerIdx) {
                const u64 srcIdx = outerIdx * innerCommSize_ * crossCommSize_ +
                    innerIdx * crossCommSize_ + indexVector[crossIdx];
                const u64 dstIdx = crossIdx * innerCommSize_ * outerCommSize_ + innerIdx * outerCommSize_ + outerIdx;
                const u64 srcOffset = unitBytes_ * srcIdx;
                const u64 dstOffset = opParam.DataDes.count * unitSize * dstIdx;

                DeviceMem srcMem = outputMem_.range(srcOffset, unitBytes_);
                CHK_SMART_PTR_NULL(srcMem);
                DeviceMem dstMem = DeviceMem::create(static_cast<u8 *>(execMem.outputPtr) + dstOffset, unitBytes_);
                CHK_SMART_PTR_NULL(dstMem);
                CHK_RET(CopyBypassForTest(dstMem, srcMem, stream_));
            }
        }
    }
    return HCCL_SUCCESS;
}
}  // namespace hccl
