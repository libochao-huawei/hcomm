/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAlltoAllMesh2D类头文件
 * Author: hulinkang
 * Create: 2025-02-12
 */

#ifndef HCCLV2_CCU_TEMP_ALLTOALL_MESH_2D_H
#define HCCLV2_CCU_TEMP_ALLTOALL_MESH_2D_H

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_to_all_mesh2d.h"

namespace Hccl {


class CcuTempAlltoAllMesh2D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAlltoAllMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAlltoAllMesh2D() override;

    std::string Describe() const override
    {
        // 在构造函数中校验tempVTopo的大小
        return StringFormat("Template of alltoall ccu mesh 2D with tempVTopo D0[%u]--D1[%u]",
            tempVTopo_[0].size(), tempVTopo_[1].size());
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                   const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    void SetA2ASendRecvInfo(const A2ASendRecvInfo &sendRecvInfo);
    HcclResult GetScratchBufferInfo(const uint64_t scratchBufferSize, DataType dataType) override;

private:
    HcclResult FillLinks(const ResLinks &tempLinks);
    HcclResult RunOneStep(uint64_t sendRecvSize, uint64_t maxTransportSize, uint32_t sendRecvTimes, uint32_t step,
        std::vector<InsQuePtr> &tempInsQues);

    A2ASendRecvInfo localSendRecvInfo_;
    std::vector<uint32_t> dimSize_;
    std::vector<LinkData> linksX_;
    std::vector<LinkData> linksY_;
    RankGroup rankGroupX_;
    RankGroup rankGroupY_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_TO_ALL_MESH_2D_H_