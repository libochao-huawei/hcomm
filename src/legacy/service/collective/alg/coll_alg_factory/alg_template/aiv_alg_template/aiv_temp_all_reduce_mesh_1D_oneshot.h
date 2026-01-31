/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板AivTempAllReduceMesh1DOneShot类头文件
 * Author: linyefeng
 * Create: 2025-09-18
 */

#ifndef AIV_TEMP_ALL_REDUCE_MESH_1D_ONESHOT
#define AIV_TEMP_ALL_REDUCE_MESH_1D_ONESHOT

#include "string_util.h"
#include "executor_utils.h"

#include "aiv_alg_template_base.h"

namespace Hccl {

class AivTempAllReduceMesh1DOneShot : public AivAlgTemplateBase {
public:
    explicit AivTempAllReduceMesh1DOneShot(const RankId virtualRank, const u32 tempRankSize,
        const std::vector<std::vector<RankId>> &tempVTopo, const std::map<RankId, u32> &tempVirtRankMap);
    ~AivTempAllReduceMesh1DOneShot() override;

    std::string Describe() const override
    {
        return StringFormat("Instruction based Template of allreduce mesh 1D oneshot with tempRankSize [%u].",
            tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, 
        const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType) override;
    HcclResult CalBlockDim(u32& blockDim, u64 dataSize, u32 blockDimLimit) override;
};

}  // namespace Hccl

#endif  // AIV_TEMP_ALL_REDUCE_MESH_1D_ONESHOT