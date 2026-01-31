/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceScatterVMeshMem2Mem1D的头文件
 * Author: caichanghua
 * Create: 2025-10-10
 */


#ifndef HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_V_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_V_MESH_1D_MEM2MEM_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextReduceScatterVMeshMem2Mem1D : public CcuContext {
public:
    CcuContextReduceScatterVMeshMem2Mem1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceScatterVMeshMem2Mem1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

protected:
    void CollectAllRanksSlice(std::vector<CcuRep::Memory>& tmpSrc,
    std::vector<CcuRep::Memory>& tmpDst, const CcuRep::MaskSignal &locMask);
    void InitResources();
    void PrepareReduceScatterVData(std::vector<CcuRep::Memory>& reduceScatterVSrc,
        std::vector<CcuRep::Memory>& reduceScatterVDst);

private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;
    std::vector<CcuRep::Variable> input_;
    CcuRep::Variable output_;
    std::vector<CcuRep::Variable> scratch_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable sliceSize_;
    CcuRep::Variable scratchInterval_;
    CcuRep::Variable offset_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_V_MESH_1D_MEM2MEM_H_