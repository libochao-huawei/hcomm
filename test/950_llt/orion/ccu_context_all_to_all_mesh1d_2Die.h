/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CcuContextAllToAllMesh1D2Die的头文件
 * Author: cuishuang
 * Create: 2026-01-15
 */

#ifndef HCCLV2_CCU_CONTEXT_ALL_TO_ALL_MESH_1D_2DIE_H_
#define HCCLV2_CCU_CONTEXT_ALL_TO_ALL_MESH_1D_2DIE_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_assist.h"

namespace Hccl {

class CcuContextAllToAllMesh1D2Die : public CcuContext {
public:
    CcuContextAllToAllMesh1D2Die(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllToAllMesh1D2Die() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    uint16_t virRankSize{0};
    uint64_t logicRankSize{0};
    uint16_t selfBit{0};
    uint16_t allBit{0};
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    bool withMyRank_ = true;  // 发数据是否包含本rank
    std::vector<RankId> rankGroup_;
    CcuRep::Variable sliceSize_;
    CcuRep::Variable inputSliceStride_;
    CcuRep::Variable outputoffset_;
    CcuRep::Variable outBuffBaseOff_;
    GroupOpSize groupOpSize_;

    void CreateLocalCopyLoop();
    void LocalCopyByLoopGroup(CcuRep::Memory dst, CcuRep::Memory src);
    void InitResource();
    void LoadArgs();
    void PreSync();
    void PostSync();
    uint32_t CalcDstRank(uint32_t peerId) const;
    void DoRepeatAllToAll();
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_TO_ALL_MESH_1D_2DIE_H_