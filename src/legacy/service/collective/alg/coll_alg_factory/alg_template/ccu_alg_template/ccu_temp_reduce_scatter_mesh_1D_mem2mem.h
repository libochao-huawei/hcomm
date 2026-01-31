/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempReduceScatterMesh1DMem2Mem类实现
 * Author: cuishuang
 * Create: 2025-10-16
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_MEM2MEM_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class CcuTempReduceScatterMeshMem2Mem1D : public CcuAlgTemplateBase {
public:
    explicit  CcuTempReduceScatterMeshMem2Mem1D(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo,
        const std::map<RankId, u32>            &tempVirtRankMap);

    ~CcuTempReduceScatterMeshMem2Mem1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Reduce Scatter ccu mesh 1D Mem2Mem with tempRankSize [%u].",
                            tempRankSize_);
    }

void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

HcclResult CalcRes(AlgTempResReq &tempResReq) override;
u32 CalcScratchMultiple(BufferType input, BufferType output) override;
HcclResult GenExtIns(const TempFuncs  &tempFuncs,const TemplateDataParams &templateDataParams,
                    const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo,
        const std::vector<InsQuePtr> &tempInsQues) const;
private:
    ReduceOp reduceOp_;
    DataType dataType_;
};

}// namespace Hccl

#endif// HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_MEM2MEM_H_