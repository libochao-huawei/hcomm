/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllReduceNHRMem2Mem1D类头文件
 * Author: zhongyu
 * Create: 2025-10-16
 */
#ifndef HCCLV2_CCU_TEMP_ALL_REDUCE_NHR_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_ALL_REDUCE_NHR_1D_MEM2MEM_H_
#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_reduce_nhr1d_mem2mem.h"
#include "executor_utils.h"


namespace Hccl {
class CcuTempAllReduceNHRMem2Mem1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllReduceNHRMem2Mem1D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllReduceNHRMem2Mem1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of AllReduce ccu nhr 1D mem2mem with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, TemplateDataParams &tempAlgParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t GetMaxSliceSize() const;
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

private:
    HcclResult CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec);
    HcclResult GetStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo);
    HcclResult GetReduceScatterStepInfo(u32 step, NHRStepInfo &stepInfo);
    HcclResult GetAllGatherStepInfo(u32 step, u32 nSteps, NHRStepInfo &stepInfo);
    HcclResult ProcessNHRStepInfo(std::vector<NHRStepInfo> &stepInfoVector, RankGroup &rankGroup,
                                std::map<u32, u32> &indexMap, std::vector<LinkData> &linksDie0,
                                std::vector<LinkData> &linksDie1, const ResLinks &tempLinks);
    HcclResult SplitDataFor2Dies(uint64_t dataCount, const ResLinks &tempLinks,
                                uint64_t &die0Size, uint64_t &die1Size) const;
    uint32_t virtRankId2RankId(const u32 virtRankId);
    ReduceOp reduceOp_;
    DataType dataType_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_1D_MEM2MEM_H_