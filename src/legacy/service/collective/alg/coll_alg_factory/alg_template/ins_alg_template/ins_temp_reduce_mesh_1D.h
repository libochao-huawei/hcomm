/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempReduceMesh1D类头文件
 * Author: c00664093
 * Create: 2025-06-04
 */

#ifndef INS_TEMP_REDUCE_MESH1D
#define INS_TEMP_REDUCE_MESH1D

#include "string_util.h"
#include "executor_utils.h"
#include "ins_alg_template_base.h"

namespace Hccl {

class InsTempReduceMesh1D : public InsAlgTemplateBase {
public:
    explicit InsTempReduceMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                 const std::vector<std::vector<RankId>> &tempVTopo,
                                 const std::map<RankId, u32> &tempVirtRankMap);
    ~InsTempReduceMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of reduce Mesh1D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) const;

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &dataParams,
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);

private:
    HcclResult RunReduce(const TemplateDataParams &dataParams, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues);
    HcclResult SendData(const TemplateDataParams &dataParams, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues);
    HcclResult GatherData(const TemplateDataParams &dataParams, const ResLinks &tempLinks,
        std::vector<InsQuePtr> &tempInsQues);
    HcclResult ReduceData(const TemplateDataParams &dataParams, std::vector<InsQuePtr> &tempInsQues);

    u32 myIdx_ = INVALID_U32;  // 本rank在通信域内的索引
};

} // namespace Hccl

#endif // INS_TEMP_REDUCE_MESH1D
