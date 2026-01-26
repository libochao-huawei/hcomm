/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllReduceMesh2DOneShot的头文件
 * Author: caichanghua
 * Create: 2025-02-19
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_2D_ONE_SHOT_H_
#define HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_2D_ONE_SHOT_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextAllReduceMesh2DOneShot : public CcuContext {
public:
    CcuContextAllReduceMesh2DOneShot(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllReduceMesh2DOneShot() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    void InitVariables();
    void LoadArgs();
    void DoAxisSync(uint32_t signalIdx);
    void DoGroupSync(int ckeIdx, uint16_t selfBit, uint16_t allBit);
    void DoGroupReduce(std::vector<CcuRep::Variable> &srcBase, CcuRep::Variable &dstBase,
        CcuRep::Variable &offset, GroupOpSize &goSize);
    std::vector<uint64_t> dimSize_;
    std::vector<uint64_t> myRankIdxInAxis_;
    uint64_t myRankIdxInCurrentAxis_{0};
    uint64_t currentAxisRankSize_{0};

    uint32_t axisId_{0};
    uint64_t rankId_{0};
    uint64_t rankSize_{0};
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;

    std::vector<CcuRep::Variable> inputAddr_;
    std::vector<CcuRep::Variable> outputAddr_;
    std::vector<CcuRep::Variable> scratchAddr_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable xSliceOffset_;
    CcuRep::Variable ySliceOffset_;
    GroupOpSize xGoSize_;
    GroupOpSize yGoSize_;

    std::string currAxisSignalName_;
    std::string otherAxisSignalName_;
    CcuRep::MaskSignal currAxisSignal_;
    CcuRep::MaskSignal otherAxisSignal_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_2D_ONE_SHOT_H_