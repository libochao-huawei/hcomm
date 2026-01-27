/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CCcuContextAllGatherMeshMem2Mem1D的头文件
 * Author: hulinkang
 * Create: 2025-05-12
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_MEM2MEM_H_

#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_assist.h"

namespace Hccl {

class CcuContextAllGatherMeshMem2Mem1D : public CcuContext {
public:
    CcuContextAllGatherMeshMem2Mem1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllGatherMeshMem2Mem1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable offSet_;
    CcuRep::Variable sliceSize_;
    GroupOpSize localGoSize_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_MEM2MEM_H_