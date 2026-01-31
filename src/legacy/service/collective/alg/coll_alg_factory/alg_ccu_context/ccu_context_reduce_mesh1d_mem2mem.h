/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceMeshMem2Mem1D的头文件
 * Author: jinliuyi
 * Create: 2025-12-4
 */
#ifndef HCCLV2_CCU_CONTEXT_REDUCE_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_MESH_1D_MEM2MEM_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_assist.h"

namespace Hccl {

class CcuContextReduceMeshMem2Mem1D : public CcuContext {
public:
    CcuContextReduceMeshMem2Mem1D(const CcuCtxArg &arg, const std::vector<CcuTransport *> &transports,
                                            const CcuTransportGroup &group);

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void InitResource();
    void LoadArgs();
    void PreSync();
    void DoRepeatReduce(const std::vector<CcuRep::Variable> &srcAddr, const CcuRep::Variable &dstAddr);
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
    CcuRep::Variable              inputRepeatStride_;
    CcuRep::Variable              outputRepeatStride_;
    CcuRep::Variable              normalSliceSize_;
    CcuRep::Variable              lastSliceSize_;
    CcuRep::Variable              repeatNumVar_;
    CcuRep::Variable              flag_;

    CcuRep::MaskSignal            locMask_;
    CcuRep::Memory                srcMem_;
    CcuRep::Memory                dstMem_;
    GroupOpSize                   localGoSize_;

    CcuRep::Variable              isInputOutputEqual_;
    uint16_t selfBit_{0};
    uint16_t allBit_{0};
    
    // variables for mesh_chunk
    std::vector<CcuRep::Variable> chunkSize_;
    CcuRep::Variable              chunkOffset_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_MESH_1D_MEM2MEM_H_