/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation base header file
 * Create: 2025-02-18
 */

#ifndef HCOMM_CCU_REPRESENTATION_READ_H
#define HCOMM_CCU_REPRESENTATION_READ_H

#include "ccu_rep_base_v1.h"
#include "ccu_datatype_v1.h"

namespace hcomm {
namespace CcuRep {

class CcuRepRead : public CcuRepBase {

public:
    CcuRepRead(const ChannelHandle channel, LocalAddr loc, RemoteAddr rem, Variable len, CompletedEvent sem,
               uint16_t mask);
    CcuRepRead(const ChannelHandle channel, LocalAddr loc, RemoteAddr rem, Variable len, uint16_t dataType,
               uint16_t opType, CompletedEvent sem, uint16_t mask);
    bool        Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep) override;
    std::string Describe() override;

private:
    ChannelHandle channel;

    LocalAddr   loc;
    RemoteAddr   rem;
    Variable len;

    CompletedEvent sem;
    uint16_t  mask{0};

    uint16_t dataType{0};
    uint16_t opType{0};
    uint16_t reduceFlag{0};
};

}; // namespace CcuRep
}; // namespace hcomm
#endif // HCOMM_CCU_REPRESENTATION_READ_H