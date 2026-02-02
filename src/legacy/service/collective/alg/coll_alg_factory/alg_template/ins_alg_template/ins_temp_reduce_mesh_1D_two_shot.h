/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceMesh1DTwoShot类头文件
 * Author: l00929943
 * Create: 2025-06-05
 */

#ifndef HCCLV2_INS_TEMP_REDUCE_1D_MESH_TWO_SHOT
#define HCCLV2_INS_TEMP_REDUCE_1D_MESH_TWO_SHOT

#include "string_util.h"
#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempReduceMesh1DTwoShot : public InsAlgTemplateBase {
public:
    explicit InsTempReduceMesh1DTwoShot(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~InsTempReduceMesh1DTwoShot() override;

    std::string Describe() const override
    {
        return StringFormat("Template of reduce 1D mesh two shot with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &dataParams,
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);

private:
    HcclResult CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec);
    HcclResult RunReduceScatter(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    HcclResult RunGatherToRoot(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    RankId GetRankFromMap(const u32 rankIdx);

    u32 myIdx_ = INVALID_U32;
};

}  // namespace Hccl

#endif  // HCCLV2_INS_TEMP_REDUCE_1D_MESH_TWO_SHOT