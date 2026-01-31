/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuInstrcutionReduceMeshMem2Mem2D类的头文件
 * Author: xiehuijiao
 * Create: 2025-11-5
 */

#ifndef HCCLV2_CCU_INSTRUCTION_REDUCE_MESH_2D_H_
#define HCCLV2_CCU_INSTRUCTION_REDUCE_MESH_2D_H_

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

// 为ReduceMeshMem2Mem2D实现的CCUIns、CCUCtxArg与CCUTaskArg
class CcuCtxArgReduceMeshMem2Mem2D : public CcuCtxArg {
public:
    explicit CcuCtxArgReduceMeshMem2Mem2D(const std::vector<uint64_t> &dSize, uint32_t rId, uint32_t rootId,
                                          uint32_t aId, const CollAlgOperator &op,
                                          const std::vector<std::vector<RankId>> &tempVTopo)
        : dimSize_(dSize), rankId_(rId), rootId_(rootId), axisId_(aId), op_(op), tempVTopo_(tempVTopo)
    {
    }
    CcuCtxSignature GetCtxSignature() const override
    {
        CcuCtxSignature signature;
        GenerateCcuCtxSignature(signature, CcuInstType::CCU_REDUCE_MESH_2D_MEM2MEM, op_, tempVTopo_);
        return signature;
    }

    std::vector<uint64_t>            dimSize_;
    uint32_t                         rankId_;
    uint32_t                         rootId_;
    uint32_t                         axisId_;
    CollAlgOperator                  op_;
    std::vector<std::vector<RankId>> tempVTopo_;
};

class CcuTaskArgReduceMeshMem2Mem2D : public CcuTaskArg {
public:
    explicit CcuTaskArgReduceMeshMem2Mem2D(uint64_t inputAddr, uint64_t outputAddr, uint64_t sliceSize,
                                           uint64_t xAxisSize, uint64_t yAxisSize, uint64_t token)
        : inputAddr_(inputAddr), outputAddr_(outputAddr), sliceSize_(sliceSize), xAxisSize_(xAxisSize),
          yAxisSize_(yAxisSize), token_(token)
    {
    }

    uint64_t inputAddr_;
    uint64_t outputAddr_;
    uint64_t sliceSize_;
    uint64_t xAxisSize_;
    uint64_t yAxisSize_;
    uint64_t token_;
};

class CcuInstructionReduceMeshMem2Mem2D : public CcuInstruction {
public:
    CcuInstructionReduceMeshMem2Mem2D() : CcuInstruction()
    {
    }

    void Init(std::vector<uint64_t> dimSize, uint32_t rankId, uint32_t rootId, uint64_t axisId, uint64_t inputAddr,
              uint64_t outputAddr, uint64_t sliceSize, uint64_t xAxisSize, uint64_t yAxisSize, uint64_t token,
              CollAlgOperator &op, std::vector<std::vector<RankId>> &tempVTopo)
    {
        dimSize_    = dimSize;
        rankId_     = rankId;
        rootId_     = rootId;
        axisId_     = axisId;
        inputAddr_  = inputAddr;
        outputAddr_ = outputAddr;
        sliceSize_  = sliceSize;
        xAxisSize_  = xAxisSize;
        yAxisSize_  = yAxisSize;
        token_      = token;
        op_         = op;
        tempVTopo_  = tempVTopo;
        return;
    }

    std::string Describe() const override
    {
        return StringFormat("CcuInstructionReduceMeshMem2Mem2D rankId [%u], instType[%d]", rankId_, instType_);
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
        return std::make_unique<CcuCtxArgReduceMeshMem2Mem2D>(dimSize_, rankId_, rootId_, axisId_, op_, tempVTopo_);
    }

    std::unique_ptr<CcuTaskArg> GetTaskArg() const override
    {
        HCCL_INFO("[ReduceAlgo]GetTaskArg begin");
        return std::make_unique<CcuTaskArgReduceMeshMem2Mem2D>(inputAddr_, outputAddr_, sliceSize_, xAxisSize_,
                                                               yAxisSize_, token_);
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
    CcuInstType                      instType_ = CcuInstType::CCU_REDUCE_MESH_2D_MEM2MEM;
    std::vector<uint64_t>            dimSize_;
    uint32_t                         rankId_{0};
    uint32_t                         rootId_{0};
    uint64_t                         axisId_{0};
    uint64_t                         inputAddr_{0};
    uint64_t                         outputAddr_{0};
    uint64_t                         sliceSize_{0};
    uint64_t                         xAxisSize_{0};
    uint64_t                         yAxisSize_{0};
    uint64_t                         token_{0};
    RankGroup                        rankGroup_;
    std::vector<LinkData>            links_;
    CollAlgOperator                  op_;
    std::vector<std::vector<RankId>> tempVTopo_;
};

} // namespace Hccl
#endif // HCCLV2_CCU_INSTRUCTION_REDUCE_MESH_2D_MEM2MEM_H_