/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_CCU_INSTRUCTION_REDUCE_SCATTER_MESH_2D_H_
#define HCCLV2_CCU_INSTRUCTION_REDUCE_SCATTER_MESH_2D_H_

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

// 为ReduceScatterMesh2D实现的CCUIns、CCUCtxArg与CCUTaskArg
class CcuCtxArgReduceScatterMesh2D : public CcuCtxArg {
public:
    explicit CcuCtxArgReduceScatterMesh2D(const std::vector<uint64_t> &dSize, uint32_t rId, uint32_t aId,
        const CollAlgOperator &op, const std::vector<std::vector<RankId>> &tempVTopo) :
            dimSize_(dSize), rankId_(rId), axisId_(aId), op_(op), tempVTopo_(tempVTopo) {}
    CcuCtxSignature GetCtxSignature() const override
    {
        CcuCtxSignature signature;
        GenerateCcuCtxSignature(signature, CcuInstType::CCU_REDUCE_SCATTER_MESH_2D_DIRECT, op_, tempVTopo_);
        return signature;
    }
    std::vector<uint64_t> dimSize_;
    uint32_t rankId_;
    uint32_t axisId_;
    CollAlgOperator op_;
    std::vector<std::vector<RankId>> tempVTopo_;
};

class CcuTaskArgReduceScatterMesh2D : public CcuTaskArg {
public:
    explicit CcuTaskArgReduceScatterMesh2D(uint64_t inputAddr, uint64_t outputAddr, uint64_t outputSize,
        uint64_t xAxisSize, uint64_t yAxisSize, uint64_t offSet, uint64_t token) :
        inputAddr_(inputAddr), outputAddr_(outputAddr), outputSize_(outputSize), xAxisSize_(xAxisSize), yAxisSize_(yAxisSize),
        offSet_(offSet), token_(token) {}
    uint64_t inputAddr_;
    uint64_t outputAddr_;
    uint64_t outputSize_;
    uint64_t xAxisSize_;
    uint64_t yAxisSize_;
    uint64_t offSet_;
    uint64_t token_;
};

class CcuInstructionReduceScatterMesh2D : public CcuInstruction {
public:
    CcuInstructionReduceScatterMesh2D() : CcuInstruction()
    {
    }

    void Init(std::vector<uint64_t> dimSize, uint32_t rankId, uint64_t inputAddr, uint64_t outputAddr, uint64_t axisId,
        uint64_t outputSize, uint64_t xAxisSize, uint64_t yAxisSize, uint64_t offSet, uint64_t token, CollAlgOperator &op,
        std::vector<std::vector<RankId>> &tempVTopo)
    {
        dimSize_ = dimSize;
        rankId_ = rankId;
        axisId_ = axisId;
        inputAddr_ = inputAddr;
        outputAddr_ = outputAddr;
        outputSize_ = outputSize;
        xAxisSize_ = xAxisSize;
        yAxisSize_ = yAxisSize;
        offSet_ = offSet;
        token_ = token;
        op_ = op;
        tempVTopo_ = tempVTopo;
        return;
    }

    std::string Describe() const override
    {
        return StringFormat("CcuInstructionReduceScatterMesh2D rankId [%u], instType[%s]", rankId_, instType_.Describe().c_str());
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
        return std::make_unique<CcuCtxArgReduceScatterMesh2D>(dimSize_, rankId_, axisId_, op_, tempVTopo_);
    }

    std::unique_ptr<CcuTaskArg> GetTaskArg() const override
    {
        return std::make_unique<CcuTaskArgReduceScatterMesh2D>(inputAddr_, outputAddr_, outputSize_, xAxisSize_,
            yAxisSize_, offSet_, token_);
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
    CcuInstType instType_ = CcuInstType::CCU_REDUCE_SCATTER_MESH_2D_DIRECT;
    std::vector<uint64_t> dimSize_;
    uint32_t rankId_{0};
    uint64_t axisId_{0};
    uint64_t inputAddr_{0};
    uint64_t outputAddr_{0};
    uint64_t outputSize_{0};
    uint64_t xAxisSize_{0};
    uint64_t yAxisSize_{0};
    uint64_t offSet_{0};
    uint64_t token_{0};
    RankGroup rankGroup_;
    std::vector<LinkData> links_;
    CollAlgOperator op_;
    std::vector<std::vector<RankId>> tempVTopo_;
};

}
#endif // HCCLV2_CCU_INSTRUCTION_REDUCE_SCATTER_MESH_2D_H_
