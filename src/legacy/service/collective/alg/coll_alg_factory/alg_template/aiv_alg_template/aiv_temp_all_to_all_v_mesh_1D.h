/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板AivTempAlltoAllVMesh1D类头文件
 * Author: wuhongge
 * Create: 2025-12-15
 */

#ifndef AIV_TEMP_ALL_TO_ALL_V_MESH_1D
#define AIV_TEMP_ALL_TO_ALL_V_MESH_1D

#include "string_util.h"
#include "executor_utils.h"

#include "aiv_alg_template_base.h"

namespace Hccl {

class AivTempAlltoAllVMesh1D : public AivAlgTemplateBase {
public:
    explicit AivTempAlltoAllVMesh1D(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~AivTempAlltoAllVMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Instruction based Template of alltoallv mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
};

}  // namespace Hccl

#endif  // AIV_TEMP_ALL_TO_ALL_MESH_1D