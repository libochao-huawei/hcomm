/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceScatterMesh1D2Die的头文件
 * Author: ningshuqi
 * Create: 2025-02-19
 */


#ifndef HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_1D_2Die_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_1D_2Die_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextReduceScatterMesh1D2Die : public CcuContext {
public:
    CcuContextReduceScatterMesh1D2Die(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceScatterMesh1D2Die() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    void InitResources();
    void LoadArgs();
    void PreSync();
    void PostSync(uint32_t signalIndex);
    void MissionSync(uint32_t signalIndex);
    void RmtReduce();
    std::string GetLoopBlockTag(std::string loopType, int32_t index) const;
    void CreateReduceLoop(uint32_t size, DataType dataType, DataType outputDataType,
        ReduceOp opType);
    void ReduceLoopGroup(CcuRep::Memory &outDstOrg, std::vector<CcuRep::Memory> &srcOrg,
        GroupOpSize goSize, DataType dataType, DataType outputDataType,
        ReduceOp opType);
    void DoLocalReduce();

private:
    bool rmtReduceWithMyRank_{true};

    uint32_t myRankId_{0};
    uint32_t rankSize_{0};

    uint32_t rmtReduceRankNum_{0};
    uint32_t rmtSyncMyBit_{0};
    uint32_t rmtSyncWaitBit_{0};

    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;

    std::string ctxName_ = "";;

    uint32_t missionSyncMybit_{0};
    uint32_t missionSyncWaitBit_{0};

    std::string myMissionSignalName_ = "";
    std::string otherMissionSignalName_ = "";

    CcuRep::Variable myInput_;
    CcuRep::Variable myOutput_;
    CcuRep::Variable myScratch_;
    CcuRep::Variable myToken_;
    std::vector<CcuRep::Variable> peerInput_;
    std::vector<CcuRep::Variable> peerToken_;

    CcuRep::Variable sliceSize_;

    CcuRep::Variable rmtReduceSliceOffset_;

    GroupOpSize rmtReduceGoSize_;

    CcuRep::MaskSignal myMissionSignal_;
    CcuRep::MaskSignal otherMissionSignal_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_1D_2Die_H_