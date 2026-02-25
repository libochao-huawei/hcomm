/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description:  CcuContextAllGatherMesh1D2Die的头文件
 * Author: zhangxin
 * Create: 2026-01-10
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_2DIE_H_
#define HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_2DIE_H_

#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextAllGatherMesh1D2Die : public CcuContext {
public:
    CcuContextAllGatherMesh1D2Die(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllGatherMesh1D2Die() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    bool withMyRank_ = true;  // 发数据是否包含本rank
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    CcuRep::Variable offSet_;
    std::vector<CcuRep::Variable> token_;
    GroupOpSize groupOpSize_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_1D_H_