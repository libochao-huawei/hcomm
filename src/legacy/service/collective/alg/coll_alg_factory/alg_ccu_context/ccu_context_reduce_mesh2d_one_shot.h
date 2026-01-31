/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceMesh2DOneShot的头文件
 * Author: wanghaixu
 * Create: 2025-02-22
 */

#ifndef HCCLV2_CCU_CONTEXT_REDUCE_MESH_2D_ONE_SHOT_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_MESH_2D_ONE_SHOT_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextReduceMesh2DOneShot : public CcuContext {
public:
    CcuContextReduceMesh2DOneShot(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceMesh2DOneShot() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    uint64_t rankSize{0};
    uint32_t rankId;
    uint32_t rootId; // 当rankid == rootid时，为root节点 则跳过write操作
    DataType dataType;
    DataType outputDataType;
    ReduceOp reduceOp;
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable offSet_;
    CcuRep::Variable slicesize_;
    GroupOpSize groupOpSize_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_MESH_2D_ONE_SHOT_H_