/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板AivTempScatterMesh1D类头文件
 * Author: caixin
 * Create: 2025-09-05
 */

#ifndef AIV_TEMP_ALL_SCATTER_MESH_1D
#define AIV_TEMP_ALL_SCATTER_MESH_1D

#include "string_util.h"
#include "executor_utils.h"

#include "aiv_alg_template_base.h"

namespace Hccl {

class AivTempScatterMesh1D : public AivAlgTemplateBase {
public:
    explicit AivTempScatterMesh1D(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~AivTempScatterMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Instruction based Template of scatter mesh 2D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, 
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
};

}  // namespace Hccl

#endif  // AIV_TEMP_ALL_SCATTER_MESH_1D