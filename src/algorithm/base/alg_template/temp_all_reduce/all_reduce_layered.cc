/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "all_reduce_layered.h"

#include "op_context.h"
#include "sal_pub.h"

namespace hccl {
AllReduceLayered::AllReduceLayered(const HcclDispatcher dispatcher, u64 reduceAttr)
    : LayeredBase(dispatcher), reduceAttr_(reduceAttr)
{
}

AllReduceLayered::~AllReduceLayered() = default;

HcclResult AllReduceLayered::RunAsync(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam)
{
    CHK_RET(Prepare(opParam, execMem, layeredParam));
    CHK_RET(RunInnerReduceScatter());
    CHK_RET(RunCrossAllReduce());
    CHK_RET(RunInnerAllGather());
    HCCL_INFO("[AllReduceLayered][RunAsync] success.");
    return HCCL_SUCCESS;
}

HcclResult AllReduceLayered::Prepare(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam)
{
    CHK_RET(LayeredBase::Prepare(opParam, execMem, layeredParam));
    CHK_RET(PrepareSlicesAndBuffers(execMem));
    return HCCL_SUCCESS;
}

HcclResult AllReduceLayered::PrepareSlicesAndBuffers(const ExecMem &execMem)
{
    CHK_PRT_RET(dataUnit_ == 0, HCCL_ERROR("[AllReduceLayered][PrepareSlicesAndBuffers] dataUnit is zero."), HCCL_E_INTERNAL);
    CHK_PRT_RET(outerDataSlices_.empty(), HCCL_ERROR("[AllReduceLayered][PrepareSlicesAndBuffers] outerDataSlices is empty."), HCCL_E_INTERNAL);
    CHK_PRT_RET(outerCommRank_ >= outerDataSlices_.size(),
        HCCL_ERROR("[AllReduceLayered][PrepareSlicesAndBuffers] outerRank[%u] >= sliceSize[%zu].", outerCommRank_, outerDataSlices_.size()),
        HCCL_E_INTERNAL);

    innerSlice_ = outerDataSlices_[outerCommRank_];
    innerBytes_ = innerSlice_.size;
    innerOffset_ = innerSlice_.offset;
    innerCount_ = innerBytes_ / dataUnit_;
    CHK_RET(ExecutorBase::PrepareSliceData(innerCount_, dataUnit_, innerCommSize_, 0, innerDataSlices_));
    CHK_PRT_RET(innerCommRank_ >= innerDataSlices_.size(),
        HCCL_ERROR("[AllReduceLayered][PrepareSlicesAndBuffers] innerRank[%u] >= sliceSize[%zu].", innerCommRank_, innerDataSlices_.size()),
        HCCL_E_INTERNAL);

    crossSlice_ = innerDataSlices_[innerCommRank_];
    crossBytes_ = crossSlice_.size;
    crossOffset_ = crossSlice_.offset + innerOffset_;
    crossCount_ = crossBytes_ / dataUnit_;
    CHK_RET(ExecutorBase::PrepareSliceData(crossCount_, dataUnit_, crossCommSize_, 0, crossDataSlices_));
    CHK_PRT_RET(crossCommRank_ >= crossDataSlices_.size(),
        HCCL_ERROR("[AllReduceLayered][PrepareSlicesAndBuffers] crossRank[%u] >= sliceSize[%zu].", crossCommRank_, crossDataSlices_.size()),
        HCCL_E_INTERNAL);

    innerCclIn_ = inputMem_.range(innerOffset_, innerBytes_);
    CHK_SMART_PTR_NULL(innerCclIn_);
    innerCclOut_ = outputMem_.range(innerOffset_, innerBytes_);
    CHK_SMART_PTR_NULL(innerCclOut_);
    crossCclIn_ = inputMem_.range(crossOffset_, crossBytes_);
    CHK_SMART_PTR_NULL(crossCclIn_);
    crossCclOut_ = outputMem_.range(crossOffset_, crossBytes_);
    CHK_SMART_PTR_NULL(crossCclOut_);
    return HCCL_SUCCESS;
}

std::unique_ptr<AlgTemplateBase> AllReduceLayered::CreateLevel1ReduceScatterTemplate() const
{
    switch (algType_.algoLevel1) {
        case AlgTypeLevel1::ALG_LEVEL1_HD:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_REDUCESCATTER_HD, dispatcher_);
        case AlgTypeLevel1::ALG_LEVEL1_NB:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_REDUCESCATTER_NB, dispatcher_);
        case AlgTypeLevel1::ALG_LEVEL1_NHR:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_REDUCESCATTER_NHR, dispatcher_);
        default:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher_);
    }
}

std::unique_ptr<AlgTemplateBase> AllReduceLayered::CreateLevel1AllGatherTemplate() const
{
    switch (algType_.algoLevel1) {
        case AlgTypeLevel1::ALG_LEVEL1_HD:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RECURSIVE_HALVING_DOUBLING, dispatcher_);
        case AlgTypeLevel1::ALG_LEVEL1_NB:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NB, dispatcher_);
        case AlgTypeLevel1::ALG_LEVEL1_NHR:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHR, dispatcher_);
        default:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RING, dispatcher_);
    }
}

std::unique_ptr<AlgTemplateBase> AllReduceLayered::CreateLevel2AllReduceTemplate() const
{
    switch (interAlgType_) {
        case HcclAlgoType::HCCL_ALGO_TYPE_HDR:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_REDUCE_RECURSIVE_HALVING_DOUBLING, dispatcher_);
        case HcclAlgoType::HCCL_ALGO_TYPE_NB:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_REDUCE_NB, dispatcher_);
        case HcclAlgoType::HCCL_ALGO_TYPE_NHR:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_REDUCE_NHR, dispatcher_);
        case HcclAlgoType::HCCL_ALGO_TYPE_NHR_V1:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_REDUCE_NHR_V1, dispatcher_);
        default:
            return AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_REDUCE_RING, dispatcher_);
    }
}

HcclResult AllReduceLayered::PrepareAllGatherTemplate(AlgTemplateBase &tempAlg) const
{
    if (dynamic_cast<AllGatherNHR*>(&tempAlg) != nullptr) {
        return static_cast<AllGatherNHR&>(tempAlg).Prepare(true);
    }
    return HCCL_SUCCESS;
}

HcclResult AllReduceLayered::RunTemplateOnComm(std::unique_ptr<AlgTemplateBase> tempAlg, const SubCommInfo &commInfo,
    DeviceMem &inputMem, DeviceMem &outputMem, DeviceMem &scratchMem, u64 count, u64 offset,
    const std::vector<Slice> &slices, HcclReduceOp reduceOp, s32 stage, bool isCrossLevel)
{
    CHK_SMART_PTR_NULL(tempAlg);
    if (auto *allReduceRing = dynamic_cast<AllReduceRing *>(tempAlg.get())) {
        CHK_RET(allReduceRing->Prepare(reduceAttr_));
        CHK_RET(allReduceRing->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, reduceOp,
            LEVEL0_BRIDGE_RANK_ID, slices, offset));
        if (isCrossLevel) {
            CHK_RET(allReduceRing->RegisterProfiler(
                (commInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + commInfo.localRank,
                stage, HCCL_EXEC_STEP_NOT_SET, stream_));
            CHK_RET(allReduceRing->RunAsync(commInfo.localRank, commInfo.localRankSize, commInfo.links));
        } else {
            CHK_RET(allReduceRing->RunAsyncStaged(commInfo.localRank, commInfo.localRankSize, commInfo.links,
                RunStage::RUN_REDUCE_SCATTER));
        }
        return HCCL_SUCCESS;
    }
    if (auto *allReduceNb = dynamic_cast<AllReduceNB *>(tempAlg.get())) {
        CHK_RET(allReduceNb->Prepare(reduceAttr_));
        CHK_RET(allReduceNb->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, reduceOp,
            LEVEL0_BRIDGE_RANK_ID, slices, offset));
        if (isCrossLevel) {
            CHK_RET(allReduceNb->RegisterProfiler(
                (commInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + commInfo.localRank,
                stage, HCCL_EXEC_STEP_NOT_SET, stream_));
            CHK_RET(allReduceNb->RunAsync(commInfo.localRank, commInfo.localRankSize, commInfo.links));
        } else {
            CHK_RET(allReduceNb->RunAsyncStaged(commInfo.localRank, commInfo.localRankSize, commInfo.links,
                RunStage::RUN_REDUCE_SCATTER));
        }
        return HCCL_SUCCESS;
    }
    if (auto *allReduceHd = dynamic_cast<AllReduceRecursiveHalvingDoubling *>(tempAlg.get())) {
        CHK_RET(allReduceHd->Prepare(reduceAttr_));
        CHK_RET(allReduceHd->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, reduceOp,
            LEVEL0_BRIDGE_RANK_ID, slices, offset));
        if (isCrossLevel) {
            CHK_RET(allReduceHd->RegisterProfiler(
                (commInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + commInfo.localRank,
                stage, HCCL_EXEC_STEP_NOT_SET, stream_));
            CHK_RET(allReduceHd->RunAsync(commInfo.localRank, commInfo.localRankSize, commInfo.links));
        } else {
            CHK_RET(allReduceHd->RunAsyncStaged(commInfo.localRank, commInfo.localRankSize, commInfo.links,
                RunStage::RUN_REDUCE_SCATTER));
        }
        return HCCL_SUCCESS;
    }
    if (auto *allReduceNhrV1 = dynamic_cast<AllReduceNHRV1 *>(tempAlg.get())) {
        CHK_RET(allReduceNhrV1->Prepare(reduceAttr_));
        CHK_RET(allReduceNhrV1->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, reduceOp,
            LEVEL0_BRIDGE_RANK_ID, slices, offset));
        if (isCrossLevel) {
            CHK_RET(allReduceNhrV1->RegisterProfiler(
                (commInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + commInfo.localRank,
                stage, HCCL_EXEC_STEP_NOT_SET, stream_));
            CHK_RET(allReduceNhrV1->RunAsync(commInfo.localRank, commInfo.localRankSize, commInfo.links));
        } else {
            CHK_RET(allReduceNhrV1->RunAsyncStaged(commInfo.localRank, commInfo.localRankSize, commInfo.links,
                RunStage::RUN_REDUCE_SCATTER));
        }
        return HCCL_SUCCESS;
    }
    if (auto *allReduceNhr = dynamic_cast<AllReduceNHR *>(tempAlg.get())) {
        CHK_RET(allReduceNhr->Prepare(reduceAttr_));
        CHK_RET(allReduceNhr->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, reduceOp,
            LEVEL0_BRIDGE_RANK_ID, slices, offset));
    } else if (auto *allGatherNhr = dynamic_cast<AllGatherNHR *>(tempAlg.get())) {
        CHK_RET(PrepareAllGatherTemplate(*allGatherNhr));
        CHK_RET(static_cast<ExecutorBase &>(*allGatherNhr).Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, HCCL_REDUCE_RESERVED,
            LEVEL0_BRIDGE_RANK_ID, slices, offset));
    } else {
        CHK_RET(tempAlg->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, reduceOp,
            LEVEL0_BRIDGE_RANK_ID, slices, offset));
    }
    CHK_RET(tempAlg->RegisterProfiler((commInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + commInfo.localRank,
        stage, HCCL_EXEC_STEP_NOT_SET, stream_));
    return CollExecutorBase::RunTemplate(tempAlg, commInfo);
}

HcclResult AllReduceLayered::RunInnerReduceScatter()
{
    if (innerCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }
    return RunTemplateOnComm(CreateLevel1ReduceScatterTemplate(), innerCommInfo_, innerCclIn_, innerCclIn_, innerCclOut_,
        innerCount_, innerOffset_, innerDataSlices_, reduceType_, PROF_STAGE_1, false);
}

HcclResult AllReduceLayered::RunCrossAllReduce()
{
    if (crossCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }
    return RunTemplateOnComm(CreateLevel2AllReduceTemplate(), crossCommInfo_, crossCclIn_, crossCclOut_, crossCclOut_,
        crossCount_, crossOffset_, crossDataSlices_, reduceType_, PROF_STAGE_2, true);
}

HcclResult AllReduceLayered::RunInnerAllGather()
{
    if (innerCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }
    return RunTemplateOnComm(CreateLevel1AllGatherTemplate(), innerCommInfo_, innerCclOut_, innerCclOut_, innerCclIn_,
        innerCount_, innerOffset_, innerDataSlices_, HCCL_REDUCE_RESERVED, PROF_STAGE_2, false);
}
}  // namespace hccl
