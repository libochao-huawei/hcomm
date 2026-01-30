/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempAllReduceAicpuReduceMesh2D类头文件
 * Author: zhangzuyu
 * Create: 2025-10-16
 */

#ifndef HCCLV2_INS_TEMP_ALL_REDUCE_AICPU_REDUCE_MESH_2D
#define HCCLV2_INS_TEMP_ALL_REDUCE_AICPU_REDUCE_MESH_2D

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempAllReduceAicpuReduceMesh2D : public InsAlgTemplateBase {
public:
    explicit InsTempAllReduceAicpuReduceMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempAllReduceAicpuReduceMesh2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of aicpu reduce allreduce 2D Mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const;
private:
    HcclResult RunAicpuLocalReduce(const TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_ALL_REDUCE_AICPU_REDUCE_MESH_2D