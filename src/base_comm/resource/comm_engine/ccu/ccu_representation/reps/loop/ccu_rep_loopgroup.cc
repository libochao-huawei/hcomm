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

CcuRepLoopGroup::CcuRepLoopGroup(const Variable& parallelParam, const Variable& offsetParam) : parallelParam(parallelParam), offsetParam(offsetParam)
{
    type       = CcuRepType::LOOPGROUP;
    instrCount = 1;
}

bool CcuRepLoopGroup::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    this->instrId = instrId;
    translated    = true;

    // 这是个非常危险的操作，需要谨慎使用
    // 依赖于LoopGroupCall的实现
    // LoopGroup所指向的Loop的位置为当前指令Id + 3
    LoopGroupInstr(instr++, instrId + 3, parallelParam.Id(), offsetParam.Id(), 0);

    instrId += instrCount;

    return translated;
}

std::string CcuRepLoopGroup::Describe()
{
    return Hccl::StringFormat("LoopGroup");
}

std::shared_ptr<CcuRepBase> CcuRepLoopGroup::SetParallelParam(Variable var)
{
    return std::make_shared<CcuRepAssign>(parallelParam, var);
}

std::shared_ptr<CcuRepBase> CcuRepLoopGroup::SetOffsetParam(Variable var)
{
    return std::make_shared<CcuRepAssign>(offsetParam, var);
}

uint16_t CcuRepLoopGroup::GetStartLoopInstrId() const
{
    // 这是个非常危险的操作，需要谨慎使用
    // 依赖于LoopGroupCall的实现
    // LoopGroup所指向的Loop的位置为当前指令Id + 3
    return this->instrId + 3;
}

}; // namespace CcuRep
}; // namespace hcomm