/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllReduceMeshMem2Mem1D类头文件
 * Author: jinliuyi
 * Create: 2025-10-14
 */

#ifndef HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_1D_MEM2MEM_H_
#define HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_1D_MEM2MEM_H_
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
#include "buffer_type.h"
#include "ccu_alg_template_base.h"
#include "executor_utils.h"

namespace Hccl {
class CcuTempAllReduceMeshMem2Mem1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllReduceMeshMem2Mem1D(const RankId virtualRank, const u32 tempRankSize,
                                                     const std::vector<std::vector<RankId>> &tempVTopo,
                                                     const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllReduceMeshMem2Mem1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of All Reduce ccu mesh 1D mem2mem, tempRankSize [%u].",
                            tempRankSize_);
    }

    HcclResult GenExtIns(const TempFuncs &tempFuncs, const TemplateDataParams &templateDataParams,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues);
    HcclResult GenExtIns(const RankGraph *rankGraph, const TemplateInfo &tmpInfo,
                         const std::vector<InsQuePtr> &tempInsQues) const;
    HcclResult CalcRes(AlgTempResReq &tempResReq) override;
    HcclResult CalcSlice(const u64 dataSize, RankSliceInfo &sliceInfoVec);
    u32        CalcScratchMultiple(BufferType input, BufferType output) override;
    // init reduceInfo
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

private:
    ReduceOp reduceOp_;
    DataType dataType_;
};

} // namespace Hccl

#endif // HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_1D_MEM2MEM_H_