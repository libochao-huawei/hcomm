/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceScatterMesh1D类头文件
 * Author: chenluyao
 * Create: 2025-06-12
 */

#ifndef OPEN_HCCL_INS_TEMP_REDUCE_SCATTER_MESH_H
#define OPEN_HCCL_INS_TEMP_REDUCE_SCATTER_MESH_H

#include "string_util.h"
#include "ins_alg_template_base.h"
#include "executor_utils.h"
namespace Hccl {

class InsTempReduceScatterMesh1D : public InsAlgTemplateBase {
public:
    explicit InsTempReduceScatterMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                     const std::vector<std::vector<RankId>> &tempVTopo,
                                     const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempReduceScatterMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of reduce scatter NHR with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &tempAlgParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult PostCopy(const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues);
    u64 CalcScratchMultiple(const BufferType &inBuffType, const BufferType &outBuffType) const;
private:
    HcclResult RunReduceScatter(const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues,
                                const TemplateDataParams &tempAlgParams);
    RankId GetRankFromMap(const u32 rankIdx);
    u64 processSize_{0};
};

} // namespace Hccl

#endif //OPEN_HCCL_INS_TEMP_REDUCE_SCATTER_MESH_H