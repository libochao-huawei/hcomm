/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllToAllMesh1D的头文件
 * Author: chenchao
 * Create: 2025-02-19
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_TO_ALL_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_ALL_TO_ALL_MESH_1D_H_

#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_assist.h"

namespace Hccl {

class CcuContextAllToAllMesh1D : public CcuContext {
public:
    CcuContextAllToAllMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllToAllMesh1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable sliceSize_;
    CcuRep::Variable srcStride_;
    CcuRep::Variable srcOffset_;
    CcuRep::Variable dstOffset_;
    GroupOpSize groupOpSize_;
    bool loadFromMem_ = false;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_TO_ALL_MESH_1D_H_