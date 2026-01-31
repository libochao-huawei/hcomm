/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllGatherMesh2D类头文件
 * Author: huangweihao
 * Create: 2025-03-11
 */

#ifndef HCCLV2_CCU_TEMP_ALL_GATHER_MESH_2D_H_
#define HCCLV2_CCU_TEMP_ALL_GATHER_MESH_2D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_gather_mesh2d.h"

namespace Hccl {


class CcuTempAllGatherMesh2D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllGatherMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllGatherMesh2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of all gather ccu mesh 2D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
private:
    uint64_t DataSliceToAddr(const DataSlice &dataSlice);

    std::vector<uint32_t> dimSize_;
    std::vector<LinkData> linksX_;
    std::vector<LinkData> linksY_;
    uint64_t scratchBufferSize_{0};
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_GATHER_MESH_2D_H_