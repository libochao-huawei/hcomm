/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceMesh1D的头文件
 * Author: jinliuyi
 * Create: 2025-08-11
 */

#ifndef HCCLV2_CCU_CONTEXT_REDUCE_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_MESH_1D_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextReduceMesh1D : public CcuContext {
public:
    CcuContextReduceMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void InitResource();
    void LoadArgs();
    void PreSync();
    void DoRepeatReduce();
    void PostSync();
    
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    uint32_t rootId_{0}; // 当rankid == rootid时，为root节点 则跳过write操作
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable              currentRankSliceInputOffset_;
    CcuRep::Variable              currentRankSliceOutputOffset_;
    CcuRep::Variable              repeatNum_;
    CcuRep::Variable              inputRepeatStride_;
    CcuRep::Variable              outputRepeatStride_;
    CcuRep::Variable              normalSliceSize_;
    CcuRep::Variable              lastSliceSize_;
    CcuRep::Variable              repeatNumVar_;
    CcuRep::Variable              flag_;

    GroupOpSize groupOpSize_;

    uint16_t selfBit_{0};
    uint16_t allBit_{0};

    CcuRep::Variable srcOffset_;
    CcuRep::Variable dstOffset_;

    CcuRep::Memory              localMem_;
    std::vector<CcuRep::Memory> reomteMem_;

    CcuRep::MaskSignal localSignal_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_MESH_1D_H_