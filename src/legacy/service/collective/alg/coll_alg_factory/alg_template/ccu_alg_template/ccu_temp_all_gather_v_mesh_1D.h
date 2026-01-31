/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllGatherVMesh1D类头文件
 * Author: ningshuqi
 * Create: 2025-09-12
 */

#ifndef HCCLV2_CCU_TEMP_ALL_GATHER_V_MESH_1D_H_
#define HCCLV2_CCU_TEMP_ALL_GATHER_V_MESH_1D_H_

#include "string_util.h"
#include "env_config.h"
#include "executor_utils.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_gather_v_mesh1d.h"

namespace Hccl {

class CcuTempAllGatherVMesh1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllGatherVMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                         const std::vector<std::vector<RankId>> &tempVTopo,
                                         const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllGatherVMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of all gather V ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_GATHER_V_MESH_1D_H_