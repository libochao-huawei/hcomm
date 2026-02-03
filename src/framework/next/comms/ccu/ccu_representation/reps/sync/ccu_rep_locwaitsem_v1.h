/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation base header file
 * Create: 2025-02-18
 */

#ifndef HCOMM_CCU_REPRESENTATION_LOCWAITSEM_H
#define HCOMM_CCU_REPRESENTATION_LOCWAITSEM_H

#include "ccu_datatype_v1.h"
#include "ccu_rep_base_v1.h"

namespace hcomm {
namespace CcuRep {

class CcuRepLocWaitSem : public CcuRepBase {
public:
    CcuRepLocWaitSem(const CompletedEvent &sem, uint16_t mask, bool isProfiling=true);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;
    uint16_t    GetSemId() const;

private:
    CompletedEvent sem;
    uint16_t   mask{0};
    bool       isProfiling{true};
};

}; // namespace CcuRep
}; // namespace hcomm
#endif // HCOMM_CCU_REPRESENTATION_LOCWAITSEM_H