/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempBroadcastMeshMem2Mem2D类头文件
 * Author: lijiangquan
 * Create: 2025-09-10
 */

#ifndef HCCLV2_CCU_TEMP_BROADCAST_MESH_2D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_BROADCAST_MESH_2D_MEM2MEM_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "executor_utils.h"
namespace Hccl {

class CcuTempBroadcastMeshMem2Mem2D : public CcuAlgTemplateBase {
public:
    explicit CcuTempBroadcastMeshMem2Mem2D(const RankId virtualRank, const u32 tempRankSize,
                                           const std::vector<std::vector<RankId>> &tempVTopo,
                                           const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempBroadcastMeshMem2Mem2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Broadcast ccu mesh 2D with tempRankSize [%u].", tempRankSize_);
    }
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult PrepareLinks(const ResLinks &tempLinks);

private:
    std::vector<uint64_t> dimSize_;
    std::vector<LinkData> linksX_;
    std::vector<LinkData> linksY_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_BROADCAST_MESH_2D_MEM2MEM_H_