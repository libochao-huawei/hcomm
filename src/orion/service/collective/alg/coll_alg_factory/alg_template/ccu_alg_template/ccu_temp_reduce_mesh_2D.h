/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CCUTempReduceMesh2D类头文件
 * Author: wanghaixu
 * Create: 2025-03-18
 */

#ifndef HCCLV2_CCU_TEMP_REDUCE_MESH_2D_H_
#define HCCLV2_CCU_TEMP_REDUCE_MESH_2D_H_

#include "string_util.h"
#include "env_config.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_reduce_mesh2d.h"

namespace Hccl {

class CcuTempReduceMesh2D : public CcuAlgTemplateBase {
public:
    explicit CcuTempReduceMesh2D(const RankId virtualRank, const u32 tempRankSize,
                                    const std::vector<std::vector<RankId>> &tempVTopo,
                                    const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempReduceMesh2D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of Reduce ccu mesh 2D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                            const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

private:
    ReduceOp reduceOp_;
    DataType dataType_;
    std::vector<uint64_t> dimSize_;
    std::vector<LinkData> linksX_;
    std::vector<LinkData> linksY_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_REDUCE_MESH_2D_H_