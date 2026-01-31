/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllReduceMesh1D的头文件
 * Author: ningshuqi
 * Create: 2025-02-19
 */

#ifndef HCCLV2_CCU_CONTEXT_REDUCE_TAIL_BLOCK_H_
#define HCCLV2_CCU_CONTEXT_REDUCE_TAIL_BLOCK_H_

#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"

namespace Hccl {

class CcuContextReduceTailBlock : public CcuContext {
public:
    CcuContextReduceTailBlock(const CcuCtxArg &arg, const std::vector<CcuTransport *> &transports,
                              const CcuTransportGroup &group);
    ~CcuContextReduceTailBlock() override {}

    void                  Algorithm() override;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override;

private:
    void ExportVariables();
    void InitResource();
    void DoGroupReduce();
    void SyncMainBlock(uint32_t ctxSignalIndex);

    std::string notifySignal_ = "empty_signal";

    uint64_t rankSize_{0};
    uint32_t rankId_{0};
    DataType dataType_;
    DataType outputDataType_;
    ReduceOp reduceOp_;

    std::vector<CcuRep::Variable> token_;
    std::vector<CcuRep::Variable> input_;
    CcuRep::Variable              output_;

    CcuRep::Variable inputOffset_;
    CcuRep::Variable outputOffset_;

    GroupOpSize groupOpSize_;

    CcuRep::MaskSignal mainBlockCtxSignal_;
    CcuRep::MaskSignal tailBlockCtxSignal_;
};
} // namespace Hccl

#endif // HCCLV2_CCU_CONTEXT_REDUCE_TAIL_BLOCK_H_