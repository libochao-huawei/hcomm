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

CcuRepPostSharedVar::CcuRepPostSharedVar(const Variable &srcVar, const Variable &dstVar, const CompletedEvent &sem,
                                         uint16_t mask)
    : srcVar(srcVar), dstVar(dstVar), sem(sem), mask(mask)
{
    type       = CcuRepType::POST_SHARED_VAR;
    instrCount = 2;  // 指令数为2个
}

bool CcuRepPostSharedVar::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    if (sem.DieId() != dep.dieId) {
        SyncXnInstr(instr++, dstVar.Id(), srcVar.Id(), dep.reserveChannalId[1], sem.Id(), mask, 0, 0, 0, 0, 1);
        LoadImdToXnInstr(instr++, dep.reserveXnId, 0);
    } else {
        LoadXXInstr(instr++, dstVar.Id(), srcVar.Id(), dep.reserveXnId);
        SetCKEInstr(instr++, sem.Id(), mask, 0, 0, 1);
    }
    CHK_PRT_THROW((instrId > UINT16_MAX - instrCount),
                        HCCL_ERROR("[CcuRepPostSharedVar::Translate]uint16 integer overflow occurs, instrId = [%hu], instrCount = [%hu]", instrId, instrCount),
                          Hccl::InternalException, "integer overflow");
    instrId += instrCount;

    return translated;
}

std::string CcuRepPostSharedVar::Describe()
{
    return Hccl::StringFormat("Post Shared Variable[%u], from Variable[%u]", dstVar.Id(), srcVar.Id());
}

}; // namespace CcuRep
}; // namespace hcomm