/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextReduceMesh2D的头文件
 * Author: wanghaixu
 * Create: 2025-03-18
 */

#ifndef HCCLV2_CCU_CONTEXT_REDUCE_MESH_2D_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_MESH_2D_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextReduceMesh2D : public CcuContext {
public:
    CcuContextReduceMesh2D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceMesh2D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void InitResources();
    void LoadArgs();
    void PreSync();
    void PostSync(uint32_t signalIndex);
    void AxisSync(uint32_t signalIndex);
    void Step1Reduce();
    void Step2ReduceForRoot();

    std::vector<uint64_t> dimSize_;
    uint32_t axisId_{0};  // 0 : X轴， 1 : Y轴

    std::vector<uint32_t> dimId_;  // 本rank所在行或列的编号
    uint32_t localId_{0};  // 本chip所在行或列的编号
    uint32_t localSize_{0};  // 本rank所在行或列的总rank数

    uint64_t rankSize{0};
    uint32_t rankId_{0};
    std::vector<uint32_t> rootDimId_;  // root所在行或列的编号
    uint32_t rootId_{0}; // 当rankid == rootid时，为root节点 则跳过write操作
    uint32_t rootLocalId_{0};  // root所在行或列的编号
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable offSet_;
    GroupOpSize xAxisGroupOpSize_;
    GroupOpSize yAxisGroupOpSize_;

    // 跨轴同步信号
    std::string localAxisSignalName_;
    std::string anotherAxisSignalName_;
    CcuRep::MaskSignal localAxisSignal_;
    CcuRep::MaskSignal anotherAxisSignal_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_MESH_2D_H_
