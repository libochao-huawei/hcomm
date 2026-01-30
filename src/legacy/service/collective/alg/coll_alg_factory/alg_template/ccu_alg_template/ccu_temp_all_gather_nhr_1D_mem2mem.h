/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllGatherNHRMem2Mem1D类头文件
 * Author: ningshuqi
 * Create: 2025-07-12
 */
#ifndef HCCLV2_CCU_TEMP_ALL_GATHER_NHR_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_ALL_GATHER_NHR_1D_MEM2MEM_H_
#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_gather_nhr1d_mem2mem.h"
#include "executor_utils.h"


namespace Hccl {
class CcuTempAllGatherNHRMem2Mem1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllGatherNHRMem2Mem1D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllGatherNHRMem2Mem1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of AllGather ccu nhr 1D mem2mem with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, TemplateDataParams &tempAlgParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) override;
    uint32_t virtRankId2RankId(const u32 virtRankId);
    uint64_t GetMaxSliceSize() const;

private:
    HcclResult GetStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo);
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_GATHER_NHR_MESH_1D_MEM2MEM_H_