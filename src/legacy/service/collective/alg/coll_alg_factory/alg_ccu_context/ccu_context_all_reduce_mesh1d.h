/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllReduceMesh1D的头文件
 * Author: ningshuqi
 * Create: 2025-02-19
 */

#ifndef HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_1D_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_device_manager.h"
#include "ccu_context_alg_base.h"

namespace Hccl {

class CcuContextAllReduceMesh1D : public CcuContextAlgBase {
public:
    CcuContextAllReduceMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllReduceMesh1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    void RunBroadcast(std::vector<CcuRep::Memory> &dst, CcuRep::Memory &src);
    void RunReduce(CcuRep::Memory &dst, std::vector<CcuRep::Memory> &src);

    CcuVersion ccuVersion_ = CcuVersion::CCU_INVALID;
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable offSet_;
    GroupOpSize groupOpSize_;
    GroupOpSizeV2 groupOpSizeV2_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_1D_H_