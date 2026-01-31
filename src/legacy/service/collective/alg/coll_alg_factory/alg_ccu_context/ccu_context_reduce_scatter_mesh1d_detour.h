/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceScatterMeshDetour1D的头文件
 * Author: wanghaixu
 * Create: 2025-04-20
 */


#ifndef HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_DETOUR_1D_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_DETOUR_1D_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_assist.h"

namespace Hccl {

class CcuContextReduceScatterMeshDetour1D : public CcuContext {
public:
    CcuContextReduceScatterMeshDetour1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceScatterMeshDetour1D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    void CreateMultiOpReduceDetour(DataType &dataType, DataType &outputDataType, ReduceOp &opType);
    void GroupReduceDetour(std::vector<CcuRep::Memory> &src, std::vector<CcuRep::Memory> &dst,
        DataType &dataType, DataType &outputDataType, ReduceOp &opType);

    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    uint64_t singleTransportSize_{0};  // 每个loop单次传输的总数据量，通信域级别
    uint64_t detourPathNum_{0};
    uint64_t pathNumPerPeer_{0};  // 到每个rank有几个transport，包括重复的
    std::vector<std::vector<CcuTransport*>> detourTransports_;
    CcuRep::Variable tailOffset_;  // 尾块相对偏移，singleTransportSize*128*iterNum

    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp;
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable offset_;
    CcuRep::Variable iterNum_;
    CcuRep::Variable tailSize_;
    GroupOpSize groupOpSize_;
    std::vector<CcuRep::Variable> lengths_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_SCATTER_MESH_DETOUR_1D_H_