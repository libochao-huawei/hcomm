/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempReduceScatterVMeshMem2Mem1D类头文件
 * Author: caichanghua
 * Create: 2025-02-12
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_SCATTER_V_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_REDUCE_SCATTER_V_MESH_1D_MEM2MEM_H_

#include "string_util.h"
#include "env_config.h"
#include "executor_utils.h"
#include "ccu_alg_template_base.h"

namespace Hccl {

class CcuTempReduceScatterVMeshMem2Mem1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempReduceScatterVMeshMem2Mem1D(const RankId virtualRank, const u32 tempRankSize,
                                         const std::vector<std::vector<RankId>> &tempVTopo,
                                         const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempReduceScatterVMeshMem2Mem1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Reduce Scatter V Mem2Mem ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    u32 CalcScratchMultiple(BufferType input, BufferType output) override;

private:
    ReduceOp reduceOp_;
    DataType dataType_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_REDUCE_SCATTER_V_MESH_1D_MEM2MEM_H_