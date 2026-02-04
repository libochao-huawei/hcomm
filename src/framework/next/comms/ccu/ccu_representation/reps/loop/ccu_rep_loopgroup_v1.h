/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation base header file
 * Create: 2025-02-18
 */

#ifndef HCOMM_CCU_REPRESENTATION_LOOPGROUP_H
#define HCOMM_CCU_REPRESENTATION_LOOPGROUP_H

#include "ccu_datatype_v1.h"
#include "ccu_rep_base_v1.h"

namespace hcomm {
namespace CcuRep {

class CcuRepLoopGroup : public CcuRepBase {
public:
    explicit CcuRepLoopGroup(const Variable& parallelParam, const Variable& offsetParam);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

    std::shared_ptr<CcuRepBase> SetParallelParam(Variable var);
    std::shared_ptr<CcuRepBase> SetOffsetParam(Variable var);
    uint16_t GetStartLoopInstrId() const;

private:
    Variable parallelParam;
    Variable offsetParam;
};

}; // namespace CcuRep
}; // namespace hcomm
#endif // HCCL_CCU_REPRESENTATION_LOOPGROUP_H