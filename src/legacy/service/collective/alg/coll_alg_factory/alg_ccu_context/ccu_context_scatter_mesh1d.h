/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextScatterMesh1D的头文件
 * Author: wanghaixu
 * Create: 2025-10-15
 */


#ifndef HCCLV2_CCU_CONTEXT_SCATTER_MESH_1D_H_
#define HCCLV2_CCU_CONTEXT_SCATTER_MESH_1D_H_

#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextScatterMesh1D : public CcuContext {
public:
    CcuContextScatterMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void InitResource();
    void LoadArgs();
    void PreSync();
    void PostSync();
    void RunSendScatter();

    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    uint32_t rootId_{0};
    uint16_t selfBit_{0};
    uint16_t allBit_{0};

    CcuRep::Variable              input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable              currentRankSliceInputOffset_;
    CcuRep::Variable              currentRankSliceOutputOffset_;
    CcuRep::Variable              inputRepeatStride_;
    CcuRep::Variable              outputRepeatStride_;
    CcuRep::Variable              normalSliceSize_;
    CcuRep::Variable              lastSliceSize_;
    CcuRep::Variable              repeatNumVar_;
    CcuRep::Variable              flag_;
    std::vector<CcuRep::Memory>   localMem_;
    std::vector<CcuRep::Memory>   remoteMem_;

    CcuRep::MaskSignal localSignal_;
    GroupOpSize        groupOpSize_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_SCATTER_MESH_1D_H_