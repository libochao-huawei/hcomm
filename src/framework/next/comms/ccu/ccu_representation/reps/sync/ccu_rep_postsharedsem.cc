/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ccu representation implementation file
 * Author: sunzhepeng
 * Create: 2024-06-17
 */

#include "ccu_rep_v1.h"

#include "string_util.h"

namespace hcomm {
namespace CcuRep {

CcuRepPostSharedSem::CcuRepPostSharedSem(const CompletedEvent &sem, uint16_t mask) : sem(sem), mask(mask)
{
    type       = CcuRepType::POST_SHARED_SEM;
    instrCount = 1;
}

bool CcuRepPostSharedSem::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    if (sem.DieId() != dep.dieId) {
        SyncCKEInstr(instr++, sem.Id(), dep.reserveCkeId, mask, dep.reserveChannalId[1], 0, 0, 0, 0, 1);
    } else {
        SetCKEInstr(instr++, sem.Id(), mask, 0, 0, 1);
    }
    CHK_PRT_THROW((instrId > UINT16_MAX - instrCount),
                        HCCL_ERROR("[CcuRepPostSharedSem::Translate]uint16 integer overflow occurs, instrId = [%hu], instrCount = [%hu]", instrId, instrCount),
                          Hccl::InternalException, "integer overflow");
    instrId += instrCount;

    return translated;
}

std::string CcuRepPostSharedSem::Describe()
{
    return Hccl::StringFormat("Post, Use semIndex[%u] and mask[%04x]", sem.Id(), mask);
}

}; // namespace CcuRep
}; // namespace hcomm