/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceScatterAicpuReduceMesh2D类头文件
 * Author: zhangzuyu
 * Create: 2025-10-16
 */

#ifndef HCCLV2_INS_TEMP_REDUCE_SCATTER_AICPU_REDUCE_MESH_2D
#define HCCLV2_INS_TEMP_REDUCE_SCATTER_AICPU_REDUCE_MESH_2D

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"
#include "ins_temp_all_to_all_mesh_2D.h"

namespace Hccl {

class InsTempReduceScatterAicpuReduceMesh2D : public InsAlgTemplateBase {
public:
    explicit InsTempReduceScatterAicpuReduceMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempReduceScatterAicpuReduceMesh2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of aicpu reduce reducescatter 2D Mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const;
private:
    HcclResult RunAicpuLocalReduce(const TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
    InsTempAlltoAllMesh2D alltoallMesh2D_;
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_REDUCE_SCATTER_AICPU_REDUCE_MESH_2D