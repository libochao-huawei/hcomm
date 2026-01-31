/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextScatterNHR1DMem2Mem的头文件
 * Author: hanyan
 * Create: 2025-11-20
 */

#ifndef CUPT_KERNEL_CCU_CONTEXT_SCATTER_NHR_1D_H_
#define CUPT_KERNEL_CCU_CONTEXT_SCATTER_NHR_1D_H_
#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_instruction_scatter_nhr1d_mem2mem.h"

namespace Hccl {
class CcuContextScatterNHR1DMem2Mem : public CcuContext {
public:
    CcuContextScatterNHR1DMem2Mem(const CcuCtxArg &arg, const std::vector<CcuTransport *> &transports,
                                  const CcuTransportGroup &group);
    ~CcuContextScatterNHR1DMem2Mem() override
    {
    }

    void                  Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void LoadArgs();
    void InitResources();
    void PreSync();
    void AxisSync(uint32_t signalIndex);
    void DoScatterNHR();
    void DoScatterNHRSingleStep(const NHRStepInfo &nhrStepInfo);
    void DoSendRecvSlice(const u32 &toRank, CcuRep::Memory &src, CcuRep::Memory &dst, u32 signalIndex);

    // 构造函数中
    uint32_t rankId_{0};
    uint32_t rootId_{0};
    uint64_t dimSize_{0};
    uint32_t axisId_{0};
    uint32_t axisSize_{0};
    uint32_t localSize_{0}; // 本rank所在行或列的总rank数
    uint32_t myRankIdx_{0};
    uint32_t signalNum_{0}; // 需要使用的signal数量
    uint32_t repeatNum_{0};
    DataType dataType_;
    // DataType outputDataType_;
    std::vector<NHRStepInfo> stepInfoVector_; // nhr算法执行过程中的参数
    std::map<u32, u32>       indexMap_;

    // load进来参数
    CcuRep::Variable              input_;
    CcuRep::Variable              output_;
    std::vector<CcuRep::Variable> scratch_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable              die0Size_;
    CcuRep::Variable              die1Size_;
    CcuRep::Variable              inputSliceStride_;
    CcuRep::Variable              curScratchStride_;
    CcuRep::Variable              inputRepeatStride_;
    CcuRep::Variable              outputRepeatStride_;
    CcuRep::Variable              repeatNumVar_;
    CcuRep::Variable              isOutputScratch_;

    // 跨轴同步信号
    std::string        localAxisSignalName_;
    std::string        anotherAxisSignalName_;
    CcuRep::MaskSignal localAxisSignal_;
    CcuRep::MaskSignal anotherAxisSignal_;
    CcuRep::MaskSignal localSignal_;

    CcuRep::Variable              repeatNumVarTemp_;
    CcuRep::Variable              repeatTimeflag_;
    std::vector<CcuRep::Variable> inputOffset_;
    std::vector<CcuRep::Variable> ScratchOffset_;
    CcuRep::Variable              curInputOffset_;
    CcuRep::Variable              curScratchOffset_;
    CcuRep::Variable              cursliceSize_;
    CcuRep::Memory                srcMem_;
    CcuRep::Memory                dstMem_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_SCATTER_NHR_1D_H_