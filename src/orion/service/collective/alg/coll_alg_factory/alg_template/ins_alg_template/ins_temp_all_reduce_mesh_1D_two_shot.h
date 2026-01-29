/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempAllReduceMesh1DTwoShot类头文件
 * Author: wanglichang
 * Create: 2025-06-05
 */

#ifndef HCCLV2_INS_TEMP_ALL_REDUCE_1D_MESH_TWO_SHOT
#define HCCLV2_INS_TEMP_ALL_REDUCE_1D_MESH_TWO_SHOT

#include "string_util.h"
#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempAllReduceMesh1DTwoShot : public InsAlgTemplateBase {
public:
    explicit InsTempAllReduceMesh1DTwoShot(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~InsTempAllReduceMesh1DTwoShot() override;

    std::string Describe() const override
    {
        return StringFormat("Template of all reduce 1D mesh with tempRankSize [%u].", tempRankSize_);
    }

    u32 CalcScratchMultiple(BufferType input, BufferType output) const;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
    const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

private:
    HcclResult RunAllReduceScatter(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    HcclResult RunAllReduceAllgather(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues, const TemplateDataParams &tempAlgParams);
    RankId GetRankFromMap(const u32 rankIdx);
};

}  // namespace Hccl

#endif  // !HCCLV2_INS_TEMP_ALL_REDUCE_1D_MESH_TWO_SHOT