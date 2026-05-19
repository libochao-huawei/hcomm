/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "layered_base.h"

#include "sal_pub.h"

namespace hccl {
namespace {
CommType GetLevel1CommType(AlgTypeLevel1 algType)
{
    switch (algType) {
        case AlgTypeLevel1::ALG_LEVEL1_HD:
            return CommType::COMM_TAG_HALVING_DOUBLING;
        case AlgTypeLevel1::ALG_LEVEL1_NB:
            return CommType::COMM_TAG_NONUNIFORM_BRUCK;
        case AlgTypeLevel1::ALG_LEVEL1_NHR:
            return CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING;
        case AlgTypeLevel1::ALG_LEVEL1_RING:
        default:
            return CommType::COMM_TAG_RING_INNER;
    }
}

CommType GetLevel2CommType(AlgTypeLevel2 algType)
{
    switch (algType) {
        case AlgTypeLevel2::ALG_LEVEL2_HD:
            return CommType::COMM_TAG_HALVING_DOUBLING;
        case AlgTypeLevel2::ALG_LEVEL2_NB:
            return CommType::COMM_TAG_NONUNIFORM_BRUCK;
        case AlgTypeLevel2::ALG_LEVEL2_NHR:
            return CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING;
        case AlgTypeLevel2::ALG_LEVEL2_RING:
        default:
            return CommType::COMM_TAG_RING_INNER;
    }
}

DeviceMem SelectLayeredMem(const DeviceMem &layeredMem, const DeviceMem &fallbackMem)
{
    return layeredMem ? layeredMem : fallbackMem;
}
} // namespace

LayeredBase::LayeredBase(const HcclDispatcher dispatcher)
    : ExecutorBase(dispatcher)
{
}

LayeredBase::~LayeredBase()
{
}

bool LayeredBase::IsSupportLayered(u32 groupSize, AlgType algType)
{
    return groupSize > HCCL_RANK_SIZE_EQ_ONE && LayeredBase::IsSupportAlgType(algType);
}

bool LayeredBase::IsSupportAlgType(AlgType algType)
{
    switch (algType.algoLevel1) {
        case AlgTypeLevel1::ALG_LEVEL1_HD:
        case AlgTypeLevel1::ALG_LEVEL1_NB:
        case AlgTypeLevel1::ALG_LEVEL1_NHR:
        case AlgTypeLevel1::ALG_LEVEL1_RING:
            return true;
        default:
            return false;
    }
}

CommType LayeredBase::GetLevel1CommType(const HcclAlgoInfo &algoInfo, AlgType algType)
{
    (void)algoInfo;
    return hccl::GetLevel1CommType(algType.algoLevel1);
}

CommType LayeredBase::GetLevel2CommType(const HcclAlgoInfo &algoInfo, AlgType algType)
{
    (void)algoInfo;
    return hccl::GetLevel2CommType(algType.algoLevel2);
}

CommType LayeredBase::GetLevel2CommType(HcclAlgoType interAlgType)
{
    switch (interAlgType) {
        case HcclAlgoType::HCCL_ALGO_TYPE_HDR:
            return CommType::COMM_TAG_HALVING_DOUBLING;
        case HcclAlgoType::HCCL_ALGO_TYPE_NB:
            return CommType::COMM_TAG_NONUNIFORM_BRUCK;
        case HcclAlgoType::HCCL_ALGO_TYPE_NHR:
            return CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING;
        case HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT:
        case HcclAlgoType::HCCL_ALGO_TYPE_RING:
        default:
            // Current hcomm CommType has no COMM_TAG_MTR; keep the prep-slice fallback ring-based.
            return CommType::COMM_TAG_RING_INNER;
    }
}

HcclResult LayeredBase::Prepare(const OpParam &opParam, ExecMem &execMem, const LayeredParam &layeredParam)
{
    layeredParam_ = std::make_shared<LayeredParam>(layeredParam);

    algoAttr_ = layeredParam.algoAttr;
    topoAttr_ = layeredParam.topoAttr;
    algType_ = layeredParam.algType;
    interAlgType_ = layeredParam.interAlgType;
    stream_ = opParam.stream;
    count_ = layeredParam.count;
    dataType_ = opParam.GetDataType();
    reduceType_ = opParam.reduceType;
    CHK_RET(SalGetDataTypeSize(dataType_, dataUnit_));

    baseOffset_ = layeredParam.baseOffset;
    inputMem_ = SelectLayeredMem(layeredParam.inputMem, execMem.inputMem);
    outputMem_ = SelectLayeredMem(layeredParam.outputMem, execMem.outputMem);
    scratchMem_ = SelectLayeredMem(layeredParam.scratchMem, execMem.scratchMem);

    outerCommInfo_ = layeredParam.outerCommInfo;
    outerCommRank_ = outerCommInfo_.localRank;
    outerCommSize_ = outerCommInfo_.localRankSize;

    innerCommInfo_ = layeredParam.innerCommInfo;
    innerCommRank_ = innerCommInfo_.localRank;
    innerCommSize_ = innerCommInfo_.localRankSize;

    crossCommInfo_ = layeredParam.crossCommInfo;
    crossCommRank_ = crossCommInfo_.localRank;
    crossCommSize_ = crossCommInfo_.localRankSize;

    outerDataSlices_ = layeredParam.outerDataSlices;
    innerDataSlices_ = layeredParam.innerDataSlices;
    crossDataSlices_ = layeredParam.crossDataSlices;
    // Reference-style layered base only caches shared topology / buffer context.
    // Operator-specific inner/cross slice and buffer interpretation must be rebuilt
    // by each layered template in its own Prepare()/PrepareSlicesAndBuffers() path.
    innerSlice_ = Slice{};
    innerCount_ = 0;
    innerBytes_ = 0;
    innerOffset_ = 0;
    innerCclIn_ = DeviceMem();
    innerCclOut_ = DeviceMem();

    crossSlice_ = Slice{};
    crossCount_ = 0;
    crossBytes_ = 0;
    crossOffset_ = 0;
    crossCclIn_ = DeviceMem();
    crossCclOut_ = DeviceMem();

    HCCL_INFO("[LayeredBase][Prepare] userRank[%u] userRankSize[%u] count[%llu] outerCommRank[%u] outerCommSize[%u] "
        "innerCommRank[%u] innerCommSize[%u] crossCommRank[%u] crossCommSize[%u]",
        topoAttr_.userRank, topoAttr_.userRankSize, count_, outerCommRank_, outerCommSize_,
        innerCommRank_, innerCommSize_, crossCommRank_, crossCommSize_);

    return HCCL_SUCCESS;
}
}  // namespace hccl
