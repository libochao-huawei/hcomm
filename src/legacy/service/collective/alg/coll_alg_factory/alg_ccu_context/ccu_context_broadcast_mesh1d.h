/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextBroadcastMesh1D的头文件
 * Author: libiaozhi
 * Create: 2025-03-21
 */

#ifndef HCCLV2_CCU_CONTEXT_BROADCAST_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_BROADCAST_MESH_1D_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextBroadcastMesh1D : public CcuContext {
public:
    CcuContextBroadcastMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextBroadcastMesh1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void CreateAllVariables();
    void LoadAndExchangeData();
    void BroadcastFromRootToAll();

    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    uint32_t rootId_{0}; // 当rankid == rootid时，为root节点 则跳过write操作
    DataType dataType_;
    DataType outputDataType_;
    CcuRep::Variable input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable offSet_;
    CcuRep::Variable slicesize_;
    GroupOpSize groupOpSize_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_BROADCAST_MESH_1D_H_