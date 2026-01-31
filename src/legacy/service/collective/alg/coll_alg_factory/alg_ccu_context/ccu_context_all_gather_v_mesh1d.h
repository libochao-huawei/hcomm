/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllGatherVMesh1D的头文件
 * Author: ningshuqi
 * Create: 2025-09-19
 */

#ifndef HCCLV2_CCU_CONTEXT_ALL_GATHER_V_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_ALL_GATHER_V_MESH_1D_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextAllGatherVMesh1D : public CcuContext {
public:
    CcuContextAllGatherVMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllGatherVMesh1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
    void LoadArgs();
    void InitResources();
    void PreSync();
    void PostSync();
    void DoGroupBroadcast();
private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    CcuRep::Variable input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable mySliceOffSet_;
    GroupOpSize groupOpSize_;
};
}// namespace Hccl
#endif // HCCLV2_CCU_CONTEXT_ALL_GATHER_V_MESH_1D_H_