/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法模板CcuTempAllReduceMesh1D类头文件
 * Author: fuyuqi
 * Create: 2025-04-17
 */

#ifndef HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_DETOUR_1D_H_
#define HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_DETOUR_1D_H_

#include <vector>
#include <map>
#include <string>
#include <hccl/hccl_types.h>
#include "reduce_op.h"
#include "hccl/base.h"
#include "types/types.h"
#include "string_util.h"
#include "data_type.h"
#include "template_utils.h"
#include "ccu_alg_template_base.h"
#include "ccu_instruction_all_reduce_mesh1d_detour.h"
#include "ccu_alg_template_base.h"

namespace Hccl {

class CcuTempAllReduceMeshDetour1D : public CcuAlgTemplateBase {
public:
    explicit CcuTempAllReduceMeshDetour1D(const RankId virtualRank, const u32 tempRankSize,
                                    const std::vector<std::vector<RankId>> &tempVTopo,
                                    const std::map<RankId, u32>            &tempVirtRankMap);
    ~CcuTempAllReduceMeshDetour1D() override;

    std::string Describe() const override
    {
        return StringFormat("Template of All Reduce ccu mesh 1D detour with tempRankSize [%u].", tempRankSize_);
    }

    HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec, const BuffInfo &buffInfo,
                         const ResLinks &tempLinks, std::vector<InsQuePtr> &tempInsQues) override;
    HcclResult CalcResDetour(const RankGraph *rankGraph, AlgTempResReq &tempResReq) override;
    HcclResult CalcResDetour(ConnectedLinkMgr *linkMgr, AlgTempResReq &tempResReq) override;
    HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec) override;
    // init reduceInfo
    void InitReduceInfo(const ReduceOp &reduceOp, const DataType &dataType);

private:
    void GetAddrInfo(const TempFuncs &tempFuncs, uint64_t &inputAddr, uint64_t &outputAddr);
    void CalcDetourOffset(uint64_t sliceSize, uint64_t &tailOffset, uint64_t &tailSize, uint64_t &iterNum);
    void ProcessLinks(std::vector<LinkData> &links, const ResLinks &tempLinks) const;
    ReduceOp reduceOp_;
    DataType dataType_;
    uint64_t detourPathNum_{0};  // 到每个对端有几个绕路路径
    uint64_t pathNumPerPeer_{0};
    std::vector<uint64_t> lengths_;
    uint64_t singleTransportSize_{0};
};
}
#endif // HCCLV2_CCU_TEMP_ALL_REDUCE_MESH_DETOUR_1D_H_