/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempReduceScatterMesh1DMultiMission类头文件
 * Author: fuyuqi
 * Create: 2025-05-22
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_MULTI_MISSION_H_
#define HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_MULTI_MISSION_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_reduce_scatter_mesh1d_multiMission.h"

namespace Hccl {


class CcuTempReduceScatterMesh1DMultiMission : public CcuAlgTemplateBase {
public:
    explicit CcuTempReduceScatterMesh1DMultiMission(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempReduceScatterMesh1DMultiMission() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Reduce Scatter ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    // init reduceInfo
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);
    uint64_t GetMaxSliceSize() const;
    HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo,
        const std::vector<InsQuePtr> &tempInsQues) const;

private:
    ReduceOp reduceOp_;
    DataType dataType_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_MULTI_MISSION_H_