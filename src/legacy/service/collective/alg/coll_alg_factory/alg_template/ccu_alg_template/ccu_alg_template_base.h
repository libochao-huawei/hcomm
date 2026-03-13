/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_CCU_ALG_TEMPLATE_BASE
#define HCCLV2_CCU_ALG_TEMPLATE_BASE

#include "const_val.h"
#include "template_utils.h"
#include "instruction.h"
#include "ins_queue.h"
#include "coll_operator.h"
#include "ccu_rank_group.h"

namespace Hccl {
using InsQuePtr = std::shared_ptr<InsQueue>;

// 定义NHRStepInfo结构体
struct NHRStepInfo {
    u32 step = 0;
    u32 myRank = 0;
    u32 nSlices;
    u32 toRank = 0;
    u32 fromRank = 0;
    std::vector<u32> txSliceIdxs;
    std::vector<u32> rxSliceIdxs;

    NHRStepInfo() : nSlices(0)
    {
    }
};

class CcuAlgTemplateBase {
public:
    explicit CcuAlgTemplateBase(const RankId virtualRank, const u32 tempRankSize,
                                  const std::vector<std::vector<RankId>> &tempVTopo,
                                  const std::map<RankId, u32> &tempVirtRankMap);
    virtual ~CcuAlgTemplateBase();

    virtual std::string Describe() const = 0;

    virtual HcclResult CalcRes(AlgTempResReq &tempResReq);
    virtual HcclResult CalcResDetour(const RankGraph *rankGraph, AlgTempResReq &tempResReq);
    virtual HcclResult CalcResDetour(ConnectedLinkMgr *linkMgr, AlgTempResReq &tempResReq);

    virtual HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    virtual HcclResult SetScratchBufferSize(uint64_t size);
    virtual HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize,
        RankSliceInfo &sliceInfoVec);
    virtual HcclResult GetScratchBufferInfo(const uint64_t scratchBufferSize, DataType dataType);
    virtual u64 CalcLoopMaxCount(ParamPool &paramPool);
    virtual HcclResult GetToken(const CollAlgOperator& op, uint64_t &token) const;
    virtual uint64_t BufferTypeToAddr(const BufferType bufferType);
    virtual  u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType);
    virtual HcclResult GetMaxTransPortDataSize(u64 &maxTransPortDataSize) const;
    virtual HcclResult CalNumBlocks(u32& numBlocks, u64 dataSize, u32 numBlocksLimit);

    // 辅助方法
    void CreateRankGroupFromTopo(RankGroup &rankGroup, size_t topoIndex) const;
    void CreateRankGroupsFrom2DTopo(RankGroup &rankGroupX, RankGroup &rankGroupY) const;
    virtual uint32_t virtRankId2RankId(const uint32_t virtRankId);
    virtual HcclResult GetStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo);
    virtual HcclResult GetReduceScatterStepInfo(u32 step, NHRStepInfo &stepInfo);
    virtual HcclResult GetAllGatherStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo);
    virtual HcclResult ProcessNHRStepInfo(std::vector<NHRStepInfo> &stepInfoVector, RankGroup &rankGroup, std::map<u32, u32> &indexMap, std::vector<LinkData> &linksDie0, std::vector<LinkData> &linksDie1, const ResLinks &tempLinks, uint32_t axisSize);

    void SetCollOp(const CollAlgOperator &op);
    void SetDmaMode(const DmaMode dmaMode);
    void SetDataType(const DataType &dataType);
    void SetRoot(const u32 root);
    void SetLoadInfo(const CollAlgParams &params);

protected:
    CollAlgOperator                  op_;
    RankId                           myRank_       = INVALID_RANKID;
    u32                              tempRankSize_ = 0;
    std::vector<std::vector<RankId>> tempVTopo_;
    std::map<RankId, u32>            tempVirtRankMap_;
    BuffInfo buffInfo_;
    OpMode opMode_;
    u32 rootId_{0};

    DmaMode dmaMode_ = DmaMode::DEFAULT;
    u32  linkNumBtwPeers_ = 1;
    DataType dataType_;
    uint64_t scratchBufferSize_ = 0;
    bool loadFromMem_ = false;
};
}
#endif // HCCLV2_CCU_ALG_TEMPLATE_BASE