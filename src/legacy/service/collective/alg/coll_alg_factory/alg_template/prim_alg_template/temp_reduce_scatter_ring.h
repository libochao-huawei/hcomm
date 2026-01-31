/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法模板TempReduceScatterRing类头文件
 * Author: shenyutian
 * Create: 2024-02-04
 */

#ifndef HCCLV2_TEMP_REDUCE_SCATTER_RING
#define HCCLV2_TEMP_REDUCE_SCATTER_RING
#include <vector>
#include <map>
#include <string>
#include <hccl/hccl_types.h>

#include "hccl/base.h"
#include "types/types.h"
#include "string_util.h"
#include "template_utils.h"
#include "alg_template_base_v2.h"

namespace Hccl {

class TempReduceScatterRing : public AlgTemplateBase {
public:
    explicit TempReduceScatterRing(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~TempReduceScatterRing() override;

    std::string Describe() const override
    {
        return StringFormat("Template of reduce scatter ring with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenPrimQue(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                          const ResLinks &tempLinks, std::vector<PrimQuePtr> &tempPrimQues) override;

    using AlgTemplateBase::CalcSliceInfo;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const bool forAllReduce, const u64 dataSize,
                             RankSliceInfo &sliceInfoVec) override;

    using AlgTemplateBase::CalcRes;
    HcclResult CalcRes(const bool forAllReduce, AlgTempResReq &tempResReq, u32 &requiredScratchMultiplier) override;

private:
    HcclResult CalcSliceInfoAllReduce(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec);

    HcclResult RunIndividualRing(const u32 queIdx, const bool &forAllReduce, const RankSliceInfo &sliceInfoVec,
                                 const ResLinks &tempLinks, PrimQuePtr currPrimQue);

    HcclResult PostCopyOffload(const RankSliceInfo &sliceInfoVec, std::vector<PrimQuePtr> &tempPrimQues);

    u32 stepNum_ = 0;
};

} // namespace Hccl

#endif // !HCCLV2_TEMP_REDUCE_SCATTER_RING