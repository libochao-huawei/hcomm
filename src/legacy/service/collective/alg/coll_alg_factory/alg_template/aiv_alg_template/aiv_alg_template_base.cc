/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板AivAlgTemplateBase类实现
 * Author: caixin
 * Create: 2025-09-05
 */

#include "aiv_alg_template_base.h"

namespace Hccl {


AivAlgTemplateBase::AivAlgTemplateBase(const RankId virtualRank, const u32 tempRankSize,
                                       const std::vector<std::vector<RankId>> &tempVTopo,
                                       const std::map<RankId, u32>            &tempVirtRankMap)
    : myRank_(virtualRank), tempRankSize_(tempRankSize), tempVTopo_(tempVTopo), tempVirtRankMap_(tempVirtRankMap)
{
}

AivAlgTemplateBase::~AivAlgTemplateBase()
{
}

void AivAlgTemplateBase::SetCollOp(const CollAlgOperator &op)
{
    op_ = op;
    return;
}

void AivAlgTemplateBase::SetDmaMode(const DmaMode dmaMode)
{
    dmaMode_ = dmaMode;
    return;
}

void AivAlgTemplateBase::InitReduceInfo(const ReduceOp &redOp, const DataType &dataType)
{
    reduceOp_ = redOp;
    dataType_ = dataType;
    return;
}

void AivAlgTemplateBase::SetDataType(const DataType &dataType)
{
    dataType_ = dataType;
    return;
}

void AivAlgTemplateBase::SetRoot(const u32 root)
{
    root_ = root;
    return;
}

HcclResult AivAlgTemplateBase::CalcRes(AlgTempResReq &tempResReq)
{
    (void)tempResReq;
    HCCL_ERROR("[AivAlgTemplateBase] Unsupported interface of resource calculation!");
    return HcclResult::HCCL_E_INTERNAL;
}

HcclResult AivAlgTemplateBase::CalcResDetour(const RankGraph *rankGraph, AlgTempResReq &tempResReq)
{
    (void)rankGraph;
    (void)tempResReq;
    HCCL_ERROR("[AivAlgTemplateBase] Current alg do not support detour mode!");
    return HcclResult::HCCL_E_INTERNAL;
}

HcclResult AivAlgTemplateBase::CalcResDetour(ConnectedLinkMgr *linkMgr, AlgTempResReq &tempResReq)
{
    (void)linkMgr;
    (void)tempResReq;
    HCCL_ERROR("[AivAlgTemplateBase] Current alg do not support detour mode!");
    return HcclResult::HCCL_E_INTERNAL;
}

u32 AivAlgTemplateBase::CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType)
{
    (void)inBuffType;
    (void)outBuffType;
    // AIV模式默认返回一整块Scratch
    return 1;
}

HcclResult AivAlgTemplateBase::GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, 
    const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues)
{
    (void)tempFuncs;
    (void)templateDataParams;
    (void)tempLinks;
    (void)tempInsQues;
    HCCL_ERROR("[AivAlgTemplateBase] Unsupported interface of instruction generation!");
    return HcclResult::HCCL_E_INTERNAL;
}

void AivAlgTemplateBase::IncSliceId()
{
    sliceId_++;
    return;
}

HcclResult AivAlgTemplateBase::CalBlockDim(u32& blockDim, u64 dataSize, u32 blockDimLimit)
{
    (void) dataSize;
    if (blockDimLimit >= tempRankSize_) {
        blockDim = tempRankSize_;
    } else {
        blockDim = blockDimLimit;
    } 
    HCCL_INFO("[AivAlgTemplateBase] Actually use core num[%u]", blockDim);
    return HCCL_SUCCESS;
}

} // namespace Hccl