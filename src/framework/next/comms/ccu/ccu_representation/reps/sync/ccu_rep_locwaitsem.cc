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

CcuRepLocWaitSem::CcuRepLocWaitSem(const CompletedEvent &sem, uint16_t mask, bool isProfiling) : sem(sem), mask(mask), isProfiling(isProfiling)
{
    type       = CcuRepType::LOC_WAIT_SEM;
    instrCount = 1;
}

uint16_t CcuRepLocWaitSem::GetSemId() const
{
    return sem.Id();
}

bool CcuRepLocWaitSem::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    // 需要profiling的使用SetCKEInstr, 否则使用ClearCKEInstr
    if (isProfiling) {
        SetCKEInstr(instr++, 0, 0, sem.Id(), mask, 1);
    } else {
        ClearCKEInstr(instr++, 0, 0, sem.Id(), mask, 1);
    }

    if (instrId > USHRT_MAX - instrCount) {
        Hccl::THROW<Hccl::InternalException>(Hccl::StringFormat("[CcuRepLocWaitSem][Translate] instrId[%u] + instrCount[%u] exceeds the "
            "maximum value of unsigned short int.", instrId, instrCount));
    }
    CHK_PRT_THROW((instrId > UINT16_MAX - instrCount),
                        HCCL_ERROR("[CcuRepLocWaitSem::Translate]uint16 integer overflow occurs, instrId = [%hu], instrCount = [%hu]", instrId, instrCount),
                          Hccl::InternalException, "integer overflow");
    instrId += instrCount;

    return translated;
}

std::string CcuRepLocWaitSem::Describe()
{
    return Hccl::StringFormat("Wait Sem[%u], mask[%04x]", sem.Id(), mask);
}

}; // namespace CcuRep
}; // namespace hcomm