/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceScatterVMesh1D的头文件
 * Author: caichanghua
 * Create: 2025-02-19
 */


#ifndef HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_V_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_V_MESH_1D_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextReduceScatterVMesh1D : public CcuContext {
public:
    CcuContextReduceScatterVMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceScatterVMesh1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
    void LoadArgs();
    void InitResources();
    void PreSync();
    void PostSync();
    void DoGroupReduce();
private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    DataType dataType;
    DataType outputDataType;
    ReduceOp reduceOp;
    std::vector<CcuRep::Variable> input_;
    CcuRep::Variable output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable mySliceOffSet_;
    GroupOpSize groupOpSize_;

    std::vector<CcuRep::Memory> nMemory_;
    CcuRep::Memory oneMemory_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_V_MESH_1D_H_