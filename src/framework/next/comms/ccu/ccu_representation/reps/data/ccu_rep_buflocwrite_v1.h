/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation base header file
 * Create: 2025-02-18
 */

#ifndef HCOMM_CCU_REPRESENTATION_BUFLOCWRITE_H
#define HCOMM_CCU_REPRESENTATION_BUFLOCWRITE_H

#include "ccu_rep_base_v1.h"
#include "ccu_datatype_v1.h"

namespace hcomm {
namespace CcuRep {

class CcuRepBufLocWrite : public CcuRepBase {
public:
    CcuRepBufLocWrite(CcuBuf src, LocalAddr dst, Variable len, CompletedEvent sem, uint32_t mask);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    CcuBuf src;
    LocalAddr    dst;
    Variable  len;

    CompletedEvent sem;
    uint32_t   mask{0};
};

};     // namespace CcuRep
};     // namespace hcomm
#endif // HCOMM_CCU_REPRESENTATION_BUFLOCWRITE_H