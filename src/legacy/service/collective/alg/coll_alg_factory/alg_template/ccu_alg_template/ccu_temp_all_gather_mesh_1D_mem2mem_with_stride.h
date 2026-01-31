/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllGatherMesh1DMem2MemWithStride类头文件
 * Author: ningshuqi
 * Create: 2025-02-12
 */

#ifndef HCCLV2_CCU_TEMP_ALL_GATHER_MESH_1D_MEM2MEM_WITH_STRIDE_H_
#define HCCLV2_CCU_TEMP_ALL_GATHER_MESH_1D_MEM2MEM_WITH_STRIDE_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class CcuTempAllGatherMesh1DMem2MemWithStride : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllGatherMesh1DMem2MemWithStride(const RankId virtualRank, const u32 tempRankSize,
                                                     const std::vector<std::vector<RankId>> &tempVTopo,
                                                     const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllGatherMesh1DMem2MemWithStride() override;

    std::string Describe() const override
    {
        return StringFormat("Template of all gather ccu mesh 1D Mem2Mem with stride, tempRankSize [%u].",
                            tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                   std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t   GetMaxSliceSize() const;

    HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo,
                         const std::vector<InsQuePtr> &tempInsQues) const;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_GATHER_MESH_1D_MEM2MEM_WITH_STRIDE_H_