/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllReduceMesh2DTwoShot的头文件
 * Author: caichanghua
 * Create: 2025-02-19
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_2D_TWO_SHOT_H_
#define HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_2D_TWO_SHOT_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextAllReduceMesh2DTwoShot : public CcuContext {
public:
    CcuContextAllReduceMesh2DTwoShot(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllReduceMesh2DTwoShot() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    void InitVariables();
    void LoadArgs();
    void SyncAll(int ckeIdx);
    void PreSync();
    void GetSliceOffsetAndGoSize(uint64_t currentSliceRankIdx, uint64_t currStepSliceType, CcuRep::Variable &currOffset,
        GroupOpSize &currGoSize);
    void DoAxisSync(uint32_t signalIdx);
    void DoGroupSync(int ckeIdx, uint16_t selfBit, uint16_t allBit);
    void DoGroupReduce(std::vector<CcuRep::Variable> &srcBase, CcuRep::Variable &dstBase,
        CcuRep::Variable &offset, GroupOpSize &goSize);
    void DoGroupBroadcast(CcuRep::Variable &srcBase, std::vector<CcuRep::Variable> &dstBase, CcuRep::Variable &offset,
        GroupOpSize &goSize);

    std::vector<uint64_t> dimSize_;
    uint32_t axisId_{0};
    uint64_t rankId_{0};
    uint64_t rankSize_{0};
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;

    uint32_t otherAxisId_{0};
    std::vector<uint64_t> myRankIdxInAxis_;
    uint64_t myRankIdxInCurrentAxis_{0};
    uint64_t currentAxisRankSize_{0};
    uint64_t myRankIdxInOtherAxis_{0};
    uint64_t otherAxisRankSize_{0};

    uint16_t selfBit_{0};
    uint16_t allBit_{0};

    std::vector<CcuRep::Variable> inputAddr_;
    std::vector<CcuRep::Variable> outputAddr_;
    std::vector<CcuRep::Variable> token_;

    CcuRep::Variable normalRankXSliceSize_;
    CcuRep::Variable normalRankYSliceSize_;
    CcuRep::Variable lastRankXSliceSize_;
    CcuRep::Variable lastRankYSliceSize_;

    GroupOpSize normalRankXGoSize_;
    GroupOpSize normalRankYGoSize_;
    GroupOpSize lastRankXGoSize_;
    GroupOpSize lastRankYGoSize_;

    std::string currAxisSignalName_;
    std::string otherAxisSignalName_;
    CcuRep::MaskSignal currAxisSignal_;
    CcuRep::MaskSignal otherAxisSignal_;

    // 算法模板运行时的参数
    std::vector<CcuRep::Memory> tmpAddrList_;
    CcuRep::Memory tmpAddr_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_REDUCE_MESH_2D_TWO_SHOT_H_