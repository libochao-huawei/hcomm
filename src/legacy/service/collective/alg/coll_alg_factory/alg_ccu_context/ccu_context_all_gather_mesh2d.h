/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllGatherMesh2D的头文件
 * Author: huangweihao
 * Create: 2025-03-11
 */


#ifndef HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_2D_H_
#define HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_2D_H_

#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextAllGatherMesh2D : public CcuContext {
public:
    CcuContextAllGatherMesh2D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextAllGatherMesh2D() override {}

    void Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;
private:
    void InitResources();
    void LoadArgs();
    void ExchangeInfoAndSync();
    void RankSync(uint32_t signalIndex);
    void AxisSync(uint32_t signalIndex);
    void FirstStep();
    void SecondStep();

    std::vector<uint64_t> dimSize_;
    uint32_t rankId_{0};
    uint32_t axisId_{0};  // 0 : X轴， 1 : Y轴

    std::vector<uint32_t> dimId_;  // 本rank所在行或列的编号
    uint32_t localId_{0};  // 本chip所在行或列的编号
    uint32_t localSize_{0};  // 本rank所在行或列的总rank数

    // 从外部获取的参数
    std::vector<CcuRep::Variable> input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;

    CcuRep::Variable xAxisSize_;   // 沿X轴搬移的数据量大小
    CcuRep::Variable yAxisSize_;   // 沿Y轴搬移的数据量大小
    CcuRep::Variable offset_;    // Rank偏移+loop偏移
    CcuRep::Variable sliceSize_; // aSize + bSize 应该可以去掉

    CcuRep::Variable firstInOffset_;
    CcuRep::Variable firstOutOffset_;
    CcuRep::Variable secondInOutBaseOffset_;
    CcuRep::Variable secondInOutStepOffset_;

    GroupOpSize goASize_;
    GroupOpSize goBSize_;

    // 跨轴同步信号
    std::string localAxisSignalName_;
    std::string anotherAxisSignalName_;
    CcuRep::MaskSignal localAxisSignal_;
    CcuRep::MaskSignal anotherAxisSignal_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_GATHER_MESH_2D_H_