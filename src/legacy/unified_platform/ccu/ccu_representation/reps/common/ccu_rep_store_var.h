/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ccu representation load header file
 * Create: 2026-01-12
 */

#ifndef HCCL_CCU_REPRESENTATION_STORE_VAR_H
#define HCCL_CCU_REPRESENTATION_STORE_VAR_H

#include "ccu_rep_base.h"
#include "ccu_datatype.h"

namespace Hccl {
namespace CcuRep {

class CcuRepStoreVar : public CcuRepBase {
public:
    CcuRepStoreVar(const Variable &src, const Variable &var);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    Variable src;
    Variable var;
    uint16_t mask{1};
};

}; // namespace CcuRep
}; // namespace Hccl
#endif // HCCL_CCU_REPRESENTATION_STORE_VAR_H