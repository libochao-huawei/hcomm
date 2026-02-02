/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_CCU_INSTRUCTION_REDUCE_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_INSTRUCTION_REDUCE_MESH_1D_MEM2MEM_H_

#include <memory>
#include <map>
#include <vector>
#include <queue>
#include <string>

#include <sstream>
#include <ios>
#include <iostream>

#include "template_utils.h"
#include "instruction.h"
#include "ins_queue.h"
#include "ccu_context_utils.h"
#include "ccu_ctx_signature.h"
#include "ccu_ins.h"
#include "ccu_rank_group.h"

namespace Hccl {

// 为ReduceMeshMem2Mem1D实现的CCUIns、CCUCtxArg与CCUTaskArg
class CcuCtxArgReduceMeshMem2Mem1D : public CcuCtxArg {
public:
    explicit CcuCtxArgReduceMeshMem2Mem1D(const std::vector<uint64_t> &dimSize, uint32_t rankId, uint32_t rootId,
                                          const CollAlgOperator &op, const std::vector<std::vector<RankId>> &tempVTopo)
        : dimSize_(dimSize), rankId_(rankId), rootId_(rootId), op_(op), tempVTopo_(tempVTopo)
    {
    }
    CcuCtxSignature GetCtxSignature() const override
    {
        CcuCtxSignature signature;
        GenerateCcuCtxSignature(signature, CcuInstType::CCU_REDUCE_MESH_1D_MEM2MEM, op_, tempVTopo_);
        return signature;
    }

    std::vector<uint64_t>            dimSize_;
    uint32_t                         rankId_;
    uint32_t                         rootId_;
    CollAlgOperator                  op_;
    std::vector<std::vector<RankId>> tempVTopo_;
};

class CcuTaskArgReduceMeshMem2Mem1D : public CcuTaskArg {
public:
    explicit CcuTaskArgReduceMeshMem2Mem1D(uint64_t inputAddr, uint64_t outputAddr, uint64_t token,
                                            uint64_t bigDataSliceNum, uint64_t bigDataSliceSize, uint64_t smallDataSliceNum,
                                            uint64_t smallDataSliceSize, uint64_t inputRepeatStride, uint64_t outputRepeatStride,
                                            uint64_t normalSliceSize, uint64_t lastSliceSize, uint64_t repeatNumVar)
        : inputAddr_(inputAddr), outputAddr_(outputAddr), token_(token), bigDataSliceNum_(bigDataSliceNum),
          bigDataSliceSize_(bigDataSliceSize), smallDataSliceNum_(smallDataSliceNum), smallDataSliceSize_(smallDataSliceSize),
          inputRepeatStride_(inputRepeatStride), outputRepeatStride_(outputRepeatStride),
          normalSliceSize_(normalSliceSize), lastSliceSize_(lastSliceSize), repeatNumVar_(repeatNumVar)
    {
    }

    uint64_t inputAddr_;
    uint64_t outputAddr_;
    uint64_t token_;
    uint64_t bigDataSliceNum_;
    uint64_t bigDataSliceSize_;
    uint64_t smallDataSliceNum_;
    uint64_t smallDataSliceSize_;
    uint64_t inputRepeatStride_;
    uint64_t outputRepeatStride_;
    uint64_t normalSliceSize_;
    uint64_t lastSliceSize_;
    uint64_t repeatNumVar_;
};

class CcuInstructionReduceMeshMem2Mem1D : public CcuInstruction {
public:
    CcuInstructionReduceMeshMem2Mem1D() : CcuInstruction()
    {
    }

    void Init(uint32_t rankId, uint32_t rootId, const CollAlgOperator &op,
              const std::vector<std::vector<RankId>> &tempVTopo, uint64_t inputAddr, uint64_t outputAddr,
              uint64_t token, uint64_t bigDataSliceNum, uint64_t bigDataSliceSize,uint64_t smallDataSliceNum,
              uint64_t smallDataSliceSize, uint64_t inputRepeatStride, uint64_t outputRepeatStride,
              uint64_t normalSliceSize, uint64_t lastSliceSize, uint64_t repeatNumVar)
    {
        dimSize_.push_back(tempVTopo[0].size());
        rankId_ = rankId;
        rootId_ = rootId;
        op_                 = op;
        tempVTopo_          = tempVTopo;
        inputAddr_          = inputAddr;
        outputAddr_         = outputAddr;
        bigDataSliceNum_    = bigDataSliceNum;
        bigDataSliceSize_   = bigDataSliceSize;
        smallDataSliceNum_  = smallDataSliceNum;
        smallDataSliceSize_ = smallDataSliceSize;
        inputRepeatStride_  = inputRepeatStride;
        outputRepeatStride_ = outputRepeatStride;
        normalSliceSize_    = normalSliceSize;
        lastSliceSize_      = lastSliceSize;
        repeatNumVar_       = repeatNumVar;
        token_              = token;
        return;
    }

    std::string Describe() const override
    {
        return StringFormat("CcuInstructionReduceMeshMem2Mem1D rankId [%u], instType[%d]", rankId_, instType_);
    }

    CcuInstType GetInstType() const override
    {
        return instType_;
    }

    void SetInstType(CcuInstType instType)
    {
        instType_ = instType;
    }

    std::unique_ptr<CcuCtxArg> GetCtxArg() const override
    {
        HCCL_INFO("[ReduceAlgo]GetCtxArg begin");
        return std::make_unique<CcuCtxArgReduceMeshMem2Mem1D>(dimSize_, rankId_, rootId_, op_, tempVTopo_);
    }

    std::unique_ptr<CcuTaskArg> GetTaskArg() const override
    {
        HCCL_INFO("[ReduceAlgo]GetTaskArg begin");
        return std::make_unique<CcuTaskArgReduceMeshMem2Mem1D>(inputAddr_, outputAddr_, token_, bigDataSliceNum_,
                                                                bigDataSliceSize_, smallDataSliceNum_, smallDataSliceSize_,
                                                                inputRepeatStride_, outputRepeatStride_, normalSliceSize_,
                                                                lastSliceSize_, repeatNumVar_);
    }

    std::vector<LinkData> GetLinks() const override
    {
        return links_;
    }

    void SetLinks(std::vector<LinkData> &links)
    {
        links_ = links;
    }

    RankGroup GetRankGroup() const override
    {
        return rankGroup_;
    }

    void SetRankGroup(RankGroup &rankGroup)
    {
        rankGroup_ = rankGroup;
    }

private:
    CcuInstType instType_ = CcuInstType::CCU_REDUCE_MESH_1D_MEM2MEM;
    std::vector<uint64_t>            dimSize_;
    uint32_t                         rankId_{0};
    uint32_t                         rootId_{0};
    CollAlgOperator                  op_;
    std::vector<std::vector<RankId>> tempVTopo_;
    uint64_t                         inputAddr_{0};
    uint64_t                         outputAddr_{0};
    uint64_t                         token_{0};
    uint64_t                         bigDataSliceSize_{0};
    uint64_t                         bigDataSliceNum_{0};
    uint64_t                         smallDataSliceSize_{0};
    uint64_t                         smallDataSliceNum_{0};
    uint64_t                         inputRepeatStride_{0};
    uint64_t                         outputRepeatStride_{0};
    uint64_t                         normalSliceSize_{0};
    uint64_t                         lastSliceSize_{0};
    uint64_t                         repeatNumVar_{0};
    RankGroup                        rankGroup_;
    std::vector<LinkData>            links_;
};
 
} // namespace Hccl
#endif // HCCLV2_CCU_INSTRUCTION_REDUCE_MESH_1D_MEM2MEM_H_
