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

CcuRepLoadArg::CcuRepLoadArg(const Variable &var, uint16_t argId) : var(var), argId(argId)
{
    type       = CcuRepType::LOAD_ARG;
    instrCount = 1;
}

uint16_t CcuRepLoadArg::GetVarId() const
{
    return var.Id();
}

bool CcuRepLoadArg::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    if (dep.isFuncBlock) {
        // Xn(var) = Xn(loadXnId) + 0
        LoadXXInstr(instr++, var.Id(), dep.loadXnId, dep.reserveXnId);
    } else {
        LoadSqeArgsToXnInstr(instr++, var.Id(), argId);
    }

    instrId += instrCount;

    return translated;
}

std::string CcuRepLoadArg::Describe()
{
    return Hccl::StringFormat("Variable[%u] = Arg[%u]", var.Id(), argId);
}

}; // namespace CcuRep
}; // namespace hcomm