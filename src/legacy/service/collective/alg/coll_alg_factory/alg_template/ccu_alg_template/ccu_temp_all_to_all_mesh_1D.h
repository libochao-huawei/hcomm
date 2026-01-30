/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllToAllMesh1D类头文件
 * Author: ningshuqi
 * Create: 2025-02-12
 */

#ifndef HCCLV2_CCU_TEMP_ALL_TO_ALL_MESH_1D_H_
#define HCCLV2_CCU_TEMP_ALL_TO_ALL_MESH_1D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_to_all_mesh1d.h"
#include "instruction.h"

namespace Hccl {


class CcuTempAllToAllMesh1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllToAllMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap
                                   );
    ~CcuTempAllToAllMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of alltoall ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    void SetA2ASendRecvInfo(const A2ASendRecvInfo &sendRecvInfo);
    HcclResult SetBuffBlockSize(const u64 buffBlockSize);
    HcclResult SetConcurrentSendRecvNum(const u32 concurrentSendRecvNum);

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                   const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
private:
    uint64_t DataSliceToAddr(const DataSlice &dataSlice);
    A2ASendRecvInfo localSendRecvInfo_;
    u32             concurrentSendRecvNum_ = 8;
    u64 buffBlockSize_ = 0;
    BuffInfo buffInfo_;
    uint64_t sendStrideSize_ = 0;  // Bytes
    uint64_t recvStrideSize_ = 0;  // Bytes
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_TO_ALL_MESH_1D_H_