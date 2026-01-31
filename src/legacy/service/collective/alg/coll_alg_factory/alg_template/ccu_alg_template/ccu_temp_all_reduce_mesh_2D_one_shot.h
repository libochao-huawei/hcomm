/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllReduceMesh2DOneShot类头文件
 * Author: caichanghua
 * Create: 2025-02-12
 */

#ifndef HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_2D_ONE_SHOT_H_
#define HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_2D_ONE_SHOT_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_rank_group.h"

namespace Hccl {
class CcuTempAllReduceMesh2DOneShot : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllReduceMesh2DOneShot(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllReduceMesh2DOneShot() override;

    std::string Describe() const override
    {
        return StringFormat("Template of All Reduce ccu mesh 2D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    // init reduceInfo
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

private:
    HcclResult PrepareLinks(const ResLinks &tempLinks);
    HcclResult PrepareRankGroups();
    HcclResult GetBufferAddr(const TempFuncs &tempFuncs,
        uint64_t &inputAddr, uint64_t &outputAddr, uint64_t &scratchAddr);

    // 内部计算用到的变量
    ReduceOp reduceOp_;
    DataType dataType_;
    std::vector<uint64_t> dimSize_;
    std::vector<LinkData> linksX_;
    std::vector<LinkData> linksY_;
    RankGroup rankGroupX_;
    RankGroup rankGroupY_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_2D_ONE_SHOT_H_