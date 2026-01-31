/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempBroadcastNHR类头文件
 * Author: linyefeng
 * Create: 2025-10-15
 */

#ifndef HCCLV2_INS_TEMP_BROADCAST_NHR_H
#define HCCLV2_INS_TEMP_BROADCAST_NHR_H

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"
#include "ins_temp_all_gather_nhr.h"

namespace Hccl {


class InsTempBroadcastNHR : public InsAlgTemplateBase {
public:
    explicit InsTempBroadcastNHR(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempBroadcastNHR() override;

    std::string Describe() const override
    {
        return StringFormat("Template of broadcase NHR with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType);

private:
    HcclResult PostCopy(const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues) const;
    HcclResult PreCopy(const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues) const;
    HcclResult RunScatter(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunAllGather(const RankSliceInfo &sliceInfoVec, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues);
    HcclResult GetScatterStepInfo(u32 step, u32 nSteps, AicpuNHRStepInfo &stepInfo) const;
    HcclResult GetAllGatherStepInfo(u32 step, u32 nSteps, AicpuNHRStepInfo &stepInfo);
    HcclResult BatchTxRx(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue,
        const RankSliceInfo &sliceInfoVec);
    HcclResult BatchSend(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue,
        const RankSliceInfo &sliceInfoVec, BufferType memType, u64 memOffset) const;
    HcclResult BatchRecv(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue,
        const RankSliceInfo &sliceInfoVec, BufferType memType, u64 memOffset) const;
    HcclResult BatchSR(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue,
        const RankSliceInfo &sliceInfoVec, BufferType memType, u64 memOffset) const;
    RankId GetRankFromMap(const u32 rankIdx) const;
    HcclResult CalcDataSliceInfo(const u64 dataSize, RankSliceInfo &sliceInfoVec);
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_BROADCAST_NHR_H