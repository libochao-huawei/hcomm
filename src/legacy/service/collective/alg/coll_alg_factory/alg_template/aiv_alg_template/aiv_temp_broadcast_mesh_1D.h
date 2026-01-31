/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板AivTempBroadcastMesh1D类头文件
 * Author: zhangzuyu
 * Create: 2025-09-15
 */
 
#ifndef AIV_TEMP_ALL_BROADCAST_MESH_1D
#define AIV_TEMP_ALL_BROADCAST_MESH_1D
 
#include "string_util.h"
#include "executor_utils.h"
 
#include "aiv_alg_template_base.h"
 
namespace Hccl {
 
class AivTempBroadcastMesh1D : public AivAlgTemplateBase {
public:
    explicit AivTempBroadcastMesh1D(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~AivTempBroadcastMesh1D() override;
 
    std::string Describe() const override
    {
        return StringFormat("Instruction based Template of aiv broadcast mesh 1D with tempRankSize [%u].", tempRankSize_);
    }
 
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, 
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
};
 
}  // namespace Hccl
 
#endif  // AIV_TEMP_BROADCAST_MESH_1D