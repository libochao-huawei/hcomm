/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempScatterMesh1D类头文件
 * Author: wanghaixu
 * Create: 2025-10-14
 */

#ifndef HCCLV2_CCU_TEMP_SCATTER_MESH_1D_H_
#define HCCLV2_CCU_TEMP_SCATTER_MESH_1D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class CcuTempScatterMesh1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempScatterMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempScatterMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of scatter ccu mesh 1D, tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t GetMaxSliceSize() const;
    uint64_t GetExpandedMode() const;
    HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo,
                         const std::vector<InsQuePtr> &tempInsQues) const;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_SCATTER_MESH_1D_H_