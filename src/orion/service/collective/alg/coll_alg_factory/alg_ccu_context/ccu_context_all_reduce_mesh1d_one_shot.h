/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllReduceMesh1D的头文件
 * Author: ningshuqi
 * Create: 2025-02-19
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_1D_ONE_SHOT_H_
#define HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_1D_ONE_SHOT_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextAllReduceMesh1DOneShot : public CcuContext {
public:
    CcuContextAllReduceMesh1DOneShot(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllReduceMesh1DOneShot() override{}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void InitResource();
    void ImportReduceVariables();
    void LoadArgs();
    void Presync();
    void Postsync();
    void SyncTailBlock(uint32_t ctxSignalIndex);
    void DoGroupReduce();
    void CalcMissionOffset(uint64_t sliceSize, uint64_t missionId, uint64_t &missionSize,
                           uint64_t &missionOffset) const;

    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;

    std::string notifySignal_ = "empty_signal";

    std::vector<CcuRep::Variable> input_;
    CcuRep::Variable output_;
    std::vector<CcuRep::Variable> token_;
    GroupOpSize groupOpSize_;

    CcuRep::Variable tailBlockOffSet_;
    GroupOpSize tailBlockGroupOpSize_;

    CcuRep::Variable reduceInput_;
    CcuRep::Variable reduceOutput_;
    CcuRep::Variable reducetoken_;
    CcuRep::Variable reduceInputOffSet_;
    CcuRep::Variable reduceOutputOffSet_;
    GroupOpSize reduceGroupOpSize_;

    CcuRep::MaskSignal mainBlockCtxSignal_;
    CcuRep::MaskSignal tailBlockCtxSignal_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_1D_H_