 /*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CCUTempBroadcastMesh1D类头文件
 * Author: libiaozhi
 * Create: 2025-03-21
 */

#ifndef HCCLV2_CCU_TEMP_BROADCAST_MESH_1D_H_
#define HCCLV2_CCU_TEMP_BROADCAST_MESH_1D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_broadcast_mesh1d.h"

namespace Hccl {

class CcuTempBroadcastMesh1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempBroadcastMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                    const std::vector<std::vector<RankId>> &tempVTopo,
                                    const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempBroadcastMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Broadcast ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                            const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;

private:
    void GetInAndOutAddr(const TempFuncs &tempFuncs, uint64_t &inputAddr, uint64_t &outputAddr);
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_BROADCAST_MESH_1D_H_