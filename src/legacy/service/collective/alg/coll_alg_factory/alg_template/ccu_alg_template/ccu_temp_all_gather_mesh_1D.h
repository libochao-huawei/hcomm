/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllGatherMesh1D类头文件
 * Author: ningshuqi
 * Create: 2025-02-12
 */

#ifndef HCCLV2_CCU_TEMP_ALL_GATHER_MESH_1D_H_
#define HCCLV2_CCU_TEMP_ALL_GATHER_MESH_1D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_gather_mesh1d.h"

namespace Hccl {


class CcuTempAllGatherMesh1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllGatherMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllGatherMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of all gather ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    uint64_t GetMaxSliceSize() const;

    HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo,
        const std::vector<InsQuePtr> &tempInsQues) const;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_GATHER_MESH_1D_H_