/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempAllReduceNHR类头文件
 * Author: wuhongge
 * Create: 2025-10-20
 */

#ifndef HCCLV2_INS_TEMP_ALL_REDUCE_NHR
#define HCCLV2_INS_TEMP_ALL_REDUCE_NHR

#include "string_util.h"
#include "ins_alg_template_base.h"
#include "executor_utils.h"
#include "ins_temp_all_gather_nhr.h"

namespace Hccl {

class InsTempAllReduceNHR : public InsAlgTemplateBase {
public:
    explicit InsTempAllReduceNHR(const RankId virtualRank, const u32 tempRankSize,
                                 const std::vector<std::vector<RankId>> &tempVTopo,
                                 const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempAllReduceNHR() override;

    std::string Describe() const override
    {
        return StringFormat("Template of reduce scatter NHR with tempRankSize [%u].", tempRankSize_);
    }

    u32 CalcScratchMultiple(BufferType input, BufferType output);
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
    const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
private:
    HcclResult PreCopy(const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult PrepareDataForAllGather(const RankSliceInfo &sliceInfoVec, std::vector<InsQuePtr> &tempInsQues);
    HcclResult PostCopy(const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunReduceScatter(const RankSliceInfo &sliceInfoVec,
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunAllGather(const RankSliceInfo &sliceInfoVec,
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult GetStepInfo(u32 step, u32 nSteps, AicpuNHRStepInfo &stepInfo);
    HcclResult GetStepInfoList(std::vector<AicpuNHRStepInfo> &stepInfoList);
    RankId GetRankFromMap(const u32 rankIdx);

    BufferType nhrInBuffType_ = BufferType::INPUT;
    BufferType nhrOutBuffType_ = BufferType::OUTPUT;
    u64 nhrInBuffBaseOff_ = 0;
    u64 nhrOutBuffBaseOff_ = 0;
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_ALL_REDUCE_NHR