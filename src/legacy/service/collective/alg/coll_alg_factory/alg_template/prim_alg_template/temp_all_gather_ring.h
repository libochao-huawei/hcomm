/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法模板TempAllGatherRing类头文件
 * Author: shenyutian
 * Create: 2024-02-04
 */

#ifndef HCCLV2_TEMP_ALL_GATHER_RING
#define HCCLV2_TEMP_ALL_GATHER_RING

#include "string_util.h"

#include "alg_template_base_v2.h"

namespace Hccl {

class TempAllGatherRing : public AlgTemplateBase {
public:
    explicit TempAllGatherRing(const RankId virtualRank, const u32 tempRankSize,
                               const std::vector<std::vector<RankId>> &tempVTopo,
                               const std::map<RankId, u32>            &tempVirtRankMap);
    ~TempAllGatherRing() override;

    std::string Describe() const override
    {
        return StringFormat("Template of all gather ring with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenPrimQue(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                          const ResLinks &tempLinks, std::vector<PrimQuePtr> &tempPrimQues) override;
    using AlgTemplateBase::CalcSliceInfo;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    using AlgTemplateBase::CalcRes;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

private:
    HcclResult PreCopyOffload(const RankSliceInfo &sliceInfoVec, const bool forAllReduce,
                              std::vector<PrimQuePtr> &tempPrimQues);
    HcclResult RunIndividualRing(const u32 queIdx, const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
                                 PrimQuePtr currPrimQue);

    u32 stepNum_ = 0;
};

} // namespace Hccl

#endif // !HCCLV2_TEMP_ALL_GATHER_RING