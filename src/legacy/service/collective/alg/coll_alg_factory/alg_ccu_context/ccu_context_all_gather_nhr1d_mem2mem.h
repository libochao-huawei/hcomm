/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllGatherNHR1D的头文件
 * Author: ningshuqi
 * Create: 2025-07-11
 */

#ifndef CUPT_KERNEL_CCU_CONTEXT_ALLGATHER_NHR_1D_H_
#define CUPT_KERNEL_CCU_CONTEXT_ALLGATHER_NHR_1D_H_
#include <vector>

#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_instruction_all_gather_nhr1d_mem2mem.h"

namespace Hccl {

class CcuContextAllGatherNHR1D : public CcuContext {
public:
    CcuContextAllGatherNHR1D(const CcuCtxArg &arg, const std::vector<CcuTransport *> &transports,
                             const CcuTransportGroup &group);
    ~CcuContextAllGatherNHR1D() override
    {
    }

    void                  Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void LoadArgs();
    void InitResources();
    void PreSync();
    void PostSync();
    void AxisSync(uint32_t signalIndex);
    void DoRepeatAllGatherNHR();
    void DoRepeatAllGatherNHRSingleStep(const NHRStepInfo                   &nhrStepInfo);
    void DoRepeatSendRecvSlices(const u32 &toRank, CcuRep::Memory &src, CcuRep::Memory &dst, u32 signalIndex);

    // 构造函数中
    uint32_t rankId_{0};
    uint64_t dimSize_{0};
    uint32_t axisId_{0};
    uint32_t axisSize_{0};
    uint32_t localSize_{0}; // 本rank所在行或列的总rank数
    uint32_t myRankIdx_{0};
    uint32_t signalNum_{0}; // 需要使用的signal数量

    std::vector<NHRStepInfo> stepInfoVector_; // nhr算法执行过程中的参数
    std::map<u32, u32>       indexMap_;

    // load进来参数
    CcuRep::Variable              input_;
    std::vector<CcuRep::Variable> output_;
    std::vector<CcuRep::Variable> token_;
    CcuRep::Variable              die0Size_;
    CcuRep::Variable              die1Size_;
    CcuRep::Variable              inputSliceStride_;
    CcuRep::Variable              outputSliceStride_;
    CcuRep::Variable              inputRepeatStride_;
    CcuRep::Variable              outputRepeatStride_;
    CcuRep::Variable              repeatNum_;
    CcuRep::Variable              isInputOutputEqual_;
    CcuRep::Variable              myrankInputSliceOffset_;
    CcuRep::Variable              tmpSliceOffset_;
    std::vector<CcuRep::Variable> outputSliceOffset_;
    CcuRep::Variable              tmpRepeatNum_;
    CcuRep::Variable              tmpCopyRepeatNum_;
    CcuRep::Variable              constVar1_;
    CcuRep::Variable              ckeBitCounter_;
    CcuRep::Variable              repeatTimeflag_;

    // 跨轴同步信号
    std::string        localAxisSignalName_;
    std::string        anotherAxisSignalName_;
    CcuRep::MaskSignal localAxisSignal_;
    CcuRep::MaskSignal anotherAxisSignal_;
    CcuRep::MaskSignal localSignal_;

    CcuRep::Memory srcMem_;
    CcuRep::Memory dstMem_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_ALL_GATHER_NHR_1D_H_