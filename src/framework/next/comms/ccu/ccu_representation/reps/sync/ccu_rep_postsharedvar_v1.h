/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation base header file
 * Create: 2025-02-18
 */

#ifndef HCOMM_CCU_REPRESENTATION_POSTSHAREDVAR_H
#define HCOMM_CCU_REPRESENTATION_POSTSHAREDVAR_H

#include "ccu_datatype_v1.h"
#include "ccu_rep_base_v1.h"

namespace hcomm {
namespace CcuRep {

class CcuRepPostSharedVar : public CcuRepBase {
public:
    CcuRepPostSharedVar(const Variable &srcVar, const Variable &dstVar, const CompletedEvent &sem, uint16_t mask);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    Variable   srcVar;
    Variable   dstVar;
    CompletedEvent sem;
    uint16_t   mask;
};

};     // namespace CcuRep
};     // namespace hcomm
#endif // HCOMM_CCU_REPRESENTATION_POSTSHAREDVAR_H