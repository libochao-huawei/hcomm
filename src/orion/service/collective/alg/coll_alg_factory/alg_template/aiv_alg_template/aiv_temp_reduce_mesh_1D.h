/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: 算法模板AivTempReduceMesh1D类头文件
 * Author: caixin
 * Create: 2025-09-17
 */
 
#ifndef AIV_TEMP_REDUCE_MESH_1D
#define AIV_TEMP_REDUCE_MESH_1D
 
#include "string_util.h"
#include "executor_utils.h"
 
#include "aiv_alg_template_base.h"
 
namespace Hccl {
 
class AivTempReduceMesh1D : public AivAlgTemplateBase {
public:
    explicit AivTempReduceMesh1D(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~AivTempReduceMesh1D() override;
 
    std::string Describe() const override
    {
        return StringFormat("Instruction based Template of reduce mesh 1D with tempRankSize [%u].", tempRankSize_);
    }
 
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, 
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
};
 
}  // namespace Hccl
 
#endif  // AIV_TEMP_REDUCE_MESH_1D