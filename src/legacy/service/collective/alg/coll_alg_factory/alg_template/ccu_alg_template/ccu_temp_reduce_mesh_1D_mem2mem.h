/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CCUTempReduceMeshMem2Mem1D类头文件
 * Author: jinliuyi
 * Create: 2025-12-3
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_REDUCE_MESH_1D_MEM2MEM_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_reduce_mesh1d_mem2mem.h"
#include "executor_utils.h"
namespace Hccl {

class CcuTempReduceMeshMem2Mem1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempReduceMeshMem2Mem1D(const RankId virtualRank, const u32 tempRankSize,
                                        const std::vector<std::vector<RankId>> &tempVTopo,
                                        const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempReduceMeshMem2Mem1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Reduce ccu mesh 1D mem2mem with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                   std::vector<InsQuePtr> &tempInsQues);
    void       InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);
    HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo,
                         const std::vector<InsQuePtr> &tempInsQues) const;

private:
    ReduceOp reduceOp_;
    DataType dataType_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_REDUCE_MESH_1D_MEM2MEM_H_