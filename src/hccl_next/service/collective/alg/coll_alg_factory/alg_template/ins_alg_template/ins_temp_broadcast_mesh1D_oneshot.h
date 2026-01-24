/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板InsTempBroadcastMesh1DOneShot类头文件
 * Author: zhangzuyu
 * Create: 2025-06-05
 */

#ifndef HCCLV2_INS_TEMP_BROADCAST_MESH_1D_ONESHOT
#define HCCLV2_INS_TEMP_BROADCAST_MESH_1D_ONESHOT

#include "string_util.h"

#include "ins_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {

class InsTempBroadcastMesh1DOneShot : public InsAlgTemplateBase {
public:
    explicit InsTempBroadcastMesh1DOneShot(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~InsTempBroadcastMesh1DOneShot() override;

    std::string Describe() const override
    {
        return StringFormat("Template of broadcast mesh with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;

    u32 CalcScratchMultiple(BufferType inBuffType, BufferType outBuffType);
private:
};

} // namespace Hccl

#endif // !HCCLV2_INS_TEMP_BROADCAST_MESH