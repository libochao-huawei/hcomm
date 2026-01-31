/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempAllReduceMesh1DOneShot类头文件
 * Author: wuhongge
 * Create: 2025-06-03
 */

#ifndef HCCLV2_INS_TEMP_ALL_REDUCE_MESH_1D_ONE_SHOT
#define HCCLV2_INS_TEMP_ALL_REDUCE_MESH_1D_ONE_SHOT

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempAllReduceMesh1DOneShot : public InsAlgTemplateBase {
public:
    explicit InsTempAllReduceMesh1DOneShot(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~InsTempAllReduceMesh1DOneShot() override;

    std::string Describe() const override
    {
        return StringFormat("Template of all reduce Mesh oneshot with tempRankSize [%u].", tempRankSize_);
    }

    u32 CalcScratchMultiple(BufferType input, BufferType output) const;
    HcclResult CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec);
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult RunAllReduce(const TemplateDataParams &tempAlgParams, const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues);
private:
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_ALL_REDUCE_MESH_1D_ONE_SHOT