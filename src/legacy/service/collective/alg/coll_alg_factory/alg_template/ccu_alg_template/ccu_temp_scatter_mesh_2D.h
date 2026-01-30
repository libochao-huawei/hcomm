/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempScatterMesh2D类头文件
 * Author: chenchao
 * Create: 2025-03-20
 */

#ifndef HCCLV2_CCU_TEMP_SCATTER_MESH_2D_H_
#define HCCLV2_CCU_TEMP_SCATTER_MESH_2D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_scatter_mesh2d.h"

namespace Hccl {


class CcuTempScatterMesh2D : public CcuAlgTemplateBase {
public:
    explicit CcuTempScatterMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap
                                   );
    ~CcuTempScatterMesh2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of scatter ccu mesh 2D with tempRankSize [%u].", tempRankSize_);
    }

    void SetA2ASendRecvInfo(const A2ASendRecvInfo &sendRecvInfo);
    HcclResult SetBuffBlockSize(const u64 buffBlockSize);
    HcclResult SetConcurrentSendRecvNum(const u32 concurrentSendRecvNum);
    uint64_t GetMaxSliceSize() const;
    uint64_t GetExpandedMode() const;

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
private:
    uint64_t DataSliceToAddr(const DataSlice &dataSlice);
    HcclResult PrepareLinks(const ResLinks &tempLinks);
    HcclResult PrepareRankGroups();
    A2ASendRecvInfo localSendRecvInfo_;
    u32             concurrentSendRecvNum_ = 8;
    u64 buffBlockSize_ = 0;
    BuffInfo buffInfo_;
    uint64_t sendStrideSize_ = 0;  // Bytes
    uint64_t recvStrideSize_ = 0;  // Bytes

    DataType dataType_;
    std::vector<uint64_t> dimSize_;
    std::vector<LinkData> linksX_;
    std::vector<LinkData> linksY_;
    RankGroup rankGroupX_;
    RankGroup rankGroupY_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_SCATTER_MESH_2D_H_