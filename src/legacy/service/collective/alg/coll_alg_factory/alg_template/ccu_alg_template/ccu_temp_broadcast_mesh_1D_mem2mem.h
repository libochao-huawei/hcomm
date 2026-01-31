 /*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CCUTempBroadcastMesh1D类头文件
 * Author: hanyan
 * Create: 2025-11-07
 */

#ifndef HCCLV2_CCU_TEMP_BROADCAST_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_BROADCAST_MESH_1D_MEM2MEM_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "executor_utils.h"
#include "ccu_instruction_broadcast_mesh1d_mem2mem.h"

namespace Hccl {
class CcuTempBroadcastMesh1DMem2Mem : public CcuAlgTemplateBase {
public:
    explicit CcuTempBroadcastMesh1DMem2Mem(const RankId virtualRank, const u32 tempRankSize,
                                                     const std::vector<std::vector<RankId>> &tempVTopo,
                                                     const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempBroadcastMesh1DMem2Mem() override;

    std::string Describe() const override
    {
        return StringFormat("Template of broadcast ccu mesh 1D Mem2Mem, tempRankSize [%u].", tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    uint64_t   GetMaxSliceSize() const;
    HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo, const std::vector<InsQuePtr> &tempInsQues) const;
};
} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_BROADCAST_MESH_1D_MEM2MEM_H_