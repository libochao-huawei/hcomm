/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllReduceMesh1D类头文件
 * Author: ningshuqi
 * Create: 2025-02-12
 */

#ifndef HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_1D_H_
#define HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_1D_H_
#include <vector>
#include <map>
#include <string>
#include <hccl/hccl_types.h>
#include "reduce_op.h"
#include "hccl/base.h"
#include "types/types.h"
#include "string_util.h"
#include "env_config.h"
#include "data_type.h"
#include "template_utils.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_reduce_mesh1d.h"
#include "ccu_alg_template_base.h"

namespace Hccl {


class CcuTempAllReduceMesh1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllReduceMesh1D(const RankId virtualRank, const u32 tempRankSize,
                                   const std::vector<std::vector<RankId>> &tempVTopo,
                                   const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllReduceMesh1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of All Reduce ccu mesh 1D with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    // init reduceInfo
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

private:
    HcclResult CheckCcuDataType() const;
    ReduceOp reduceOp_;
    DataType dataType_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_1D_H_