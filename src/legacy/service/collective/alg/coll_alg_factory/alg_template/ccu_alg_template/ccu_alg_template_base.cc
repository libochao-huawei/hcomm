/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_alg_template_base.h"
#include "ccu_context_utils.h"
#include "env_config.h"
#include "ccu_assist.h"
#include "log.h"
#include "ccu_instruction_reduce_nhr1d_mem2mem.h"

namespace Hccl {
CcuAlgTemplateBase::CcuAlgTemplateBase(const RankId virtualRank, const u32 tempRankSize,
                                       const std::vector<std::vector<RankId>> &tempVTopo,
                                       const std::map<RankId, u32>            &tempVirtRankMap)
    : myRank_(virtualRank), tempRankSize_(tempRankSize), tempVTopo_(tempVTopo), tempVirtRankMap_(tempVirtRankMap)
{ 
}

CcuAlgTemplateBase::~CcuAlgTemplateBase()
{
}

HcclResult CcuAlgTemplateBase::CalcRes(AlgTempResReq &tempResReq)
{
    (void)tempResReq;
    HCCL_ERROR("[CcuAlgTemplateBase] [CalcRes] Current alg do not support detour mode!");
    return HcclResult::HCCL_E_INTERNAL;
}

HcclResult CcuAlgTemplateBase::CalcResDetour(const RankGraph *rankGraph, AlgTempResReq &tempResReq)
{
    (void)rankGraph;
    (void)tempResReq;
    HCCL_ERROR("[CcuAlgTemplateBase] [CalcRes] Current alg do not support detour mode!");
    return HcclResult::HCCL_E_INTERNAL;
}

HcclResult CcuAlgTemplateBase::CalcResDetour(ConnectedLinkMgr *linkMgr, AlgTempResReq &tempResReq)
{
    (void)linkMgr;
    (void)tempResReq;
    HCCL_ERROR("[CcuAlgTemplateBase] [CalcRes] Current alg do not support detour mode!");
    return HcclResult::HCCL_E_INTERNAL;
}

HcclResult CcuAlgTemplateBase::Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec,
    const BuffInfo &buffInfo, const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues)
{
    (void)tempFuncs;
    (void)sliceInfoVec;
    (void)buffInfo;
    (void)tempLinks;
    (void)tempInsQues;
    HCCL_ERROR("[CcuAlgTemplateBase] Unsupported interface of CcuAlgTemplateBase::Run!");
    return HcclResult::HCCL_E_INTERNAL;
}

HcclResult CcuAlgTemplateBase::SetScratchBufferSize(uint64_t size)
{
    scratchBufferSize_ = size;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuAlgTemplateBase::CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize,
        RankSliceInfo &sliceInfoVec)
{
    (void)allignInfo;
    (void)dataSize;
    (void)sliceInfoVec;
    HCCL_WARNING("[CcuAlgTemplateBase] Interface of CcuAlgTemplateBase::CalcSliceInfo is not implemented!");
    return HcclResult::HCCL_SUCCESS;
}

void CcuAlgTemplateBase::SetDmaMode(const DmaMode dmaMode)
{
    dmaMode_ = dmaMode;
    return;
}

void CcuAlgTemplateBase::SetCollOp(const CollAlgOperator &op)
{
    op_ = op;
    return;
}

void CcuAlgTemplateBase::SetDataType(const DataType &dataType)
{
    dataType_ = dataType;
    return;
}

HcclResult CcuAlgTemplateBase::GetScratchBufferInfo(const uint64_t scratchBufferSize, DataType dataType)
{
    (void)scratchBufferSize;
    (void)dataType;
    return HcclResult::HCCL_SUCCESS;
}

void CcuAlgTemplateBase::SetRoot(const u32 root)
{
    rootId_ = root;
    return;
}

void CcuAlgTemplateBase::SetLoadInfo(const CollAlgParams &params)
{
    loadFromMem_ = params.isMc2;  // 当前只有mc2场景会设置该标记，故暂作为mc2标记使用
    return;
}

u64 CcuAlgTemplateBase::CalcLoopMaxCount(ParamPool &paramPool)
{
    u64 loopMaxCount = 0;
    if (paramPool.params.opMode == OpMode::OPBASE) {
        u64 maxLoopSize = std::min(static_cast<u64>(paramPool.params.maxTmpMemSize), static_cast<u64>(UB_MAX_DATA_SIZE));
        loopMaxCount = maxLoopSize / (DataTypeSizeGet(paramPool.op.dataType) * tempRankSize_) * tempRankSize_;
    } else {
        loopMaxCount = paramPool.op.dataCount;
    }
    return loopMaxCount;
}

HcclResult CcuAlgTemplateBase::GetToken(const CollAlgOperator &op, uint64_t &token) const
{
    if (op.inputMem != nullptr && op.inputMem->GetAddr() != 0) {
        token = CcuRep::GetTokenInfo(static_cast<uint64_t>(op.inputMem->GetAddr()),
                                     static_cast<uint64_t>(op.inputMem->GetSize()));
        return HCCL_SUCCESS;
    } else if (op.outputMem != nullptr && op.outputMem->GetAddr() != 0) {
        token = CcuRep::GetTokenInfo(static_cast<uint64_t>(op.outputMem->GetAddr()),
                                     static_cast<uint64_t>(op.outputMem->GetSize()));
        return HCCL_SUCCESS;
    } else if (op.scratchMem != nullptr && op.scratchMem->GetAddr() != 0) {
        token = CcuRep::GetTokenInfo(static_cast<uint64_t>(op.scratchMem->GetAddr()),
                                     static_cast<uint64_t>(op.scratchMem->GetSize()));
        return HCCL_SUCCESS;
    }
    HCCL_ERROR("[GetToken] Both inputMem and outputMem are null");
    return HCCL_E_PTR;
}
u32 CcuAlgTemplateBase::CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType)
{
    (void) inBuffType;
    (void) outBuffType;
    return 0;
}

HcclResult CcuAlgTemplateBase::GetMaxTransPortDataSize(u64 &maxTransPortDataSize) const
{
    maxTransPortDataSize = MAX_LOOP_GROUP_TRANS_SIZE;
    return HCCL_SUCCESS;
}

uint64_t CcuAlgTemplateBase::BufferTypeToAddr(const BufferType bufferType)
{
    if (bufferType == BufferType::INPUT && op_.inputMem != nullptr) {
        return static_cast<uint64_t>(op_.inputMem->GetAddr());
    } else if (bufferType == BufferType::OUTPUT && op_.outputMem != nullptr) {
        return static_cast<uint64_t>(op_.outputMem->GetAddr());
    } else if (bufferType == BufferType::SCRATCH && op_.scratchMem != nullptr){
        return static_cast<uint64_t>(op_.scratchMem->GetAddr());
    } else {
        return 0;
    }
}

HcclResult CcuAlgTemplateBase::CalNumBlocks(u32& numBlocks, u64 dataSize, u32 numBlocksLimit)
{
    (void) numBlocks;
    (void) dataSize;
    (void) numBlocksLimit;
    HCCL_WARNING("CalNumBlocks not support ccu template.");
    return HCCL_SUCCESS;
}

void CcuAlgTemplateBase::CreateRankGroupFromTopo(RankGroup &rankGroup, size_t topoIndex) const
{
    if (topoIndex < tempVTopo_.size()) {
        for (auto &peer : tempVTopo_[topoIndex]) {
            rankGroup.AddRank(peer);
        }
    }
}

void CcuAlgTemplateBase::CreateRankGroupsFrom2DTopo(RankGroup &rankGroupX, RankGroup &rankGroupY) const
{
    CreateRankGroupFromTopo(rankGroupX, 0);
    CreateRankGroupFromTopo(rankGroupY, 1);
}

uint32_t CcuAlgTemplateBase::virtRankId2RankId(const uint32_t virtRankId)
{
    for(auto iter = tempVirtRankMap_.begin(); iter != tempVirtRankMap_.end(); iter++) {
        if(iter->second == virtRankId) {
            return iter->first;
        }
    }
    return 0;
}

HcclResult CcuAlgTemplateBase::GetStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo)
{
    u32 nStepsNHR = nSteps / 2;
    u32 realStep = step;
    if (realStep < nStepsNHR) {
        CHK_RET(GetReduceScatterStepInfo(realStep, stepInfo));
    } else {
        realStep = step % nStepsNHR;
        CHK_RET(GetAllGatherStepInfo(realStep, nStepsNHR, stepInfo));
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuAlgTemplateBase::GetReduceScatterStepInfo(u32 step, NHRStepInfo &stepInfo)
{
    u32 virtRankIdx = tempVirtRankMap_[myRank_];
    stepInfo.txSliceIdxs.clear();
    stepInfo.rxSliceIdxs.clear();
    stepInfo.step = step;
    stepInfo.myRank = virtRankIdx;

    // 计算通信对象
    u32 deltaRank = 1 << step;
    u32 sendTo = (virtRankIdx + tempRankSize_ - deltaRank) % tempRankSize_;
    u32 recvFrom = (virtRankIdx + deltaRank) % tempRankSize_;

    // 数据份数和数据编号增量
    u32 nSlices = (tempRankSize_ - 1 + (1 << step)) / (1 << (step + 1));
    u32 deltaSliceIndex = 1 << (step + 1);
    u32 rxSliceIdx = virtRankIdx;
    u32 txSliceIdx = (virtRankIdx - (1 << step) + tempRankSize_) % tempRankSize_;

    stepInfo.nSlices = nSlices;
    stepInfo.toRank = sendTo;
    stepInfo.fromRank = recvFrom;

    for (u32 i = 0; i < nSlices; i++) {
        stepInfo.txSliceIdxs.push_back(txSliceIdx);
        stepInfo.rxSliceIdxs.push_back(rxSliceIdx);

        HCCL_DEBUG("[NHR][GetReduceScatterStepInfo] i[%u] txSliceIdx[%u] rxSliceIdx[%u]", i, txSliceIdx, rxSliceIdx);

        txSliceIdx = (txSliceIdx + tempRankSize_ - deltaSliceIndex) % tempRankSize_;
        rxSliceIdx = (rxSliceIdx + tempRankSize_ - deltaSliceIndex) % tempRankSize_;
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuAlgTemplateBase::GetAllGatherStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo)
{
    u32 virtRankIdx = tempVirtRankMap_[myRank_];
    stepInfo.txSliceIdxs.clear();
    stepInfo.rxSliceIdxs.clear();
    stepInfo.step = step;
    stepInfo.myRank = virtRankIdx;

    // 计算通信对象
    u32 deltaRank = 1 << (nSteps - 1 - step);
    u32 recvFrom = (virtRankIdx + tempRankSize_ - deltaRank) % tempRankSize_;
    u32 sendTo = (virtRankIdx + deltaRank) % tempRankSize_;

    // 数据份数和数据编号增量
    u32 nSlices = (tempRankSize_ - 1 + (1 << (nSteps - 1 - step))) / (1 << (nSteps - step));
    u32 deltaSliceIndex = 1 << (nSteps - step);
    u32 txSliceIdx = virtRankIdx;
    u32 rxSliceIdx = (virtRankIdx - (1 << (nSteps - 1 - step)) + tempRankSize_) % tempRankSize_;

    stepInfo.nSlices = nSlices;
    stepInfo.toRank = sendTo;
    stepInfo.fromRank = recvFrom;

    for (u32 i = 0; i < nSlices; i++) {
        stepInfo.txSliceIdxs.push_back(txSliceIdx);
        stepInfo.rxSliceIdxs.push_back(rxSliceIdx);

        HCCL_DEBUG("[NHR][GetAllGatherStepInfo] i[%u] txSliceIdx[%u] rxSliceIdx[%u]", i, txSliceIdx, rxSliceIdx);

        txSliceIdx = (txSliceIdx + tempRankSize_ - deltaSliceIndex) % tempRankSize_;
        rxSliceIdx = (rxSliceIdx + tempRankSize_ - deltaSliceIndex) % tempRankSize_;
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuAlgTemplateBase::ProcessNHRStepInfo(std::vector<NHRStepInfo> &stepInfoVector, RankGroup &rankGroup, std::map<u32, u32> &indexMap, std::vector<LinkData> &linksDie0, std::vector<LinkData> &linksDie1, const ResLinks &tempLinks, uint32_t axisSize)
{
    u32 nSteps = GetNHRStepNum(tempRankSize_) * 2; // 分为RS和AG两次NHR
    for (u32 step = 0; step < nSteps; step++) {
        NHRStepInfo stepInfo;
        CHK_RET(GetStepInfo(step, nSteps, stepInfo));
        stepInfoVector.push_back(stepInfo);
        if (indexMap.count(stepInfo.fromRank) == 0) {
            u32 fromRankIdx = virtRankId2RankId(stepInfo.fromRank);
            indexMap[stepInfo.fromRank] = linksDie0.size();
            linksDie0.push_back(tempLinks.at(fromRankIdx)[0]);
            if (axisSize > 1) {
                linksDie1.push_back(tempLinks.at(fromRankIdx)[1]);
            }
            rankGroup.AddRank(fromRankIdx);
        }
        if (indexMap.count(stepInfo.toRank) == 0) {
            u32 toRankIdx = virtRankId2RankId(stepInfo.toRank);
            indexMap[stepInfo.toRank] = linksDie0.size();
            linksDie0.push_back(tempLinks.at(toRankIdx)[0]);
            if (axisSize > 1) {
                linksDie1.push_back(tempLinks.at(toRankIdx)[1]);
            }
            rankGroup.AddRank(toRankIdx);
        }
    }
    rankGroup.AddRank(myRank_);
    
    return HcclResult::HCCL_SUCCESS;
}

} // namespace Hccl