/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ccu representation implementation file
 * Author: sunzhepeng
 * Create: 2024-06-17
 */

#include "ccu_rep_v1.h"

#include "string_util.h"
#include "exception_util.h"
#include "ccu_api_exception.h"

namespace hcomm {
namespace CcuRep {

CcuRepLoop::CcuRepLoop(const std::string &label, const Variable &loopParam) : label(label), loopParam(loopParam)
{
    type       = CcuRepType::LOOP;
    instrCount = 1; // loop翻译需要1条指令
}

const std::string &CcuRepLoop::GetLabel() const
{
    return label;
}

void CcuRepLoop::Reference(std::shared_ptr<CcuRepLoopBlock> refRep)
{
    loopBlock = refRep;
}

std::shared_ptr<CcuRepBase> CcuRepLoop::SetLoopParam(Executor executor, Variable var)
{
    return std::make_shared<CcuRepSetLoop>(loopParam, executor, var);
}

bool CcuRepLoop::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    if (!loopBlock->Translated()) {
        Hccl::THROW<Hccl::CcuApiException>("Reference To Invalid LoopBlock");
    }

    LoopInstr(instr++, loopBlock->StartInstrId(), loopBlock->StartInstrId() + loopBlock->InstrCount() - 1,
              loopParam.Id());

    instrId += instrCount;

    return translated;
}

std::string CcuRepLoop::Describe()
{
    return Hccl::StringFormat("Loop reference to [%s]", label.c_str());
}

}; // namespace CcuRep
}; // namespace hcomm