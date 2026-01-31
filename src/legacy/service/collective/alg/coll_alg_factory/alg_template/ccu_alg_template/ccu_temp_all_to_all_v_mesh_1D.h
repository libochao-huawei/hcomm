/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllToAllVMesh1D类头文件
 * Author: zhanlijuan
 * Create: 2025-03-17
 */

#ifndef HCCLV2_CCU_TEMP_ALL_TO_ALL_V_MESH_1D_H_
#define HCCLV2_CCU_TEMP_ALL_TO_ALL_V_MESH_1D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_to_all_v_mesh1d.h"

namespace Hccl {


class CcuTempAlltoAllVMesh1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAlltoAllVMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap
                                   );
    ~CcuTempAlltoAllVMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of alltoallv ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult GetScratchBufferInfo(const uint64_t scratchBufferSize, DataType dataType) override;
    void SetA2ASendRecvInfo(const A2ASendRecvInfo &sendRecvInfo);
    HcclResult SetBuffBlockSize(const u64 buffBlockSize);
    HcclResult SetConcurrentSendRecvNum(const u32 concurrentSendRecvNum);

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                   const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
private:
    uint64_t CalcSendRecvNumSubStep();
    A2ASendRecvInfo localSendRecvInfo_;
    u32             concurrentSendRecvNum_ = 8;
    u64 buffBlockSize_ = 0;
    BuffInfo buffInfo_;
    std::unordered_map<u32, uint64_t> sendNumSubStep_; // 需要向对应对端rank发几次数据
    std::unordered_map<u32, uint64_t> recvNumSubStep_; // 需要从对应对端rank收几次数据
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_TO_ALL_V_MESH_1D_H_