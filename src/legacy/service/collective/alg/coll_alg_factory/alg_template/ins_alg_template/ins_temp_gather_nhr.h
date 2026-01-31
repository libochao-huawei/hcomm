/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempGatherNHR类头文件
 * Author: caichanghua
 * Create: 2025-02-12
 */

#ifndef INS_TEMP_GATHER_NHR
#define INS_TEMP_GATHER_NHR

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "ins_temp_all_gather_nhr.h"
namespace Hccl {

class InsTempGatherNHR : public InsAlgTemplateBase {
public:
    explicit InsTempGatherNHR(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempGatherNHR() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Gather NHR with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GetScatterStepInfo(u32 step, u32 nSteps, AicpuNHRStepInfo &stepInfo) const;
    uint64_t GetExpandedMode() const;
private:
    HcclResult PreCopy(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec,
        std::vector<InsQuePtr> &tempInsQues);
    HcclResult PostCopy(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec,
        std::vector<InsQuePtr> &tempInsQues);

    HcclResult BatchTxRx(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue, const RankSliceInfo &sliceInfoVec);
    HcclResult BatchSend(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue, const RankSliceInfo &sliceInfoVec, BufferType memType, u32 memOffset) const;
    HcclResult BatchRecv(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue, const RankSliceInfo &sliceInfoVec, BufferType memType, u32 memOffset) const;
    HcclResult BatchSR(AicpuNHRStepInfo &stepInfo, const ResLinks &tempLinks, InsQuePtr &queue, const RankSliceInfo &sliceInfoVec, BufferType memType, u32 memOffset) const;

    HcclResult GetGatherStepInfo(std::vector<AicpuNHRStepInfo> &nhrSteps) const;

    // 主要搬运所用的Buffer
    BufferType mainBufferType_;
    u64 mainBufferBaseOffset_{0};
};

} // namespace Hccl

#endif /* INS_TEMP_GATHER_NHR */
