/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: 算法模板CcuTempAllToAllMesh1D2Die类头文件
 * Author: cuishaung
 * Create: 2026-01-13
 */

#ifndef HCCLV2_CCU_TEMP_ALL_TO_ALL_MESH_1D_2DIE_H_
#define HCCLV2_CCU_TEMP_ALL_TO_ALL_MESH_1D_2DIE_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_to_all_mesh1d_2Die.h"
#include "executor_utils.h"

namespace Hccl {

class CcuTempAllToAllMesh1D2Die : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllToAllMesh1D2Die(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllToAllMesh1D2Die() override;

   std::string Describe() const override
    {
        return StringFormat("Template of alltoall2Die ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams, const ResLinks &tempLinks,
                   std::vector<InsQuePtr> &tempInsQues);
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult SetBuffBlockSize(const u64 buffBlockSize);
    HcclResult SetConcurrentSendRecvNum(const u32 concurrentSendRecvNum);
private:
    u32             concurrentSendRecvNum_ = 8;
    u64 buffBlockSize_ = 0;
};

} // namespace Hccl
#endif // HCCLV2_CCU_TEMP_ALL_TO_ALL_MESH_1D_2DIE_H_