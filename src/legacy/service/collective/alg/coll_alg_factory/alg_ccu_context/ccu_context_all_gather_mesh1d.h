/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllGatherMesh1D的头文件
 * Author: ningshuqi
 * Create: 2025-02-19
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_H_

#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextAllGatherMesh1D : public CcuContext {
public:
    CcuContextAllGatherMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllGatherMesh1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable offSet_;
    GroupOpSize groupOpSize_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_H_