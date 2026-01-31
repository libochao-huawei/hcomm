/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempReduceScatterNHR1D类头文件
 * Author: yuantangzhi
 * Create: 2025-10-14
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_SCATTER_NHR_1D_H_
#define HCCLV2_CCU_TEMP_REDUCE_SCATTER_NHR_1D_H_
#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_reduce_scatter_nhr1d_mem2mem.h"
#include "executor_utils.h"

namespace Hccl {
class CcuTempReduceScatterNHR1DMem2Mem : public CcuAlgTemplateBase {
public:
    explicit CcuTempReduceScatterNHR1DMem2Mem(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempReduceScatterNHR1DMem2Mem() override;

    std::string Describe() const override
    {
        return StringFormat("Template of ReduceScatter ccu nhr 1D mem2mem with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, TemplateDataParams &tempAlgParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t GetMaxSliceSize() const;
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) override;

private:
    HcclResult GetStepInfo(u32 step, NHRStepInfo &stepInfo);
    RankId GetRankFromMap(const u32 rankIdx);
    ReduceOp reduceOp_;
    DataType dataType_;
    bool isSipportTwoDie_{false};
    u32 linkNum_{1};
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_REDUCE_SCATTER_NHR_1D_H_