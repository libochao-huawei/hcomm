/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempReduceScatterMeshMem2Mem2D类头文件
 * Author: yangjiahe
 * Create: 2025-10-16
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_2D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_2D_MEM2MEM_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_reduce_scatter_mesh2d_mem2mem.h"

namespace Hccl {

class CcuTempReduceScatterMeshMem2Mem2D : public CcuAlgTemplateBase {
public:
    explicit CcuTempReduceScatterMeshMem2Mem2D(const RankId virtualRank, const u32 tempRankSize,
                                   	       const std::vector<std::vector<RankId>> &tempVTopo,
                                   	       const std::map<RankId, u32> &tempVirtRankMap);
    ~CcuTempReduceScatterMeshMem2Mem2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Reduce Scatter ccu mesh 2D mem2mem with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                   const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    // init reduceInfo
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

private:
    ReduceOp reduceOp_;
    DataType dataType_;
    std::vector<uint64_t> dimSize_;
    std::vector<LinkData> linksX_;
    std::vector<LinkData> linksY_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_REDUCE_SCATTER_MESH_2D_MEM2MEM_H_
