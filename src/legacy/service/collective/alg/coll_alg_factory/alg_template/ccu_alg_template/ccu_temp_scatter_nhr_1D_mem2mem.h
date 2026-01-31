/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempScatterNHRMem2Mem1D类头文件
 * Author: hanyan
 * Create: 2025-11-11
 */
#ifndef HCCLV2_CCU_TEMP_SCATTER_NHR_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_SCATTER_NHR_1D_MEM2MEM_H_
#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_scatter_nhr1d_mem2mem.h"
#include "executor_utils.h"

namespace Hccl {
class CcuTempScatterNHRMem2Mem1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempScatterNHRMem2Mem1D(const RankId virtualRank, const u32 tempRankSize,
                                        const std::vector<std::vector<RankId>> &tempVTopo,
                                        const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempScatterNHRMem2Mem1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Scatter ccu nhr 1D mem2mem with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, TemplateDataParams &tempAlgParams, const ResLinks &tempLinks,
                         std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t   GetMaxSliceSize() const;
    uint32_t   virtRankId2RankId(const uint32_t virtRankId);
    u32        CalcScratchMultiple(BufferType input, BufferType output) override;
    HcclResult GetScatterStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo);
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_SCATTER_MESH_1D_MEM2MEM_H_