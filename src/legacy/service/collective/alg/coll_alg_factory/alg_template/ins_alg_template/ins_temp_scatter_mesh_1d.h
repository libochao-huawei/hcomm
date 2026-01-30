/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法模板InsTempAllGatherMesh1D类头文件
 * Author: shenyutian
 * Create: 2024-04-30
 */

#ifndef HCCLV2_INS_TEMP_SCATTER_MESH_1D
#define HCCLV2_INS_TEMP_SCATTER_MESH_1D

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempScatterMesh1D : public InsAlgTemplateBase {
public:
    explicit InsTempScatterMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                  const std::vector<std::vector<RankId>> &tempVTopo,
                                  const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempScatterMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Instruction based Template of scatter mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(TempFuncs &tempFuncs, TemplateDataParams &tempAlgParams,
                        ResLinks &tempResLinks, std::vector<InsQuePtr> &tempInsQues);
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    uint64_t GetExpandedMode() const;

private:
    HcclResult RunMesh(TemplateDataParams &tempAlgParams,
                    ResLinks &tempResLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult PreCopy(TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues);
    HcclResult PostCopy(const TemplateDataParams &tempAlgParams, std::vector<InsQuePtr> &tempInsQues);

    u32 majorQueNum_       = 0;
    u32 queNumPerNeighbor_ = 1;
    bool enableInterRankCounterNotify_ = false;
    bool isZeroCopy_ = false;
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_SCATTER_MESH