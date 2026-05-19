/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "reduce_scatter_layered.h"

#include <numeric>

#include "op_context.h"
#include "sal_pub.h"

namespace hccl {
namespace {
void PrepareReduceScatterSlices(u64 dataCount, u32 unitSize, u32 sliceNum, std::vector<Slice> &dataSlice)
{
    dataSlice.resize(sliceNum);
    const u64 sliceSize = dataCount * unitSize;
    for (u32 i = 0; i < sliceNum; i++) {
        dataSlice[i].size = sliceSize;
        dataSlice[i].offset = i * sliceSize;
    }
}
} // namespace

ReduceScatterLayered::ReduceScatterLayered(const HcclDispatcher dispatcher, u64 reduceAttr)
    : LayeredBase(dispatcher), reduceAttr_(reduceAttr)
{
}

ReduceScatterLayered::~ReduceScatterLayered() = default;

HcclResult ReduceScatterLayered::CopyOutBypassForTest(DeviceMem &dstMem, const DeviceMem &srcMem, Stream &stream)
{
    return HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, stream);
}

void ReduceScatterLayered::SetOuterReduceScatterContext(const HcomCollOpInfo *opInfo,
    const std::vector<std::vector<Slice>> &multiStreamSlice,
    const std::vector<std::vector<Slice>> &level0DataSegsSlice, u32 perDataSize)
{
    outerOpInfo_ = opInfo;
    outerPerDataSize_ = perDataSize;
    outerMultiStreamSlice_ = multiStreamSlice;
    outerLevel0DataSegsSlice_ = level0DataSegsSlice;
    outerUserMemSlices_.clear();
}

HcclResult ReduceScatterLayered::RunAsync(
    const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam)
{
    (void)opParam;
    (void)execMem;
    (void)layeredParam;
    CHK_RET(RunInnerReduceScatter());
    CHK_RET(RunCrossReduceScatter());
    HCCL_INFO("[ReduceScatterLayered][RunAsync] success.");
    return HCCL_SUCCESS;
}

HcclResult ReduceScatterLayered::Prepare(
    const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam)
{
    CHK_RET(LayeredBase::Prepare(opParam, execMem, layeredParam));
    CHK_RET(PrepareSlicesAndBuffers(execMem));
    return HCCL_SUCCESS;
}

HcclResult ReduceScatterLayered::PrepareSlicesAndBuffers(const ExecMem &execMem)
{
    CHK_PRT_RET(dataUnit_ == 0, HCCL_ERROR("[ReduceScatterLayered][PrepareSlicesAndBuffers] dataUnit is zero."),
        HCCL_E_INTERNAL);
    CHK_PRT_RET(outerCommSize_ == 0 || innerCommSize_ == 0 || crossCommSize_ == 0,
        HCCL_ERROR("[ReduceScatterLayered][PrepareSlicesAndBuffers] invalid comm sizes outer[%u] inner[%u] cross[%u].",
            outerCommSize_, innerCommSize_, crossCommSize_), HCCL_E_INTERNAL);

    const u64 outerCount = execMem.count * topoAttr_.userRankSize;
    const u64 outerBytes = outerCount * dataUnit_;
    innerBytes_ = outerBytes / outerCommSize_;
    innerCount_ = innerBytes_ / dataUnit_;
    innerOffset_ = innerBytes_ * outerCommRank_;
    PrepareReduceScatterSlices(innerCount_ / innerCommSize_, dataUnit_, innerCommSize_, innerDataSlices_);
    CHK_PRT_RET(innerCommRank_ >= innerDataSlices_.size(),
        HCCL_ERROR("[ReduceScatterLayered][PrepareSlicesAndBuffers] innerRank[%u] >= sliceSize[%zu].",
            innerCommRank_, innerDataSlices_.size()), HCCL_E_INTERNAL);

    crossBytes_ = innerBytes_ / innerCommSize_;
    crossCount_ = crossBytes_ / dataUnit_;
    crossOffset_ = innerOffset_ + crossBytes_ * innerCommRank_;
    PrepareReduceScatterSlices(crossCount_ / crossCommSize_, dataUnit_, crossCommSize_, crossDataSlices_);
    CHK_PRT_RET(crossCommRank_ >= crossDataSlices_.size(),
        HCCL_ERROR("[ReduceScatterLayered][PrepareSlicesAndBuffers] crossRank[%u] >= sliceSize[%zu].",
            crossCommRank_, crossDataSlices_.size()), HCCL_E_INTERNAL);

    innerCclIn_ = inputMem_.range(innerOffset_, innerBytes_);
    CHK_SMART_PTR_NULL(innerCclIn_);
    innerCclOut_ = scratchMem_.range(innerOffset_, innerBytes_);
    CHK_SMART_PTR_NULL(innerCclOut_);
    crossCclIn_ = inputMem_.range(crossOffset_, crossBytes_);
    CHK_SMART_PTR_NULL(crossCclIn_);
    crossCclOut_ = scratchMem_.range(crossOffset_, crossBytes_);
    CHK_SMART_PTR_NULL(crossCclOut_);

    HCCL_INFO("[ReduceScatterLayered][PrepareSlicesAndBuffers] innerOffset[%llu] innerBytes[%llu] "
        "crossOffset[%llu] crossBytes[%llu].", innerOffset_, innerBytes_, crossOffset_, crossBytes_);
    return HCCL_SUCCESS;
}

HcclResult ReduceScatterLayered::BuildOuterUserMemSlices(const OpParam &opParam, const ExecMem &execMem)
{
    (void)opParam;
    outerUserMemSlices_.clear();
    if (outerOpInfo_ == nullptr) {
        outerUserMemSlices_ = outerLevel0DataSegsSlice_;
        return HCCL_SUCCESS;
    }

    for (u32 ringIndex = 0; ringIndex < outerLevel0DataSegsSlice_.size(); ++ringIndex) {
        std::vector<Slice> userMemSlices;
        for (const auto &cclSlice : outerLevel0DataSegsSlice_[ringIndex]) {
            Slice userSlice;
            const u64 count = (outerOpInfo_->strideCount == 0) ? outerOpInfo_->count : outerOpInfo_->strideCount;
            userSlice.size = cclSlice.size;
            CHK_PRT_RET(execMem.outputMem.size() == 0,
                HCCL_ERROR("[ReduceScatterLayered][BuildOuterUserMemSlices] ccl output mem size is zero"),
                HCCL_E_PARA);
            userSlice.offset = (cclSlice.offset / execMem.outputMem.size()) * count * outerPerDataSize_ +
                outerMultiStreamSlice_[ringIndex][0].offset;
            userMemSlices.push_back(userSlice);
        }
        outerUserMemSlices_.push_back(userMemSlices);
    }
    return HCCL_SUCCESS;
}

HcclResult ReduceScatterLayered::UpdateOuterUserMemSlicesForStride(const OpParam &opParam)
{
    const u64 strideCount = (outerOpInfo_ == nullptr) ? opParam.GetStrideCount() : outerOpInfo_->strideCount;
    if (strideCount == 0) {
        return HCCL_SUCCESS;
    }
    const u64 count = (outerOpInfo_ == nullptr) ? opParam.GetDataCount(topoAttr_.userRank) : outerOpInfo_->count;

    for (u32 ringIndex = 0; ringIndex < outerUserMemSlices_.size(); ++ringIndex) {
        for (u32 sliceIndex = 0; sliceIndex < outerUserMemSlices_[ringIndex].size(); ++sliceIndex) {
            const u64 selfRank = outerUserMemSlices_[ringIndex][sliceIndex].offset /
                (count * outerPerDataSize_);
            outerUserMemSlices_[ringIndex][sliceIndex].offset +=
                selfRank * ((strideCount - count) * outerPerDataSize_);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ReduceScatterLayered::CopyIn(const OpParam &opParam, hccl::ExecMem &execMem, const LayeredParam &layeredParam)
{
    (void)layeredParam;
    CHK_RET(BuildOuterUserMemSlices(opParam, execMem));
    return UpdateOuterUserMemSlicesForStride(opParam);
}

u64 ReduceScatterLayered::CalcCopyOutSrcOffset(const ExecMem &execMem, const OpParam &opParam, u32 perDataSize) const
{
    if (opParam.opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V) {
        const auto *counts = static_cast<u64 *>(opParam.VDataDes.counts);
        return std::accumulate(counts, counts + topoAttr_.userRank, 0ULL) * perDataSize;
    }
    return topoAttr_.userRank * execMem.outputMem.size();
}

HcclResult ReduceScatterLayered::CopyOut(const OpParam &opParam, hccl::ExecMem &execMem,
    const LayeredParam &layeredParam)
{
    (void)layeredParam;
    const u32 unitSize = SIZE_TABLE[opParam.DataDes.dataType];
    CHK_PRT_RET(unitSize == 0,
        HCCL_ERROR("[ReduceScatterLayered][CopyOut] unitSize is zero for dataType[%d].", opParam.DataDes.dataType),
        HCCL_E_PARA);
    const u64 curSize = execMem.count * unitSize;
    if (innerCommSize_ == 1 && crossCommSize_ == 1 && outerOpInfo_ == nullptr) {
        const u64 offset = topoAttr_.userRank * curSize;
        DeviceMem srcMem = execMem.inputMem.range(offset, curSize);
        CHK_SMART_PTR_NULL(srcMem);
        DeviceMem dstMem = execMem.outputMem.range(0, curSize);
        CHK_SMART_PTR_NULL(dstMem);
        CHK_RET(CopyOutBypassForTest(dstMem, srcMem, stream_));
        if (execMem.outputPtr != nullptr) {
            DeviceMem userDst = DeviceMem::create(execMem.outputPtr, curSize);
            CHK_SMART_PTR_NULL(userDst);
            return CopyOutBypassForTest(userDst, srcMem, stream_);
        }
        return HCCL_SUCCESS;
    }
    if (innerCommSize_ <= 1 && crossCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }

    const u64 offset = CalcCopyOutSrcOffset(execMem, opParam, outerPerDataSize_);
    DeviceMem srcMem = execMem.inputMem.range(offset, curSize);
    CHK_SMART_PTR_NULL(srcMem);
    if (outerOpInfo_ != nullptr && outerOpInfo_->outputAddr != nullptr) {
        DeviceMem dstMem = DeviceMem::create(static_cast<u8 *>(outerOpInfo_->outputAddr), curSize);
        CHK_SMART_PTR_NULL(dstMem);
        return CopyOutBypassForTest(dstMem, srcMem, stream_);
    }
    DeviceMem dstMem = execMem.outputMem.range(0, curSize);
    CHK_SMART_PTR_NULL(dstMem);
    CHK_RET(CopyOutBypassForTest(dstMem, srcMem, stream_));
    if (execMem.outputPtr != nullptr) {
        DeviceMem userDst = DeviceMem::create(execMem.outputPtr, curSize);
        CHK_SMART_PTR_NULL(userDst);
        return CopyOutBypassForTest(userDst, srcMem, stream_);
    }
    return HCCL_SUCCESS;
}

const std::vector<std::vector<Slice>> &ReduceScatterLayered::GetOuterUserMemSlices() const
{
    return outerUserMemSlices_;
}

std::unique_ptr<ExecutorBase> ReduceScatterLayered::CreateLevel1Executor() const
{
    switch (algType_.algoLevel1) {
        case AlgTypeLevel1::ALG_LEVEL1_HD:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterRecursiveHalvingDoubling(dispatcher_));
        case AlgTypeLevel1::ALG_LEVEL1_NB:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterNB(dispatcher_));
        case AlgTypeLevel1::ALG_LEVEL1_NHR:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterNHR(dispatcher_));
        case AlgTypeLevel1::ALG_LEVEL1_RING:
        default:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterRing(dispatcher_));
    }
}

std::unique_ptr<ExecutorBase> ReduceScatterLayered::CreateLevel2Executor() const
{
    switch (interAlgType_) {
        case HcclAlgoType::HCCL_ALGO_TYPE_HDR:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterRecursiveHalvingDoubling(dispatcher_));
        case HcclAlgoType::HCCL_ALGO_TYPE_NB:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterNB(dispatcher_));
        case HcclAlgoType::HCCL_ALGO_TYPE_NHR:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterNHR(dispatcher_));
        case HcclAlgoType::HCCL_ALGO_TYPE_NHR_V1:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterNHRV1(dispatcher_));
        default:
            return std::unique_ptr<ExecutorBase>(new (std::nothrow) ReduceScatterRing(dispatcher_));
    }
}

HcclResult ReduceScatterLayered::PrepareExecutor(ExecutorBase &executor, DeviceMem &inputMem, DeviceMem &outputMem,
    DeviceMem &scratchMem, u64 count, u64 offset, const std::vector<Slice> &slices, bool useInterAlgForLevel2) const
{
    (void)inputMem;
    (void)outputMem;
    (void)scratchMem;
    (void)count;
    (void)offset;
    (void)slices;
    (void)useInterAlgForLevel2;
    if (dynamic_cast<ReduceScatterNHR*>(&executor) != nullptr) {
        auto &nhr = static_cast<ReduceScatterNHR&>(executor);
        return nhr.Prepare(reduceAttr_, false);
    }
    return executor.Prepare(reduceAttr_, static_cast<HcomCollOpInfo *>(nullptr));
}

HcclResult ReduceScatterLayered::RunOnComm(std::unique_ptr<ExecutorBase> executor, const SubCommInfo &commInfo,
    DeviceMem &inputMem, DeviceMem &outputMem, DeviceMem &scratchMem, u64 count, u64 offset,
    const std::vector<Slice> &slices, s32 stage)
{
    CHK_SMART_PTR_NULL(executor);
    CHK_RET(PrepareExecutor(*executor, inputMem, outputMem, scratchMem, count, offset, slices, stage == PROF_STAGE_2));
    CHK_RET(executor->Prepare(inputMem, outputMem, scratchMem, count, dataType_, stream_, reduceType_,
        LEVEL0_BRIDGE_RANK_ID, slices, offset));
    CHK_RET(executor->RegisterProfiler(
        (commInfo.localRankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + commInfo.localRank,
        stage, HCCL_EXEC_STEP_NOT_SET, stream_));
    HcclResult ret = executor->RunAsync(commInfo.localRank, commInfo.localRankSize, commInfo.links);
    CHK_PRT_RET(ret == HCCL_E_AGAIN, HCCL_WARNING("[ReduceScatterLayered][RunOnComm] group destroyed."), ret);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[ReduceScatterLayered][RunOnComm] rank[%u] rank size[%u] failed", commInfo.localRank,
            commInfo.localRankSize), ret);
    return HCCL_SUCCESS;
}

HcclResult ReduceScatterLayered::RunInnerReduceScatter()
{
    if (innerCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }
    return RunOnComm(CreateLevel1Executor(), innerCommInfo_, innerCclIn_, innerCclIn_, innerCclOut_, innerCount_,
        innerOffset_, innerDataSlices_, PROF_STAGE_1);
}

HcclResult ReduceScatterLayered::RunCrossReduceScatter()
{
    if (crossCommSize_ <= 1) {
        return HCCL_SUCCESS;
    }

    std::unique_ptr<ExecutorBase> executor = CreateLevel2Executor();
    CHK_SMART_PTR_NULL(executor);
    if (dynamic_cast<ReduceScatterNHR*>(&*executor) != nullptr && algoAttr_.isSupportAtomicWrite) {
        executor->CloseBarrier();
    }
    return RunOnComm(std::move(executor), crossCommInfo_, crossCclIn_, crossCclIn_, crossCclOut_, crossCount_,
        crossOffset_, crossDataSlices_, PROF_STAGE_2);
}
}  // namespace hccl
