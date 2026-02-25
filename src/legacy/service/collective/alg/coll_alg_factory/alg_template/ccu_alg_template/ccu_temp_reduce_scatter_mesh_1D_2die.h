/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: 算法模板CcuTempReduceScatterMesh1D2Die类头文件
 * Author: caichanghua
 * Create: 2026-01-10
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_2DIE_H_
#define HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_2DIE_H_
#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_reduce_scatter_mesh1d_2die.h"
#include "executor_utils.h"

namespace Hccl {
class CcuTempReduceScatterMesh1D2Die : public CcuAlgTemplateBase {
public:
    explicit CcuTempReduceScatterMesh1D2Die(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempReduceScatterMesh1D2Die() override;

    std::string Describe() const override
    {
        return StringFormat("Template of ReduceScatter ccu nhr 1D mem2mem with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, TemplateDataParams &tempAlgParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t GetMaxSliceSize() const;
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) override;

private:
    ReduceOp reduceOp_;
    DataType dataType_;
    bool isSipportTwoDie_{false};
    u32 linkNum_{1};
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_1D_2DIE_H_