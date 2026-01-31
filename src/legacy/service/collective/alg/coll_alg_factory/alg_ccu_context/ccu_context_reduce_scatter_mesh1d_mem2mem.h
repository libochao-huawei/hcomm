/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceScatterMeshMem2Mem1D的头文件
 * Author: cuishuang
 * Create: 2025-10-16
 */

#ifndef HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_1D_MEM2MEM_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {
class CcuContextReduceScatterMeshMem2Mem1D : public CcuContext {
public:
    CcuContextReduceScatterMeshMem2Mem1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceScatterMeshMem2Mem1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void InitResource();
    void LoadArgs();
    void PreSync();
    void PostSync();
    void DoRepeatReduceScatter();
    void DoReduceScatter();

    std::string GetLoopBlockTag(std::string loopType, int32_t index);
    void CreateReduceLoop(uint32_t size, DataType dataType, DataType outputDataType, ReduceOp opType);
    void ReduceLoopGroup(CcuRep::Memory outDstOrg, CcuRep::Memory srcOrg, std::vector<CcuRep::Memory> &scratchOrg,
        GroupOpSize goSize, DataType dataType, DataType outputDataType, ReduceOp opType);

    const std::string LOOP_BLOCK_TAG{"_local_copy_reduce_loop_"};

    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    DataType dataType_;
    DataType outputDataType_;
    CcuRep::Variable repeatNum_;
    ReduceOp reduceOp_;
    std::vector<CcuRep::Variable> input_;
    CcuRep::Variable output_;
    std::vector<CcuRep::Variable> scratch_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable              currentRankSliceInputOffset_;
    CcuRep::Variable              inputRepeatStride_;
    CcuRep::Variable              outputRepeatStride_;
    CcuRep::Variable              normalSliceSize_;
    GroupOpSize normalGoSize_;
    uint16_t selfBit_{0};
    uint16_t allBit_{0};
    std::vector<CcuRep::Memory>   localMem_;
    std::vector<CcuRep::Memory>   remoteMem_;
    CcuRep::MaskSignal localSignal_;
    CcuRep::Variable flag_; // 用以判断是否是第一次重复
};
}// namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_1D_MEM2MEM_H_
