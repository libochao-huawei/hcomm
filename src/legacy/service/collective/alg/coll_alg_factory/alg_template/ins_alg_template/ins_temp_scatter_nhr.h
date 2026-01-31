/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempScatterNHR类头文件
 * Author: chenchao
 * Create: 2025-02-12
 */

#ifndef HCCLV2_INS_TEMP_SCATTER_NHR
#define HCCLV2_INS_TEMP_SCATTER_NHR

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "ins_temp_all_gather_nhr.h"
namespace Hccl {


class InsTempScatterNHR : public InsAlgTemplateBase {
public:
    explicit InsTempScatterNHR(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempScatterNHR() override;

    std::string Describe() const override
    {
        return StringFormat("Template of scatter NHR with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GetStepInfo(u32 step, u32 nSteps, AicpuNHRStepInfo &stepInfo) const;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t GetExpandedMode() const;
    HcclResult GenExtIns(TempFuncs &tempFuncs, TemplateDataParams &templateDataParams,
                         ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const;
private:
    HcclResult PreCopy(TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult PostCopy(TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunNHR(TemplateDataParams &templateDataParams, ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) const;
    HcclResult BatchSend(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue, TemplateDataParams &templateDataParams, u32 repeat) const;
    HcclResult BatchRecv(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue, TemplateDataParams &templateDataParams, u32 repeat) const;
    HcclResult BatchSR(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue, TemplateDataParams &templateDataParams, u32 repeat) const;
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_REDUCE_SCATTER_NHR