/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation store var header file
 */

#ifndef HCOMM_CCU_REP_STORE_VAR_H
#define HCOMM_CCU_REP_STORE_VAR_H

#include "ccu_rep_base_v1.h"
#include "ccu_datatype_v1.h"

namespace hcomm {
namespace CcuRep {

class CcuRepStoreVar : public CcuRepBase {
public:
    CcuRepStoreVar(const Variable &var, const Variable &dst, uint32_t num = 1);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    Variable var;
    Variable dst;
    uint32_t num;
    uint16_t mask{1};
};

}; // namespace CcuRep
}; // namespace hcomm
#endif // HCOMM_CCU_REP_STORE_VAR_H
