/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ccu representation implementation file
 * Author: sunzhepeng
 * Create: 2024-06-17
 */

#include "ccu_rep_v1.h"
#include <climits>

#include "string_util.h"

namespace hcomm {
namespace CcuRep {

CcuRepLocPostSem::CcuRepLocPostSem(const CompletedEvent &sem, uint16_t mask) : sem(sem), mask(mask)
{
    type       = CcuRepType::LOC_POST_SEM;
    instrCount = 1;
}

bool CcuRepLocPostSem::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    SetCKEInstr(instr++, sem.Id(), mask, 0, 0, 1);

    if (instrId > USHRT_MAX - instrCount) {
        Hccl::THROW<Hccl::InternalException>(Hccl::StringFormat("[CcuRepLocPostSem][Translate] instrId[%u] + instrCount[%u] exceeds the "
            "maximum value of unsigned short int.", instrId, instrCount));
    }
    instrId += instrCount;

    return translated;
}

std::string CcuRepLocPostSem::Describe()
{
    return Hccl::StringFormat("Set Sem[%u], mask[%04x]", sem.Id(), mask);
}

}; // namespace CcuRep
}; // namespace hcomm