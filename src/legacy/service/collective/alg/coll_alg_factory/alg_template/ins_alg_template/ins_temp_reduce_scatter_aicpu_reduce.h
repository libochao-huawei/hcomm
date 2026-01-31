/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceScatterAicpuReduce类头文件
 * Author: zhangzuyu
 * Create: 2025-09-10
 */

#ifndef HCCLV2_INS_TEMP_REDUCE_SCATTER_AICPU_REDUCE
#define HCCLV2_INS_TEMP_REDUCE_SCATTER_AICPU_REDUCE

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempReduceScatterAicpuReduce : public InsAlgTemplateBase {
public:
    explicit InsTempReduceScatterAicpuReduce(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempReduceScatterAicpuReduce() override;

    std::string Describe() const override
    {
        return StringFormat("Template of aicpu reducescatter with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const;
private:
    HcclResult RunAlltoAllMesh(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult RunAicpuLocalReduce(const TemplateDataParams &templateDataParams, std::vector<InsQuePtr> &tempInsQues);
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_REDUCE_SCATTER_AICPU_REDUCE